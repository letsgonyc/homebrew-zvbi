/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000 I�aki Garc�a Etxebarria
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include <gdk/gdkx.h>
#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>
#include <glade/glade.h>
#include <signal.h>

#define ZCONF_DOMAIN "/zapping/options/main/"
#include "zmisc.h"
#include "interface.h"
#include "tveng.h"
#include "v4linterface.h"
#include "plugins.h"
#include "zconf.h"
#include "frequencies.h"
#include "sound.h"

/* This comes from callbacks.c */
extern enum tveng_capture_mode restore_mode; /* the mode set when we went
						fullscreen */

/* These are accessed by callbacks.c as extern variables */
tveng_device_info * main_info;
gboolean flag_exit_program = FALSE;
tveng_channels * current_country = NULL;
GList * plugin_list = NULL;
struct soundinfo * si;
gboolean disable_preview = FALSE; /* TRUE if zapping_setup_fb didn't
				     work */
GtkWidget * main_window;
gboolean was_fullscreen=FALSE; /* will be TRUE if when quitting we
				  were fullscreen */

void shutdown_zapping(void);
gboolean startup_zapping(void);

/* Keep compiler happy */
gboolean
delete_event                (GtkWidget       *widget,
			     GdkEvent        *event,
			     gpointer         user_data);

gboolean
delete_event                (GtkWidget       *widget,
			     GdkEvent        *event,
			     gpointer         user_data)
{
  flag_exit_program = TRUE;
  
  return FALSE;
}

