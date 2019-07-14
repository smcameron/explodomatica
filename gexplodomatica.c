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
#include <stdlib.h>
#include <gtk/gtk.h>
#include <pthread.h>

#include "explodomatica.h"
#include "wwviaudio.h"

static struct sound *generated_sound = NULL;

#define ARRAYSIZE(x) (sizeof(x) / sizeof((x)[0]))

struct slider_spec {
	char *labeltext;
	double r1, r2, inc, initial_value;
	char *tooltiptext;
} sliderspeclist[] = {
	{ "Layers:", 1.0, 6.0, 1.0, 4.0,
			"Specifies number of sound layers to use to build up each explosion" },
	{ "Duration (secs):", 0.2, 60.0, 0.05, 15.0,
			"Specifies duration of explosion in seconds" },
	{ "Pre-explosions:", 0.0, 5.0, 1.0, 1.0,
			"Number of \"pre-explosions\" to use.  You can think of pre-explosions "
			"as the \"ka-\" in \"ka-BOOM!\"" },
	{ "Pre-delay:", 0.1, 3.0, 0.05, 0.20,
			"Duration of \"pre-explosions\" in seconds before the \"main\" "
			"explosion kicks in." },
	{ "Pre-lp-factor:", 0.2, 0.9, 0.05, 0.8,
			"Specifies the impact of the low pass filter used "
			"on the pre-explosion part of the sound. Values "
			"closer to zero lower the cutoff frequency "
			"while values close to one raise the cutoff frequency. "
			"Value should be between 0.2 and 0.9. "
			"Default is 0.800000" },
	{ "Pre-lp-count:", 0, 10, 1.0, 2.0,
			"Specifies the number of times the low pass filter used "
			"on the pre-explosion part of the sound." },
	{ "Speed factor:", 0.1, 10.0, 0.05, 1.0,
			"Amount to speed up (or slow down) the final "
			"explosion sound. Values greater than 1.0 speed "
			"the sound up, values less than 1.0 slow it down." },
	{ "Reverb early refls:", 1.0, 50.0, 1.0, 5.0,
			"Number of early reflections in reverb" },
	{ "Reverb late refls:", 1.0, 1000.0, 1.0, 40.0,
			"Number of late reflections in reverb" },
};

struct slider {
	GtkWidget *label, *slider;
	double r1, r2, inc;
};

typedef void (*clickfunction)(GtkWidget *widget, gpointer data);

static void generateclicked(GtkWidget *widget, gpointer data);
static void inputclicked(GtkWidget *widget, gpointer data);
static void playclicked(GtkWidget *widget, gpointer data);
static void cancelclicked(GtkWidget *widget, gpointer data);
static void saveclicked(GtkWidget *widget, gpointer data);
static void quitclicked(GtkWidget *widget, gpointer data);

struct button_spec {
	char *buttontext;
	clickfunction f;
	char *tooltiptext;
} buttonspeclist[] = {
#define GENERATEBUTTON 0
	{ "Generate", generateclicked, "Generate an explosion sound effect using the "
					"current values of all parameters"},
#define INPUTBUTTON 1
	{ "Input", inputclicked, "Use wav file data as input instead of generating white noise as input."},
#define PLAYBUTTON 2
	{ "Play", playclicked, "Play the most recently generated sound."},
#define SAVEBUTTON 3
	{ "Save", saveclicked, "Save the most recently generated sound."},
#define CANCELBUTTON 4
	{ "Cancel", cancelclicked, "Stop calculating audio data."},
	{ "Quit", quitclicked, "Quit Explodomatica"},

};

struct gui {
	GtkWidget *window;
	GtkWidget *vbox1;
	GtkWidget *slidertable;
	struct slider sliderlist[ARRAYSIZE(sliderspeclist)];
	GtkWidget *button[ARRAYSIZE(buttonspeclist)];
	GtkWidget *drawingbox;
	GtkWidget *drawing_area;
	GtkWidget *reverbcheck;
	GtkWidget *whitenoisecheck;
	GtkWidget *buttonhbox;
	GtkWidget *file_selection;
	GtkWidget *input_file_selection;
	GtkWidget *progress_bar;
	volatile float progress;
	struct explosion_def e;
	struct explodomatica_thread_arg arg;
	int ptimer;
	pthread_t t;
	int thread_done;
	char input_file[PATH_MAX];
};

#if 0
static gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    return TRUE;
}
#endif

static void quitclicked(__attribute__((unused)) GtkWidget *widget,
		__attribute__((unused)) gpointer data)
{
	gtk_main_quit();
}

