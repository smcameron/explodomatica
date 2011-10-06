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
	{ "Reverb late refls:", 1.0, 2000.0, 1.0, 1000.0,
			"Number of late reflections in reverb" },
};

struct slider {
	GtkWidget *label, *slider;
	double r1, r2, inc;
};

typedef void (*clickfunction)(GtkWidget *widget, gpointer data);

static void mutateclicked(GtkWidget *widget, gpointer data);
static void generateclicked(GtkWidget *widget, gpointer data);
static void playclicked(GtkWidget *widget, gpointer data);
static void saveclicked(GtkWidget *widget, gpointer data);
static void quitclicked(GtkWidget *widget, gpointer data);

struct button_spec {
	char *buttontext;
	clickfunction f;
	char *tooltiptext;
} buttonspeclist[] = {
	{ "Mutate", mutateclicked, "Randomly alter all parameters by some small amount."},
	{ "Generate", generateclicked, "Generate an explosion sound effect using the "
					"current values of all parameters"},
	{ "Play", playclicked, "Play the most recently generated sound."},
	{ "Save", saveclicked, "Save the most recently generated sound."},
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
	GtkWidget *buttonhbox;
	GtkWidget *file_selection;
};


static gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    return TRUE;
}

static void quitclicked(GtkWidget *widget, gpointer data)
{
    gtk_main_quit();
}

static void playclicked(GtkWidget *widget, gpointer data)
{
	printf("play clicked\n");
}

static void saveclicked(GtkWidget *widget, gpointer data)
{
	struct gui *ui = data;

	printf("save clicked\n");
	if (!generated_sound) {
		printf("nothing to save.\n");
		return;
	}
	gtk_widget_show(ui->file_selection);
}

static void mutateclicked(GtkWidget *widget, gpointer data)
{
	printf("mutate clicked\n");
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

static void generateclicked(GtkWidget *widget, gpointer data)
{
	struct gui *ui = data;
	struct explosion_def e;

	printf("generate clicked\n");

	e = explodomatica_defaults;

	strcpy(e.save_filename, "");
	strcpy(e.input_file, "");
	e.input_data = NULL;
	e.input_samples = 0;

	e.nlayers = (int) gtk_range_get_value(GTK_RANGE(ui->sliderlist[LAYERS].slider));
	e.duration = gtk_range_get_value(GTK_RANGE(ui->sliderlist[DURATION].slider));
	e.preexplosions = (int) gtk_range_get_value(GTK_RANGE(ui->sliderlist[PREEXPLOSIONS].slider));
	e.preexplosion_delay = gtk_range_get_value(GTK_RANGE(ui->sliderlist[PREEXPLOSION_DELAY].slider));
	e.preexplosion_low_pass_factor = gtk_range_get_value(GTK_RANGE(ui->sliderlist[PREEXPLOSION_LP_FACTOR].slider));
	e.preexplosion_lp_iters = (int) gtk_range_get_value(GTK_RANGE(ui->sliderlist[PREEXPLOSION_LP_ITERS].slider));
	e.final_speed_factor = gtk_range_get_value(GTK_RANGE(ui->sliderlist[FINAL_SPEED_FACTOR].slider));
	e.reverb_early_refls = (int) gtk_range_get_value(GTK_RANGE(ui->sliderlist[REVERB_EARLY_REFLS].slider));
	e.reverb_late_refls = (int) gtk_range_get_value(GTK_RANGE(ui->sliderlist[REVERB_LATE_REFLS].slider));
	e.reverb = gtk_toggle_button_get_active((GtkToggleButton *) ui->reverbcheck);

	if (generated_sound)
		free_sound(generated_sound);
	generated_sound = explodomatica(&e);
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
	if (tooltiptext)
		gtk_widget_set_tooltip_text(s->slider, tooltiptext);
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

static void save_file_selected(GtkWidget *w, struct gui *ui)
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

static void init_ui(int *argc, char **argv[], struct gui *ui)
{
	unsigned int i;

	gtk_init(argc, argv);

	ui->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW (ui->window), "Explodomatica");

	g_signal_connect(ui->window, "delete-event", G_CALLBACK (delete_event), NULL);
	g_signal_connect(ui->window, "destroy", G_CALLBACK (quitclicked), NULL);

	ui->vbox1 = gtk_vbox_new(FALSE, 0);
	ui->slidertable = gtk_table_new(ARRAYSIZE(ui->sliderlist), 2, FALSE);
	ui->drawingbox = gtk_hbox_new(FALSE, 0);
	ui->buttonhbox = gtk_hbox_new(FALSE, 0);
	ui->drawing_area = gtk_drawing_area_new();

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
	gtk_toggle_button_set_active((GtkToggleButton *) ui->reverbcheck, TRUE);

	gtk_box_pack_start(GTK_BOX(ui->drawingbox), ui->drawing_area, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(ui->vbox1), ui->drawingbox);

	for (i = 0; i < ARRAYSIZE(ui->button); i++) {
		ui->button[i] = gtk_button_new_with_label(buttonspeclist[i].buttontext);
		g_signal_connect(ui->button[i], "clicked", G_CALLBACK (buttonspeclist[i].f), ui);
		gtk_box_pack_start(GTK_BOX (ui->buttonhbox), ui->button[i], TRUE, TRUE, 0);
		gtk_widget_set_tooltip_text(ui->button[i], buttonspeclist[i].tooltiptext);
	}

	gtk_box_pack_start(GTK_BOX (ui->vbox1), ui->reverbcheck, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER (ui->vbox1), ui->buttonhbox);

	gtk_window_set_default_size(GTK_WINDOW(ui->window), 800, 500);


	ui->file_selection = gtk_file_selection_new("Save Audio file");
	g_signal_connect (ui->file_selection, "destroy", G_CALLBACK (gtk_widget_hide), ui->file_selection);
	g_signal_connect (GTK_FILE_SELECTION (ui->file_selection)->ok_button,
		"clicked", G_CALLBACK (save_file_selected), (gpointer) ui);
    
	/* Connect the cancel_button to hide the widget */
	g_signal_connect_swapped(GTK_FILE_SELECTION (ui->file_selection)->cancel_button,
	                      "clicked", G_CALLBACK (gtk_widget_hide),
			      ui->file_selection);
    
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(ui->file_selection), 
					"explosion.wav");
    
	gtk_widget_show(ui->vbox1);
	gtk_widget_show(ui->buttonhbox);
	gtk_widget_show(ui->drawingbox);
	gtk_widget_show(ui->drawing_area);
	for (i = 0; i < ARRAYSIZE(ui->sliderlist); i++)
		show_slider(&ui->sliderlist[i]);
	gtk_widget_show(ui->slidertable);
	gtk_widget_show(ui->reverbcheck);
	for (i = 0; i < ARRAYSIZE(ui->button); i++)
		gtk_widget_show(ui->button[i]);
	gtk_widget_show(ui->drawing_area);
	gtk_widget_show(ui->window);
}

int main(int argc, char *argv[])
{
	struct gui ui;

	if (wwviaudio_initialize_portaudio(10, 10)) {
		fprintf(stderr, "Can't initialized port audio\n");
		exit(1);
	}
	init_ui(&argc, &argv, &ui);
	gtk_main();
	wwviaudio_cancel_all_sounds();
	wwviaudio_stop_portaudio();

	return 0;
}
