
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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <malloc.h>
#include <math.h>
#include <stdlib.h>
#include <sys/time.h>
#include <getopt.h>
#include <limits.h>

#include <sndfile.h> /* libsndfile */


struct explosion_def {
	char save_filename[PATH_MAX + 1];
	char input_file[PATH_MAX + 1];
	double *input_data;
	unsigned long long input_samples;
	double duration;
	int nlayers;
	int preexplosions;
	double preexplosion_delay;
	double preexplosion_low_pass_factor;
	int preexplosion_lp_iters;
	double final_speed_factor;
	int reverb_early_refls;
	int reverb_late_refls;
	int reverb; 
} e, defaults = {
	{ 0 },
	{ 0 },
	NULL,
	0LL,
	4.0,	/* duration in seconds (roughly) */
	4,	/* nlayers */
	1,	/* preexplosions */
	0.25,	/* preexplosion delay, 250ms */
	0.8,	/* preexplosion low pass factor */
	1,	/* preexplosion low pass iters */
	0.45,	/* final speed factor */
	10,	/* final reverb early reflections */
	50,	/* final reverb late reflections */
	1,	/* reverb wanted? */
};

	
#define SAMPLERATE 44100
#define ARRAYSIZE(x) (sizeof(x) / sizeof((x)[0]))

struct sound {
	double *data;
	int nsamples;
};

void usage(void)
{
	fprintf(stderr, "usage:\n");
	fprintf(stderr, "explodomatica [options] somefile.wav\n");
	fprintf(stderr, "caution: somefile.wav will be overwritten.\n");
	fprintf(stderr, "options:\n");
	fprintf(stderr, "  --duration n    Specifies duration of explosion in secs\n");
	fprintf(stderr, "                  Default value is %f secs\n",
			defaults.duration);
	fprintf(stderr, "  --nlayers n     Specifies number of sound layers to use\n");
	fprintf(stderr, "                  to build up each explosion.  Default is %d\n", defaults.nlayers);
	fprintf(stderr, "  --preexplosions n\n");
	fprintf(stderr, "                  Specifies number of 'pre-explostions' to generate\n");
	fprintf(stderr, "                  Default is %d\n", defaults.preexplosions);
	fprintf(stderr, "  --pre-delay n\n");
	fprintf(stderr, "                  Specifies approximate length of the 'ka' in 'ka-BOOM!'\n");
	fprintf(stderr, "                  (it is somewhat randomized)\n");
	fprintf(stderr, "                  Default is %f secs\n", defaults.preexplosion_delay);
	fprintf(stderr, "  --pre-lp-factor n\n");
	fprintf(stderr, "                  Specifies the impact of the low pass filter used\n");
	fprintf(stderr, "                  on the pre-explosion part of the sound.  values\n");
	fprintf(stderr, "                  closer to zero lower the cutoff frequency\n");
	fprintf(stderr, "                  while values close to one raise it.\n");
	fprintf(stderr, "                  Value should be between 0.2 and 0.9.\n");
	fprintf(stderr, "                  Default is %f\n", defaults.preexplosion_low_pass_factor);
	fprintf(stderr, "  --pre-lp-count n\n");
	fprintf(stderr, "                  Specifies the number of times the low pass filter used\n");
	fprintf(stderr, "                  on the pre-explosion part of the sound.  values\n");

	fprintf(stderr, "                  Default is %f\n", defaults.preexplosion_lp_iters);
	
	fprintf(stderr, "  --speedfactor n\n");
	fprintf(stderr, "                  Amount to speed up (or slow down) the final\n");
	fprintf(stderr, "                  explosion sound. Values greater than 1.0 speed\n");
	fprintf(stderr, "                  the sound up, values less than 1.0 slow it down\n");
	fprintf(stderr, "                  Default is %f\n", defaults.final_speed_factor);
	fprintf(stderr, "  --noreverb      Suppress the 'reverb' effect\n");
	fprintf(stderr, "  --input file    Use the given (44100Hz mono) wav file\n"
			"                  as input instead of generating white noise for input.\n");
	exit(1);
}

static double drand(void)
{
	return (double) rand() / (double) RAND_MAX;
}