static void playclicked(__attribute__((unused)) GtkWidget *widget,
		__attribute__((unused)) gpointer data)
{
	if (!generated_sound) {
		return;
	}
	wwviaudio_cancel_all_sounds();
	wwviaudio_use_double_clip(1, generated_sound->data, generated_sound->nsamples);
	wwviaudio_add_sound(1);
}

static void cancelclicked(__attribute__((unused)) GtkWidget *widget,
		__attribute__((unused)) gpointer data)
{
	struct gui *ui = data;
	if (ui->thread_done)
		return;
	pthread_cancel(ui->t);
	ui->progress = 0.0;
	generated_sound = 0;
	gtk_widget_set_sensitive(ui->button[GENERATEBUTTON], 1);
	gtk_widget_set_sensitive(ui->button[SAVEBUTTON], 0);
	gtk_widget_set_sensitive(ui->button[PLAYBUTTON], 0);
	gtk_widget_set_sensitive(ui->button[CANCELBUTTON], 0);
}

static void saveclicked(__attribute__((unused)) GtkWidget *widget, gpointer data)
{
	struct gui *ui = data;

	if (!generated_sound) {
		return;
	}
	gtk_widget_show(ui->file_selection);
}

static void inputclicked(__attribute__((unused)) GtkWidget *widget, gpointer data)
{
	struct gui *ui = data;
	gtk_widget_show(ui->input_file_selection);
}

#define LAYERS 0
#define DURATION 1
#define PREEXPLOSIONS 2
#define PREEXPLOSION_DELAY 3
#define PREEXPLOSION_LP_FACTOR 4
#define PREEXPLOSION_LP_ITERS 5
#define FINAL_SPEED_FACTOR 6
#define REVERB_EARLY_REFLS 7
#define REVERB_LATE_REFLS 8

static void data_ready(struct sound *s, void *x)
{
	struct gui *ui = x;
	generated_sound = s;
	ui->thread_done = 1;
}

static void generateclicked(__attribute__((unused)) GtkWidget *widget, gpointer data)
{
	struct gui *ui = data;

	ui->progress = 0.0;

	/* disable save and play buttons while sound is generated */
	gtk_widget_set_sensitive(ui->button[GENERATEBUTTON], 0);
	gtk_widget_set_sensitive(ui->button[SAVEBUTTON], 0);
	gtk_widget_set_sensitive(ui->button[PLAYBUTTON], 0);
	gtk_widget_set_sensitive(ui->button[CANCELBUTTON], 1);

	ui->e = explodomatica_defaults;
	
	strcpy(ui->e.save_filename, "");
	if (gtk_toggle_button_get_active((GtkToggleButton *) ui->whitenoisecheck))
		strcpy(ui->e.input_file, "");
	else
		strcpy(ui->e.input_file, ui->input_file);
	if (ui->e.input_data != NULL) {
		free(ui->e.input_data);
		ui->e.input_data = NULL;
	}
	ui->e.input_samples = 0;

	ui->e.nlayers = (int) gtk_range_get_value(GTK_RANGE(ui->sliderlist[LAYERS].slider));
	ui->e.duration = gtk_range_get_value(GTK_RANGE(ui->sliderlist[DURATION].slider));
	ui->e.preexplosions = (int) gtk_range_get_value(GTK_RANGE(ui->sliderlist[PREEXPLOSIONS].slider));
	ui->e.preexplosion_delay = gtk_range_get_value(GTK_RANGE(ui->sliderlist[PREEXPLOSION_DELAY].slider));
	ui->e.preexplosion_low_pass_factor = gtk_range_get_value(GTK_RANGE(ui->sliderlist[PREEXPLOSION_LP_FACTOR].slider));
	ui->e.preexplosion_lp_iters = (int) gtk_range_get_value(GTK_RANGE(ui->sliderlist[PREEXPLOSION_LP_ITERS].slider));
	ui->e.final_speed_factor = gtk_range_get_value(GTK_RANGE(ui->sliderlist[FINAL_SPEED_FACTOR].slider));
	ui->e.reverb_early_refls = (int) gtk_range_get_value(GTK_RANGE(ui->sliderlist[REVERB_EARLY_REFLS].slider));
	ui->e.reverb_late_refls = (int) gtk_range_get_value(GTK_RANGE(ui->sliderlist[REVERB_LATE_REFLS].slider));
	ui->e.reverb = gtk_toggle_button_get_active((GtkToggleButton *) ui->reverbcheck);

	if (generated_sound)
		free_sound(generated_sound);

	ui->arg.e = &ui->e;
	ui->arg.f = data_ready; 
	ui->arg.arg = ui;
	explodomatica_thread(&ui->t, &ui->arg);
}