int main(int argc, char * argv[])
{
  GtkWidget * tv_screen;
  gchar * buffer;
  GList * p;
  gint x, y, w, h; /* Saved geometry */
  GdkGeometry geometry;
  GdkWindowHints hints;
  plugin_sample sample; /* The a/v sample passed to the plugins */

#ifdef ENABLE_NLS
  bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
  textdomain (PACKAGE);
#endif

  /* Init gnome, libglade, modules and tveng */
  gnome_init ("zapping", VERSION, argc, argv);
  glade_gnome_init();

  if (!g_module_supported ())
    {
      RunBox(_("Sorry, but there is no module support"),
	     GNOME_MESSAGE_BOX_ERROR);
      return 0;
    }

  main_info = tveng_device_info_new( GDK_DISPLAY() );

  if (!main_info)
    {
      g_error(_("Cannot get device info struct"));
      return -1;
    }

  if (!startup_zapping())
    {
      RunBox(_("Zapping couldn't be started"),
	      GNOME_MESSAGE_BOX_ERROR);
      tveng_device_info_destroy(main_info);
      return 0;
    }

  /* We must do this (running zapping_setup_fb) before attaching the
     device because V4L and V4L2 don't support multiple capture opens
  */
 open_device:

  main_info -> file_name = strdup(zcg_char(NULL, "video_device"));

  if (!main_info -> file_name)
    {
      perror("strdup");
      return 1;
    }

  tveng_set_zapping_setup_fb_verbosity(zcg_int(NULL,
					       "zapping_setup_fb_verbosity"),
				       main_info);


  /* try to run the auxiliary suid program */
  if (tveng_run_zapping_setup_fb(main_info) == -1)
    disable_preview = TRUE;

  free(main_info -> file_name);

  if (tveng_attach_device(zcg_char(NULL, "video_device"),
			  TVENG_ATTACH_READ,
			  main_info) == -1)
    {
      /* Check that the given device is /dev/video, if it isn't, try
	 it */
      if (strcmp(zcg_char(NULL, "video_device"), "/dev/video"))
	{
	  GtkWidget * question_box = gnome_message_box_new(
               _("The specified device isn't \"/dev/video\".\n"
		 "Should I try it?"),
	       GNOME_MESSAGE_BOX_QUESTION,
	       GNOME_STOCK_BUTTON_YES,
	       GNOME_STOCK_BUTTON_NO,
	       NULL);

	  gtk_window_set_title(GTK_WINDOW(question_box),
			       zcg_char(NULL, "video_device"));
	 
	  switch (gnome_dialog_run(GNOME_DIALOG(question_box)))
	    {
	    case 0: /* Retry */
	      zcs_char("/dev/video", "video_device");
	      goto open_device;
	    default:
	      break; /* Don't do anything */
	    }
	}

      buffer =
	g_strdup_printf(_("Sorry, but \"%s\" could not be opened:\n%s"),
			zcg_char(NULL, "video_device"),
			main_info->error);

      RunBox(buffer, GNOME_MESSAGE_BOX_ERROR);

      g_free(buffer);

      return -1;
    }

  /* Do some checks for the preview */
  if ((!disable_preview) && (!tveng_detect_XF86DGA(main_info)))
	disable_preview = TRUE;

  if ((!disable_preview) && (!tveng_detect_preview(main_info)))
	disable_preview = TRUE;

  /* Mute the device while we are starting Zapping */
  if (-1 == tveng_set_mute(1, main_info))
    fprintf(stderr, "%s\n", main_info->error);

  main_window = create_zapping();

  if (!main_window)
    {
      g_warning("Sorry, but the interface (zapping.glade) couldn't"
		" be loaded.\nCheck your installation.");
      tveng_device_info_destroy(main_info);
      return 0;
    }

  tv_screen = lookup_widget(main_window, "tv_screen");

  /* Add the plugins to the GUI */
  p = g_list_first(plugin_list);
  while (p)
    {
      plugin_add_gui(GNOME_APP(main_window),
		     (struct plugin_info*)p->data);
      p = p->next;
    }

  /* Disable preview if needed */
  if (disable_preview)
    {
      gtk_widget_set_sensitive(lookup_widget(main_window, "view1"),
			       FALSE);
      gtk_widget_hide(lookup_widget(main_window, "view1"));
    }
  gtk_widget_show(main_window);

  /* Restore the input and the standard */
  tveng_set_input_by_index(zcg_int(NULL, "current_input"), main_info);
  tveng_set_standard_by_index(zcg_int(NULL, "current_standard"), main_info);

  update_standards_menu(main_window, main_info);

  /* Sets the coords to the previous values, if the users wants to */
  if (zcg_bool(NULL, "keep_geometry"))
    {
      zconf_get_integer(&x, "/zapping/internal/callbacks/x");
      zconf_get_integer(&y, "/zapping/internal/callbacks/y");
      zconf_get_integer(&w, "/zapping/internal/callbacks/w");
      zconf_get_integer(&h, "/zapping/internal/callbacks/h");
      /* Hopefully this will let me track the ghost bug */
#ifdef DEBUG
      g_message("Restoring %dx%d - %d-%d", x, y, w, h);
#endif
      gdk_window_move_resize(main_window->window, x, y, w, h);
    }

  /* Process all events */
  while (gtk_events_pending())
    gtk_main_iteration();

  if (-1 == tveng_set_mute(zcg_bool(NULL, "start_muted"),
			   main_info))
    fprintf(stderr, "tveng_set_mute: %s\n", main_info->error);

  /* Start the capture in the last mode */
  switch (zcg_int(NULL, "capture_mode"))
    {
    case TVENG_CAPTURE_PREVIEW:
      on_go_fullscreen1_activate(GTK_MENU_ITEM(
	 lookup_widget(main_window, "go_fullscreen1")), NULL);
      restore_mode = TVENG_CAPTURE_WINDOW;
      if (main_info->current_mode == TVENG_CAPTURE_PREVIEW)
	break;
    case TVENG_CAPTURE_READ:
      on_go_capturing2_activate(GTK_MENU_ITEM(
	 lookup_widget(main_window, "go_capturing2")), NULL);
      if (main_info->current_mode == TVENG_CAPTURE_READ)
	break;
    default:
      on_go_previewing2_activate(GTK_MENU_ITEM(
	 lookup_widget(main_window, "go_previewing2")), NULL);
      break;
    }

  while (!flag_exit_program)
    {
      while (gtk_events_pending())
	gtk_main_iteration(); /* Check for UI changes */

      if (flag_exit_program)
	continue; /* Exit the loop if neccesary now */

      hints = GDK_HINT_MIN_SIZE;
      geometry.min_width = main_info->caps.minwidth;
      geometry.min_height = main_info->caps.minheight;

      /* Set the geometry flags if needed */
      if (zcg_bool(NULL, "fixed_increments"))
	{
	  geometry.width_inc = 64;
	  geometry.height_inc = 48;
	  hints |= GDK_HINT_RESIZE_INC;
	}

      switch (zcg_int(NULL, "ratio")) {
      case 1:
	geometry.min_aspect = geometry.max_aspect = 4.0/3.0;
	hints |= GDK_HINT_ASPECT;
	break;
      case 2:
	geometry.min_aspect = geometry.max_aspect = 16.0/9.0;
	hints |= GDK_HINT_ASPECT;
	break;
      default:
	break;
      }

      gdk_window_set_geometry_hints(main_window->window, &geometry,
				    hints);

      /* Collect the sound data (the queue needs to be emptied,
	 otherwise mem usage will grow a lot) */
      sound_read_data(si);

      /* We are probably viewing fullscreen, just do nothing */
      if (main_info -> current_mode != TVENG_CAPTURE_READ)
	{
	  usleep(50000);
	  continue;
	}

      /* Avoid segfault */
      if (!zimage_get())
	{
	  g_warning(_("There is no allocated mem for the capture"));
	  continue;
	}

      zimage_reallocate(main_info->format.width, main_info->format.height);

      /* Do the image processing here */
      if (tveng_read_frame(zimage_get_data(zimage_get()),
			   ((int)zimage_get()->bpl)*zimage_get()->height,
			  50, main_info) == -1)
	{
	  g_warning("read(): %s\n", main_info->error);
	  continue;
	}

      /* Give the image to the plugins too */
      memset(&sample, 0, sizeof(plugin_sample));
      memcpy(&(sample.format), &(main_info->format),
	     sizeof(struct tveng_frame_format));
      sample.video_data  = zimage_get_data(zimage_get());
      sample.image       = zimage_get();
      sample.v_timestamp = tveng_get_timestamp(main_info);

      sample.audio_data  = si->buffer;
      sample.audio_size  = si->size;
      sample.audio_bits  = si->bits;
      sample.audio_rate  = si->rate;
      sample.a_timestamp = ((((__s64)si->tv.tv_sec) *
			     1000000)+si->tv.tv_usec)*1000;
      
      p = g_list_first(plugin_list);
      while (p)
	{
	  plugin_process_sample(&sample, (struct plugin_info*)p->data);
	  /* Update the gdkimage, since it may have changed */
	  zimage_reallocate(sample.format.width, sample.format.height);
	  p = p->next;
	}
      gdk_draw_image(tv_screen -> window,
		     tv_screen -> style -> white_gc,
		     zimage_get(),
		     0, 0, 0, 0,
		     sample.format.width,
		     sample.format.height);
    }

  /* Closes all fd's, writes the config to HD, and that kind of things
   */
  shutdown_zapping();

  return 0;
}

