#ifndef __EXPLODOMATICA_H__
#define __EXPLODOMATICA_H__
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

#ifdef DEFINE_EXPLODOMATICA_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

struct sound {
        double *data;
        int nsamples;
};

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
};

static struct explosion_def explodomatica_defaults = {
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

GLOBAL struct sound *explodomatica(struct explosion_def *e);

typedef void (*explodomatica_callback)(struct sound *s, void *arg);

struct explodomatica_thread_arg {
        struct explosion_def *e;
        explodomatica_callback f;
        void *arg;
};

GLOBAL void explodomatica_thread(pthread_t *t, struct explodomatica_thread_arg *arg);

GLOBAL void free_sound(struct sound *s);
GLOBAL int explodomatica_save_file(char *filename, struct sound *s, int channels);
GLOBAL void explodomatica_progress_variable(volatile float *progress);

#endif