static int irand(int n)
{
	return (n * (rand() & 0x0ffff)) / 0x0ffff;
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
	printf("Saved output in '%s'\n", filename);
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

static void amplify_in_place(struct sound *s, double gain)
{
	int i;

	for (i = 0; i < s->nsamples; i++) {
		s->data[i] = s->data[i] * gain;
		if (s->data[i] > 1.0)
			s->data[i] = 1.0;
		if (s->data[i] < -1.0)
			s->data[i] = -1.0;
	}
}

static struct sound *make_noise(int nsamples)
{
	int i;
	struct sound *s;
	s = alloc_sound(nsamples);

	/* If there is input data, use that rather than generating noise */
	if (e.input_data) {
		int n;

		n = nsamples;
		if (e.input_samples < (unsigned long long) n)
			n = e.input_samples;
		memset(s->data, 0, sizeof(s->data[0]) * nsamples);
		memcpy(s->data, e.input_data, n * sizeof(s->data[0]));
		s->nsamples = nsamples;
		return s;
	}
		
	/* generate noise */
	for (i = 0; i < nsamples; i++) {
		s->data[i] = 2.0 * drand() - 1.0;
		s->nsamples++;
	}
	amplify_in_place(s, 0.70);
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

	o = alloc_sound(s->nsamples);

	memcpy(o->data, s->data, sizeof(o->data[0]) * s->nsamples);
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
		s->data[i] = s->data[i] / (1.05 * max);
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

static void dot(void)
{
	printf("."); fflush(stdout);
}

static struct sound *poor_mans_reverb(struct sound *s,
	int early_refls, int late_refls)
{
	int i, delay;
	struct sound *echo, *echo2;
	struct sound *withverb;
	double gain;

	printf("Calculating poor man's reverb");
	fflush(stdout);
	withverb = alloc_sound(s->nsamples * 2);
	for (i = 0; i < s->nsamples; i++)
		withverb->data[i] = s->data[i];
	dot();
	withverb->nsamples = s->nsamples * 2;
	echo = copy_sound(withverb);

	for (i = 0; i < early_refls; i++) {
		dot();
		echo2 = sliding_low_pass(echo, 0.5, 0.5);
		gain = drand() * 0.03 + 0.03;
		amplify_in_place(echo, gain); 

		/* 300 ms range */
		delay = (3 * 4410 * (rand() & 0x0ffff)) / 0x0ffff;
		delay_effect_in_place(echo2, delay);
		accumulate_sound(withverb, echo2);
		free_sound(echo2);
	}

	for (i = 0; i < late_refls; i++) {
		dot();
		echo2 = sliding_low_pass(echo, 0.5, 0.2);
		gain = drand() * 0.01 + 0.03;
		amplify_in_place(echo, gain); 

		/* 2000 ms range */
		delay = (2 * 44100 * (rand() & 0x0ffff)) / 0x0ffff;
		delay_effect_in_place(echo2, delay);
		accumulate_sound(withverb, echo2);
		free_sound(echo2);
	}
	printf("done\n");
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

static struct sound *make_preexplosions(struct explosion_def *e)
{
	struct sound *pe;
	int i;

	if (!e->preexplosions)
		return NULL;

	pe = alloc_sound(seconds_to_frames(e->duration));
	pe->nsamples = seconds_to_frames(e->duration);
	for (i = 0; i < e->preexplosions; i++) {
		struct sound *exp;
		int offset;
		exp = make_explosion(e->duration / 2, e->nlayers);
		offset = irand(seconds_to_frames(e->preexplosion_delay));
		delay_effect_in_place(exp, offset);
		accumulate_sound(pe, exp);
		renormalize(pe);
		free_sound(exp);
	}
	for (i = 0 ; i < e->preexplosion_lp_iters; i++) {
		sliding_low_pass_inplace(pe,
			e->preexplosion_low_pass_factor,
			e->preexplosion_low_pass_factor);
	}
	renormalize(pe);
	return pe;
}

static void read_input_file(char *filename,
	double **input_data, unsigned long long *input_samples)
{
	SF_INFO sfi;
	SNDFILE *sf;
	unsigned long long nframes;
	unsigned long long buffersize;
	unsigned long long samples;

	memset(&sfi, 0, sizeof(sfi));

	sf = sf_open(filename, SFM_READ, &sfi);
	if (!sf) {
		fprintf(stderr, "explodomatica: Cannot open '%s': %s\n", 
			filename, sf_strerror(sf));
		exit(1);
	}

	printf("Input file:%s\n", filename);
	printf("  frames:      %llu\n", sfi.frames);
	printf("  sample rate: %d\n", sfi.samplerate);
	printf("  channels:    %d\n", sfi.channels);
	printf("    format:    %d\n", sfi.format);
	printf("  sections:    %d\n", sfi.sections);
	printf("  seekable:    %d\n", sfi.seekable);

	samples = sfi.channels * sfi.frames;
	buffersize = (sizeof(*input_data[0]) * samples);
	*input_data = malloc(buffersize);
	memset(*input_data, 0, buffersize); 

	printf("samples = %llu\n", samples);
	nframes = sf_read_double(sf, *input_data, samples); 
	if (nframes != samples) {
		fprintf(stderr, "explodomatica: Error reading '%s': %s\n", 
			filename, sf_strerror(sf));
		exit(1);
	}
	*input_samples = nframes;

	sf_close(sf);	
}

static void process_options(int argc, char *argv[], struct explosion_def *e)
{
	int this_option_optind = optind ? optind : 1;
	int option_index = 0;
	int c, n, ival;
	double dval;

	static struct option long_options[] = {
		{"duration", 1, 0, 0},
		{"nlayers", 1, 0, 1},
		{"preexplosions", 1, 0, 2},
		{"speedfactor", 1, 0, 3},
		{"pre-delay", 1, 0, 4},
		{"pre-lp-factor", 1, 0, 5},
		{"pre-lp-count", 1, 0, 6},
		{"noreverb", 0, 0, 7},
		{"input", 1, 0, 8},
		{0, 0, 0, 0}
	};

	while (1) {

		c = getopt_long(argc, argv, "d:l:p:s:",
			long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 0: /* duration */
			n = sscanf(optarg, "%lg", &dval);
			if (n != 1)
				usage();
			e->duration = dval;
			printf("duration = %g\n", dval);
			break;
		case 1: /* nlayers */
			n = sscanf(optarg, "%d", &ival);
			if (n != 1)
				usage();
			printf("nlayers = %d\n", ival);
			e->nlayers = ival;
			break;
		case 2: /* preexplosions */
			n = sscanf(optarg, "%d", &ival);
			if (n != 1)
				usage();
			printf("preexplosions = %d\n", ival);
			e->preexplosions = ival;
			break;
		case 3: /* speedfactor */
			n = sscanf(optarg, "%lg", &dval);
			if (n != 1)
				usage();
			e->final_speed_factor = dval;
			printf("speedfactor = %g\n", dval);
			break;
		case 4: /* pre-delay */
			n = sscanf(optarg, "%lg", &dval);
			if (n != 1)
				usage();
			e->preexplosion_delay = dval;
			printf("preexplosion_delay = %g\n", dval);
			break;
		case 5: /* pre-lp-factor */
			n = sscanf(optarg, "%lg", &dval);
			if (n != 1)
				usage();
			e->preexplosion_low_pass_factor = dval;
			printf("preexplosion_low_pass_factor = %g\n", dval);
			break;
		case 6: /* preexplosion_lp_iters */
			n = sscanf(optarg, "%d", &ival);
			if (n != 1)
				usage();
			printf("preexplosion low pass count = %d\n", ival);
			e->preexplosion_lp_iters = ival;
			break;
		case 7: /* noreverb */
			printf("noreverb selected\n");
			e->reverb = 0;
			break;

		case 8: /* input file */
			strncpy(e->input_file, optarg, PATH_MAX);
			printf("input file: '%s'\n", e->input_file);
			read_input_file(e->input_file, &e->input_data, &e->input_samples);
			break;
			
		default:
			usage();
		}
	}
	if (optind < argc) {
		strcpy(e->save_filename, argv[optind]);
		printf("save filename is %s\n", e->save_filename);
	} else
		usage();
}

void explodomatica(struct explosion_def *e)
{
	struct sound *pe, *s, *s2;

	pe = make_preexplosions(e);
	s = make_explosion(e->duration, e->nlayers);
	if (pe) {
		accumulate_sound(s, pe);
		renormalize(s);
	}
	change_speed_inplace(s, e->final_speed_factor);
	trim_trailing_silence(s);
	if (e->reverb) {
		s2 = poor_mans_reverb(s, e->reverb_early_refls, e->reverb_late_refls);
		trim_trailing_silence(s2);
	} else {
		s2 = copy_sound(s);
	}
	save_file(e->save_filename, s2, 1);
	free_sound(s);
}

int main(int argc, char *argv[])
{
	struct timeval tv;

	e = defaults;

	gettimeofday(&tv, NULL);
	srand(tv.tv_usec);

	if (argc < 2)
		usage();

	process_options(argc, argv, &e);
	explodomatica(&e);

	return 0;
}

