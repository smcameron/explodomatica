
/* 
    (C) Copyright 2011, Stephen M. Cameron.

    This file is part of explodomatica.

    explodomatica is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    explodomatica is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with explodomatica; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <malloc.h>
#include <math.h>
#include <stdlib.h>

#include <sndfile.h> /* libsndfile */

#define SAMPLERATE 44100

struct sound {
	double *data;
	int nsamples;
};

static void free_sound(struct sound *s)
{
	if (s->data)
		free(s->data);
	s->data = NULL;
	s->nsamples = 0;
}

static struct sound *alloc_sound(int nframes)
{
	struct sound *s;

	s = malloc(sizeof(*s));
	s->data = malloc(sizeof(*s->data) * nframes * 2);
	memset(s->data, 0, sizeof(*s->data) * nframes * 2);
	s->nsamples = 0;
	return s;
}

int seconds_to_frames(double seconds)
{
	return seconds * SAMPLERATE;
}

static int save_file(char *filename, struct sound *s)
{
	SNDFILE *sf;
	SF_INFO sfinfo;


	sfinfo.frames = 0;
	sfinfo.samplerate = SAMPLERATE;
	sfinfo.channels = 2;
	sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
	sfinfo.sections = 0;
	sfinfo.seekable = 1;

	sf = sf_open(filename, SFM_WRITE, &sfinfo);
	if (!sf) {
		fprintf(stderr, "Cannot open '%s'\n", filename);
		return -1;
	}
	sf_write_double(sf, s->data, s->nsamples);
	sf_close(sf);
	return 0;
}

static struct sound *make_sinewave(int nframes, double frequency)
{
	int i;
	double theta = 0.0;
	double delta = frequency * 2.0 * 3.1415927 / 44100.0;
	struct sound *s;

	s = alloc_sound(nframes);

	for (i = 0; i < nframes * 2; i++) {
		s->data[i] = sin(theta) * 0.5;
		theta += delta;
		s->nsamples++;
	} 
	return s;
}

static struct sound *add_sound(struct sound *s1, struct sound *s2)
{
	int i, n;
	struct sound *s;

	n = s1->nsamples;
	if (s2->nsamples > n)
		n = s2->nsamples;

	s = malloc(sizeof(*s));
	s->data = malloc(sizeof(*s->data) * n);
	s->nsamples = 0;

	for (i = 0; i < n; i++) {
		s->data[i] = 0.0;
		if (i < s1->nsamples)
			s->data[i] += s1->data[i];
		if (i < s2->nsamples)
			s->data[i] += s2->data[i];
		s->nsamples++;
	}
	return s;
}

static void accumulate_sound(struct sound *acc, struct sound *inc)
{
	struct sound *t;

	t = add_sound(acc, inc);
	free_sound(acc);
	acc->data = t->data;
	acc->nsamples = t->nsamples;
}

static struct sound *make_noise(int nframes)
{
	int i;
	struct sound *s;

	s = alloc_sound(nframes);
	for (i = 0; i < nframes * 2; i++) {
		s->data[i] = (1.0 * (double) rand() / (double) RAND_MAX) - 1.0;
		s->nsamples++;
	}
	return s;
}

static void fadeout(struct sound *s, int nsamples)
{
	int i;
	double factor;

	for (i = 0; i < nsamples; i++) {
		factor = 1.0 - ((double) i / (double) nsamples);
		s->data[i] *= factor;	
	}
}	

/* algorithm for low pass filter gleaned from wikipedia
 * and adapted for stereo samples
 */
static struct sound *low_pass(struct sound *s, double RC)
{
	int i;
	struct sound *o;
	double dt = (1.0 / (double) SAMPLERATE);
	double alpha = dt / (RC + dt);

	o = malloc(sizeof(*o));
	o->data = malloc(sizeof(*o->data) * s->nsamples);

	o->data[0] = s->data[0];
	o->data[1] = s->data[1];

	for (i = 2; i < s->nsamples;) {
		o->data[i] = o->data[i - 2] + alpha * (s->data[i] - o->data[i - 2]);
		i++;
		o->data[i] = o->data[i - 2] + alpha * (s->data[i] - o->data[i - 2]);
		i++;
	}
	o->nsamples = s->nsamples;
	return o;
}

