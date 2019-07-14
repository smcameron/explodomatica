
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

#include "explodomatica.h"

static struct explosion_def explodomatica_defaults = EXPLOSION_DEF_DEFAULTS;

void usage(void)
{
	fprintf(stderr, "usage:\n");
	fprintf(stderr, "explodomatica [options] somefile.wav\n");
	fprintf(stderr, "caution: somefile.wav will be overwritten.\n");
	fprintf(stderr, "options:\n");
	fprintf(stderr, "  --duration n    Specifies duration of explosion in secs\n");
	fprintf(stderr, "                  Default value is %f secs\n",
			explodomatica_defaults.duration);
	fprintf(stderr, "  --nlayers n     Specifies number of sound layers to use\n");
	fprintf(stderr, "                  to build up each explosion.  Default is %d\n", explodomatica_defaults.nlayers);
	fprintf(stderr, "  --preexplosions n\n");
	fprintf(stderr, "                  Specifies number of 'pre-explostions' to generate\n");
	fprintf(stderr, "                  Default is %d\n", explodomatica_defaults.preexplosions);
	fprintf(stderr, "  --pre-delay n\n");
	fprintf(stderr, "                  Specifies approximate length of the 'ka' in 'ka-BOOM!'\n");
	fprintf(stderr, "                  (it is somewhat randomized)\n");
	fprintf(stderr, "                  Default is %f secs\n", explodomatica_defaults.preexplosion_delay);
	fprintf(stderr, "  --pre-lp-factor n\n");
	fprintf(stderr, "                  Specifies the impact of the low pass filter used\n");
	fprintf(stderr, "                  on the pre-explosion part of the sound.  values\n");
	fprintf(stderr, "                  closer to zero lower the cutoff frequency\n");
	fprintf(stderr, "                  while values close to one raise it.\n");
	fprintf(stderr, "                  Value should be between 0.2 and 0.9.\n");
	fprintf(stderr, "                  Default is %f\n", explodomatica_defaults.preexplosion_low_pass_factor);
	fprintf(stderr, "  --pre-lp-count n\n");
	fprintf(stderr, "                  Specifies the number of times the low pass filter used\n");
	fprintf(stderr, "                  on the pre-explosion part of the sound.  values\n");

	fprintf(stderr, "                  Default is %d\n", explodomatica_defaults.preexplosion_lp_iters);
	
	fprintf(stderr, "  --speedfactor n\n");
	fprintf(stderr, "                  Amount to speed up (or slow down) the final\n");
	fprintf(stderr, "                  explosion sound. Values greater than 1.0 speed\n");
	fprintf(stderr, "                  the sound up, values less than 1.0 slow it down\n");
	fprintf(stderr, "                  Default is %f\n", explodomatica_defaults.final_speed_factor);
	fprintf(stderr, "  --noreverb      Suppress the 'reverb' effect\n");
	fprintf(stderr, "  --input file    Use the given (44100Hz mono) wav file\n"
			"                  as input instead of generating white noise for input.\n");
	exit(1);
}

static void process_options(int argc, char *argv[], struct explosion_def *e)
{
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

int main(int argc, char *argv[])
{
	struct timeval tv;
	struct explosion_def e;
	struct sound *s;

	e = explodomatica_defaults;

	gettimeofday(&tv, NULL);
	srand(tv.tv_usec);

	if (argc < 2)
		usage();

	process_options(argc, argv, &e);
	s = explodomatica(&e);
	free_sound(s);

	return 0;
}