void shutdown_zapping(void)
{
  int i = 0;
  gchar * buffer = NULL;
  tveng_tuned_channel * channel;
  gboolean do_screen_cleanup = FALSE;

  /* Stops any capture currently active */
  if (main_info->current_mode == TVENG_CAPTURE_WINDOW)
    do_screen_cleanup = TRUE;

  zcs_int(tveng_stop_everything(main_info), "capture_mode");
  if (was_fullscreen)
    zcs_int(TVENG_CAPTURE_PREVIEW, "capture_mode");

  /* Unloads all plugins, this tells them to save their config too */
  plugin_unload_plugins(plugin_list);
  plugin_list = NULL;

  /* Write the currently tuned channels */
  zconf_delete(ZCONF_DOMAIN "tuned_channels");
  while ((channel = tveng_retrieve_tuned_channel_by_index(i)) != NULL)
    {
      buffer = g_strdup_printf(ZCONF_DOMAIN "tuned_channels/%d/name",
			       i);
      zconf_create_string(channel->name, "Channel name", buffer);
      g_free(buffer);
      buffer = g_strdup_printf(ZCONF_DOMAIN "tuned_channels/%d/freq",
			       i);
      zconf_create_integer((int)channel->freq, "Tuning frequence", buffer);
      g_free(buffer);
      buffer = g_strdup_printf(ZCONF_DOMAIN "tuned_channels/%d/real_name",
			       i);
      zconf_create_string(channel->real_name, "Real channel name", buffer);
      g_free(buffer);
      buffer = g_strdup_printf(ZCONF_DOMAIN "tuned_channels/%d/country",
			       i);
      zconf_create_string(channel->country, 
			  "Country the channel is in", buffer);
      g_free(buffer);
      i++;
    }

  zcs_char(current_country -> name, "current_country");
  if (main_info->num_standards)
    zcs_int(main_info -> cur_standard, "current_standard");
  if (main_info->num_inputs)
    zcs_int(main_info -> cur_input, "current_input");

  sound_destroy_struct(si);

  /* Shutdown sound */
  shutdown_sound();

  /* Shutdown all other modules */
  shutdown_callbacks();

  /* Save the config and show an error if something failed */
  if (!zconf_close())
    ShowBox(_("ZConf could not be closed properly , your\n"
	      "configuration will be lost.\n"
	      "Possible causes for this are:\n"
	      "   - There is not enough free memory\n"
	      "   - You do not have permissions to write to $HOME/.zapping\n"
	      "   - libxml is non-functional (?)\n"
	      "   - or, more probably, you have found a bug in\n"
	      "     Zapping. Please contact the author.\n"
	      ), GNOME_MESSAGE_BOX_ERROR);

  /* Mute the device again and close */
  tveng_set_mute(1, main_info);
  tveng_device_info_destroy(main_info);

  /* Destroy the image that holds the capture */
  zimage_destroy();

  /* refresh the tv screen (in case we were in windowed preview mode)
   */
  if (do_screen_cleanup)
    zmisc_refresh_tv_screen(0, 0, 0, 0, FALSE);
}