static double interpolate(double x, double x1, double y1, double x2, double y2)
{
        /* return corresponding y on line x1,y1,x2,y2 for value x
	 * (y2 -y1)/(x2 - x1) = (y - y1) / (x - x1)     by similar triangles.
	 * (x -x1) * (y2 -y1)/(x2 -x1) = y - y1         a little algebra...
	 * y = (x - x1) * (y2 - y1) / (x2 -x1) + y1;
	 */
	if (abs(x2 - x1) < (0.01 * 1.0 / SAMPLERATE))
		return y1;
	return (x - x1) * (y2 - y1) / (x2 -x1) + y1;
}

static struct sound *change_speed(struct sound *s, double factor)
{
	struct sound *o;
	int i, nframes;
	double sample_point;
	int sp1, sp2;

	nframes = (int) ((s->nsamples / 2) / factor);
	o = alloc_sound(nframes);

	o->data[0] = s->data[0];
	o->data[1] = s->data[1];
	o->nsamples = 2;

	for (i = 2; i < nframes;) {
		sample_point = (double) i / (double) nframes * (double) s->nsamples;
		sp1 = (int) sample_point;
		if ((sp1 % 2) != 0)
			sp1--;
		sp2 = sp1 + 2;
		o->data[i] = interpolate(sample_point, (double) sp1, s->data[sp1], 
						(double) sp2, s->data[sp2]);
		o->nsamples++;
		i++;
		sample_point = (double) i / (double) nframes * (double) s->nsamples;
		sp1 = (int) sample_point;
		if ((sp1 % 2) != 1)
			sp1--;
		sp2 = sp1 + 2;
		o->data[i] = interpolate(sample_point, (double) sp1, s->data[sp1], 
						(double) sp2, s->data[sp2]);
		i++;
		o->nsamples++;
	}
	return o;
}

static struct sound *copy_sound(struct sound *s)
{
	struct sound *o;
	int i, nframes;

	nframes = (int) (s->nsamples / 2);
	o = alloc_sound(nframes);

	for (i = 0; i < s->nsamples; i++)
		o->data[i] = s->data[i];
	o->nsamples = s->nsamples;
	return o;
}

static void renormalize(struct sound *s)
{
	int i;
	double max = 0;

	for (i = 0; i < s->nsamples; i++)
		if (abs(s->data[i]) > max)
			max = abs(s->data[i]);
	for (i = 0; i < s->nsamples; i++)
		s->data[i] = s->data[i] / max;
}

static struct sound *make_explosion(double seconds, int nlayers)
{
	struct sound *s[10];
	struct sound *t, *t2;
	double RC;
	double rc[] = { 25.0, 19.0, 10.0, 3.0};
	int i;

	if (nlayers > 4)
		nlayers = 4;
	if (nlayers < 1)
		nlayers = 1;

	for (i = 0; i < nlayers; i++) {
		double speedfactor;
		t = make_noise(seconds_to_frames(seconds));
		fadeout(t, t->nsamples);
		t2 = low_pass(t, rc[i] / (double) SAMPLERATE);
		speedfactor = (double) i + 1.0;
		s[i] = change_speed(t2, speedfactor);
		free_sound(t);
		free_sound(t2);
	}

	for (i = 1; i < nlayers; i++) {
		accumulate_sound(s[0], s[i]);
		renormalize(s[0]);
		free_sound(s[i]);
	}

	return s[0];
}

int main(int argc, char *argv[])
{
	struct sound *s, *s2, *s3, *s4, *s5;

#if 0
	s = make_sinewave(seconds_to_frames(2), 120); 
	s = make_noise(seconds_to_frames(2));
	s2 = make_sinewave(seconds_to_frames(10), 261);
	s4 = make_sinewave(seconds_to_frames(10), 120);
	s3 = add_sound(s2, s4);	
	fadeout(s3, s3->nsamples);
	fadeout(s, s->nsamples);
	fadeout(s, s->nsamples);
	s5 = low_pass(s, 12.0 / SAMPLERATE); 
	s2 = change_speed(s, 2.0);
#endif
	s = make_explosion(5.0, 4);
	save_file(argv[1], s);
	free_sound(s);

	return 0;
}