static void add_slider(GtkWidget *container, int row,
		char *labeltext, struct slider *s,
		double r1, double r2, double inc, double initial_value,
		char *tooltiptext)
{
	s->label = gtk_label_new(labeltext);
	gtk_label_set_justify(GTK_LABEL(s->label), GTK_JUSTIFY_RIGHT);
	s->slider = gtk_hscale_new_with_range(r1, r2, inc);
	gtk_range_set_value(GTK_RANGE(s->slider), initial_value);
	if (tooltiptext) {
		gtk_widget_set_tooltip_text(s->slider, tooltiptext);
		gtk_widget_set_tooltip_text(s->label, tooltiptext);
	}
	gtk_table_attach(GTK_TABLE(container), s->label, 0, 1, row, row + 1, 0, 0, 5, 1);
	gtk_table_attach(GTK_TABLE(container), s->slider, 1, 2, row, row + 1,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 5, 1);
	s->r1 = r1;
	s->r2 = r2;
	s->inc = inc;
}

static void show_slider(struct slider *s)
{
	gtk_widget_show(s->label);
	gtk_widget_show(s->slider);
}

static void save_file_selected(__attribute__((unused)) GtkWidget *w, struct gui *ui)
{
	char *filename = (char *) gtk_file_selection_get_filename (GTK_FILE_SELECTION (ui->file_selection));	
	if (!generated_sound) {
		printf("Nothing to save\n");
		return;
	}
	printf("Saving %s\n", filename);
	explodomatica_save_file(filename, generated_sound, 1);
	gtk_widget_hide(ui->file_selection);
	return;
}

static void input_file_selected(__attribute__((unused)) GtkWidget *w, struct gui *ui)
{
	char *filename = (char *) gtk_file_selection_get_filename(GTK_FILE_SELECTION (ui->input_file_selection));	

	strncpy(ui->input_file, filename, sizeof(ui->input_file));
	gtk_widget_hide(ui->input_file_selection);
	return;
}

static gint update_progress_bar(gpointer data)
{
	struct gui *ui = data;
	
	if (ui->progress < 0.0)
		ui->progress = 0.0;
	if (ui->progress > 1.0)
		ui->progress = 1.0;	
	gtk_progress_bar_update(GTK_PROGRESS_BAR(ui->progress_bar),
			ui->progress);
	if (ui->thread_done) {
		/* enable save and play buttons after sound is generated */
		gtk_widget_set_sensitive(ui->button[GENERATEBUTTON], 1);
		gtk_widget_set_sensitive(ui->button[SAVEBUTTON], 1);
		gtk_widget_set_sensitive(ui->button[PLAYBUTTON], 1);
		gtk_widget_set_sensitive(ui->button[CANCELBUTTON], 0);
		pthread_join(ui->t, NULL);
		ui->thread_done = 0;
	}
	return TRUE;
}

typedef void (*file_selected_function)(GtkWidget *w, struct gui *ui);

static void setup_file_selection(GtkWidget **file_selection, char *title,
		file_selected_function ok_selected, void *arg, char *default_filename)
{
	*file_selection = gtk_file_selection_new(title);
	g_signal_connect (*file_selection, "delete-event", G_CALLBACK (gtk_widget_hide), *file_selection);
	g_signal_connect (*file_selection, "destroy", G_CALLBACK (gtk_widget_hide), *file_selection);
	g_signal_connect (GTK_FILE_SELECTION (*file_selection)->ok_button,
		"clicked", G_CALLBACK (ok_selected), (gpointer) arg);
    
	/* Connect the cancel_button to hide the widget */
	g_signal_connect_swapped(GTK_FILE_SELECTION (*file_selection)->cancel_button,
	                      "clicked", G_CALLBACK (gtk_widget_hide),
			      *file_selection);
    
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(*file_selection),
						default_filename);
}

