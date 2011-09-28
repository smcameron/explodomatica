
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
#include <sys/time.h>

#include <sndfile.h> /* libsndfile */

#define SAMPLERATE 44100
#define ARRAYSIZE(x) (sizeof(x) / sizeof((x)[0]))

struct sound {
	double *data;
	int nsamples;
};

void usage(void)
{
	fprintf(stderr, "usage:\n");
	fprintf(stderr, "explodomatica somefile.wav\n");
	fprintf(stderr, "caution: somefile.wav will be overwritten.\n");
	exit(1);
}

static void free_sound(struct sound *s)
{
	if (s->data)
		free(s->data);
	s->data = NULL;
	s->nsamples = 0;
}

static struct sound *alloc_sound(int nsamples)
{
	struct sound *s;

	s = malloc(sizeof(*s));
	s->data = malloc(sizeof(*s->data) * nsamples);
	memset(s->data, 0, sizeof(*s->data) * nsamples);
	s->nsamples = 0;
	return s;
}

int seconds_to_frames(double seconds)
{
	return seconds * SAMPLERATE;
}

static int save_file(char *filename, struct sound *s, int channels)
{
	SNDFILE *sf;
	SF_INFO sfinfo;

	sfinfo.frames = 0;
	sfinfo.samplerate = SAMPLERATE;
	sfinfo.channels = channels;
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

static struct sound *make_sinewave(int nsamples, double frequency)
{
	int i;
	double theta = 0.0;
	double delta = frequency * 2.0 * 3.1415927 / 44100.0;
	struct sound *s;

	s = alloc_sound(nsamples);

	for (i = 0; i < nsamples; i++) {
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

static struct sound *make_noise(int nsamples)
{
	int i;
	struct sound *s;

	s = alloc_sound(nsamples);
	for (i = 0; i < nsamples; i += 2) {
		s->data[i] = (2.0 * (double) rand() / (double) RAND_MAX) - 1.0;
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
static struct sound *sliding_low_pass(struct sound *s,
	double alpha1, double alpha2)
{
	int i;
	struct sound *o;
	double alpha;

	o = malloc(sizeof(*o));
	o->data = malloc(sizeof(*o->data) * s->nsamples);

	o->data[0] = s->data[0];

	for (i = 1; i < s->nsamples;) {
		alpha = ((double) i / (double) s->nsamples) *
			(alpha2 - alpha1) + alpha1;
		alpha = alpha * alpha;
		o->data[i] = o->data[i - 1] + alpha * (s->data[i] - o->data[i - 1]);
		i++;
	}
	o->nsamples = s->nsamples;
	return o;
}

static void sliding_low_pass_inplace(struct sound *s, double alpha1, double alpha2)
{
	struct sound *o;

	o = sliding_low_pass(s, alpha1, alpha2);
	free_sound(s);
	s->data = o->data;
	s->nsamples = o->nsamples;
}

static double interpolate(double x, double x1, double y1, double x2, double y2)
{
        /* return corresponding y on line x1,y1,x2,y2 for value x
	 * (y2 -y1)/(x2 - x1) = (y - y1) / (x - x1)     by similar triangles.
	 * (x -x1) * (y2 -y1)/(x2 -x1) = y - y1         a little algebra...
	 * y = (x - x1) * (y2 - y1) / (x2 -x1) + y1;
	 */
	if (fabs(x2 - x1) < (0.01 * 1.0 / (double) SAMPLERATE))
		return (y1 + y2) / 2.0;
	return (x - x1) * (y2 - y1) / (x2 -x1) + y1;
}

static struct sound *change_speed(struct sound *s, double factor)
{
	struct sound *o;
	int i, nsamples;
	double sample_point;
	int sp1, sp2;

	nsamples = (int) (s->nsamples / factor);
	o = alloc_sound(nsamples);

	o->data[0] = s->data[0];
	o->nsamples = 1;

	for (i = 1; i < nsamples; i++) {
		sample_point = (double) i / (double) nsamples * (double) s->nsamples;
		sp1 = (int) sample_point;
		sp2 = sp1 + 1;
		o->data[i] = interpolate(sample_point, (double) sp1, s->data[sp1], 
						(double) sp2, s->data[sp2]);
		o->nsamples++;
	}
	return o;
}

static void change_speed_inplace(struct sound *s, double factor)
{
	struct sound *o;

	o = change_speed(s, factor);
	free_sound(s);
	s->data = o->data;
	s->nsamples = o->nsamples;
}

static struct sound *copy_sound(struct sound *s)
{
	struct sound *o;
	int i;

	o = alloc_sound(s->nsamples);

	for (i = 0; i < s->nsamples; i++)
		o->data[i] = s->data[i];
	o->nsamples = s->nsamples;
	return o;
}

static void renormalize(struct sound *s)
{
	int i;
	double max = 0.0;

	for (i = 0; i < s->nsamples; i++)
		if (fabs(s->data[i]) > max)
			max = fabs(s->data[i]);
	for (i = 0; i < s->nsamples; i++)
		s->data[i] = s->data[i] / (1.001 * max);
}

static void amplify_in_place(struct sound *s, double gain)
{
	int i;

	for (i = 0; i < s->nsamples; i++) {
		s->data[i] = s->data[i] * gain;
		if (s->data[i] > 1.0)
			s->data[i] = 1.0;
	}
}

static void delay_effect_in_place(struct sound *s, int delay_samples)
{
	int i, source;

	for (i = s->nsamples - 1; i >= 0; i--) {
		source = i - delay_samples;
		if (s->nsamples > source) {
			if (source > 0) {
				s->data[i] = s->data[source];
			} else {
				s->data[i] = 0.0;
			}
		}
	}
}

static struct sound *poor_mans_reverb(struct sound *s,
	int early_refls, int late_refls)
{
	int i, delay;
	struct sound *echo, *echo2;
	struct sound *withverb;
	double gain;

	printf("1\n");
	withverb = alloc_sound(s->nsamples * 2);
	for (i = 0; i < s->nsamples; i++)
		withverb->data[i] = s->data[i];
	printf("2\n");
	withverb->nsamples = s->nsamples * 2;
	echo = copy_sound(withverb);

	for (i = 0; i < early_refls; i++) {
		printf("3, i = %d\n", i);
		echo2 = sliding_low_pass(echo, 0.8, 0.8);
		gain = (double) rand() / (double) RAND_MAX * 0.05 + 0.03;
		printf("4, i = %d\n", i);
		amplify_in_place(echo, gain); 
		printf("5, i = %d\n", i);

		/* 100 ms range */
		delay = (4410 * (rand() & 0x0ffff)) / 0x0ffff;
		printf("6, i = %d\n", i);
		delay_effect_in_place(echo2, delay);
		printf("7, i = %d\n", i);
		accumulate_sound(withverb, echo2);
		printf("8, i = %d\n", i);
		free_sound(echo2);
	}

	for (i = 0; i < late_refls; i++) {
		printf("9, i = %d\n", i);
		echo2 = sliding_low_pass(echo, 0.7, 0.3);
		gain = (double) rand() / (double) RAND_MAX * 0.03 + 0.03;
		printf("10, i = %d\n", i);
		amplify_in_place(echo, gain); 
		printf("11, i = %d\n", i);

		/* 1000 ms range */
		delay = (44100 * (rand() & 0x0ffff)) / 0x0ffff;
		printf("12, i = %d\n", i);
		delay_effect_in_place(echo2, delay);
		printf("13, i = %d\n", i);
		accumulate_sound(withverb, echo2);
		printf("14, i = %d\n", i);
		free_sound(echo2);
	}
	printf("15, i = %d\n", i);
	return withverb;
}

static struct sound *make_explosion(double seconds, int nlayers)
{
	struct sound *s[10];
	struct sound *t = NULL;
	double a1, a2;
	int i, j, iters;

	for (i = 0; i < nlayers; i++) {
		t = make_noise(seconds_to_frames(seconds));

		if (i > 0) 
			change_speed_inplace(t, i * 2);

		iters = i + 1;
		if (iters > 3)
			iters = 3;
		for (j = 0; j < iters; j++)
			fadeout(t, t->nsamples);

		a1 = (double) (i + 1) / (double) nlayers;
		a2 = (double) i / (double) nlayers;

		iters = 3 - i; 
		if (iters < 0)
			iters = 1;	
		for (j = 0; j < iters; j++) {
			sliding_low_pass_inplace(t, a1, a2);
			renormalize(t);
		}
		s[i] = t;
	}

	for (i = 1; i < nlayers; i++) {
		accumulate_sound(s[0], s[i]);
		free_sound(s[i]);
	}
	renormalize(s[0]);
	return s[0];
}

static void trim_trailing_silence(struct sound *s)
{
	int i;

	for (i = s->nsamples -1 ; i >= 0; i--) {
		if (fabs(s->data[i]) < 0.00001)
			s->nsamples--;
	}
}

int main(int argc, char *argv[])
{
	struct sound *s, *s2;
	struct timeval tv;

	gettimeofday(&tv, NULL);
	srand(tv.tv_usec);

	if (argc < 2)
		usage();
	s = make_explosion(4.0, 4);
	change_speed_inplace(s, 0.25);
	trim_trailing_silence(s);
	s2 = poor_mans_reverb(s, 10, 20);
	trim_trailing_silence(s2);
	save_file(argv[1], s2, 1);
	free_sound(s);

	return 0;
}