gboolean startup_zapping()
{
  int i = 0;
  gchar * buffer = NULL;
  gchar * buffer2 = NULL;
  tveng_tuned_channel new_channel;
  GList * p;

  /* Starts the configuration engine */
  if (!zconf_init("zapping"))
    {
      g_error(_("Sorry, Zapping is unable to create the config tree"));
      return FALSE;
    }

  /* Sets defaults for zconf */
  zcc_bool(TRUE, "Save and restore zapping geometry (non ICCM compliant)", 
	   "keep_geometry");
  zcc_bool(FALSE, "Resize by fixed increments", "fixed_increments");
  zcc_char(tveng_get_country_tune_by_id(0)->name,
	     "The country you are currently in", "current_country");
  current_country = 
    tveng_get_country_tune_by_name(zcg_char(NULL, "current_country"));
  zcc_char("/dev/video", "The device file to open on startup",
	   "video_device");
  zcc_bool(FALSE, "TRUE if Zapping should be started without sound",
	   "start_muted");
  zcc_bool(TRUE, "TRUE if some flickering should be avoided in preview mode",
	   "avoid_flicker");
  zcc_int(0, "Verbosity value given to zapping_setup_fb",
	  "zapping_setup_fb_verbosity");
  zcc_int(0, "Ratio mode", "ratio");
  zcc_int(0, "Current standard", "current_standard");
  zcc_int(0, "Current input", "current_input");
  zcc_int(TVENG_CAPTURE_WINDOW, "Current capture mode", "capture_mode");

  /* Loads all the tuned channels */
  while (zconf_get_nth(i, &buffer, ZCONF_DOMAIN "tuned_channels") !=
	 NULL)
    {
      g_assert(strlen(buffer) > 0);

      if (buffer[strlen(buffer)-1] == '/')
	buffer[strlen(buffer)-1] = 0;

      /* Get all the items from here  */
      buffer2 = g_strconcat(buffer, "/name", NULL);
      zconf_get_string(&new_channel.name, buffer2);
      g_free(buffer2);
      buffer2 = g_strconcat(buffer, "/real_name", NULL);
      zconf_get_string(&new_channel.real_name, buffer2);
      g_free(buffer2);
      buffer2 = g_strconcat(buffer, "/freq", NULL);
      zconf_get_integer(&new_channel.freq, buffer2);
      g_free(buffer2);
      buffer2 = g_strconcat(buffer, "/country", NULL);
      zconf_get_string(&new_channel.country, buffer2);
      g_free(buffer2);

      new_channel.index = 0;
      tveng_insert_tuned_channel(&new_channel);

      /* Free the previously allocated mem */
      g_free(new_channel.name);
      g_free(new_channel.real_name);
      g_free(new_channel.country);

      g_free(buffer);
      i++;
    }

  /* Starts all modules */
  if (!startup_callbacks())
    return FALSE;

  /* Start sound capturing */
  if (!startup_sound())
    return FALSE;
  
  /* Loads the modules */
  plugin_list = plugin_load_plugins();

  /* init them, and remove the ones that couldn't be inited */
 restart_loop:

  p = g_list_first(plugin_list);
  while (p)
    {
      plugin_load_config((struct plugin_info*)p->data);
      if (!plugin_init(main_info, (struct plugin_info*)p->data))
	{
	  plugin_unload((struct plugin_info*)p->data);
	  plugin_list = g_list_remove_link(plugin_list, p);
	  g_list_free_1(p);
	  goto restart_loop;
	}
      p = p->next;
    }

  si = sound_create_struct();

  /* Sync (more or less) the timestamps from the video and the audio */
  sound_start_timer();
  tveng_start_timer(main_info);

  return TRUE;
}