static void init_ui(int *argc, char **argv[], struct gui *ui)
{
	unsigned int i;

	gtk_init(argc, argv);

	strcpy(ui->input_file, "");
	ui->progress = 0.0;
	ui->thread_done = 0;
	ui->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW (ui->window), "Explodomatica");

	g_signal_connect(ui->window, "delete-event", G_CALLBACK (quitclicked), NULL);
	g_signal_connect(ui->window, "destroy", G_CALLBACK (quitclicked), NULL);

	ui->vbox1 = gtk_vbox_new(FALSE, 0);
	ui->slidertable = gtk_table_new(ARRAYSIZE(ui->sliderlist), 2, FALSE);
	ui->drawingbox = gtk_hbox_new(FALSE, 0);
	ui->buttonhbox = gtk_hbox_new(FALSE, 0);
	ui->drawing_area = gtk_drawing_area_new();
	ui->progress_bar = gtk_progress_bar_new();

	gtk_container_add(GTK_CONTAINER (ui->window), ui->vbox1);
	gtk_container_add(GTK_CONTAINER (ui->vbox1), ui->slidertable);

	for (i = 0; i < ARRAYSIZE(ui->sliderlist); i++) {
		add_slider(ui->slidertable, i, sliderspeclist[i].labeltext, &ui->sliderlist[i],
			sliderspeclist[i].r1, sliderspeclist[i].r2, sliderspeclist[i].inc,
			sliderspeclist[i].initial_value,
			sliderspeclist[i].tooltiptext);
	}

	ui->reverbcheck = gtk_check_button_new_with_label("Poor man's reverb");
	gtk_widget_set_tooltip_text(ui->reverbcheck, "Enable (or disable) \"poor man's reverb\"");
	/* gtk_toggle_button_set_active((GtkToggleButton *) ui->reverbcheck, TRUE); */

	ui->whitenoisecheck = gtk_check_button_new_with_label("Use white noise");
	gtk_widget_set_tooltip_text(ui->whitenoisecheck,
		"If checked, use white noise as input signal.  "
		"If not checked, use specified 44.1kHz mono wave file as "
		"input signal (use Input button below)");
	gtk_box_pack_start(GTK_BOX(ui->drawingbox), ui->drawing_area, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(ui->vbox1), ui->drawingbox);

	for (i = 0; i < ARRAYSIZE(ui->button); i++) {
		ui->button[i] = gtk_button_new_with_label(buttonspeclist[i].buttontext);
		g_signal_connect(ui->button[i], "clicked", G_CALLBACK (buttonspeclist[i].f), ui);
		gtk_box_pack_start(GTK_BOX (ui->buttonhbox), ui->button[i], TRUE, TRUE, 0);
		gtk_widget_set_tooltip_text(ui->button[i], buttonspeclist[i].tooltiptext);
	}

	gtk_box_pack_start(GTK_BOX (ui->vbox1), ui->reverbcheck, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX (ui->vbox1), ui->whitenoisecheck, TRUE, TRUE, 1);
	gtk_container_add(GTK_CONTAINER(ui->vbox1), ui->progress_bar);
	gtk_container_add(GTK_CONTAINER (ui->vbox1), ui->buttonhbox);

	gtk_window_set_default_size(GTK_WINDOW(ui->window), 800, 500);

	setup_file_selection(&ui->file_selection, "Save Audio file", save_file_selected, ui, "explosion.wav");
	setup_file_selection(&ui->input_file_selection, "Select input file", input_file_selected, ui, "");

	/* No sound yet generated, so disable buttons until then */    
	gtk_widget_set_sensitive(ui->button[SAVEBUTTON], 0);
	gtk_widget_set_sensitive(ui->button[PLAYBUTTON], 0);
	gtk_widget_set_sensitive(ui->button[CANCELBUTTON], 0);

	gtk_widget_show(ui->vbox1);
	gtk_widget_show(ui->progress_bar);
	gtk_widget_show(ui->buttonhbox);
	gtk_widget_show(ui->drawingbox);
	gtk_widget_show(ui->drawing_area);
	for (i = 0; i < ARRAYSIZE(ui->sliderlist); i++)
		show_slider(&ui->sliderlist[i]);
	gtk_widget_show(ui->slidertable);
	gtk_widget_show(ui->reverbcheck);
	gtk_widget_show(ui->whitenoisecheck);
	for (i = 0; i < ARRAYSIZE(ui->button); i++)
		gtk_widget_show(ui->button[i]);
	gtk_widget_show(ui->drawing_area);
	gtk_widget_show(ui->window);
	ui->ptimer = gtk_timeout_add(200, update_progress_bar, ui);
	explodomatica_progress_variable(&ui->progress);
}

int main(int argc, char *argv[])
{
	struct gui ui;

	wwviaudio_set_sound_device(-1);
	if (wwviaudio_initialize_portaudio(20, 20)) {
		fprintf(stderr, "Can't initialized port audio\n");
		exit(1);
	}
	init_ui(&argc, &argv, &ui);
	gtk_main();
	wwviaudio_cancel_all_sounds();
	wwviaudio_stop_portaudio();

	return 0;
}
