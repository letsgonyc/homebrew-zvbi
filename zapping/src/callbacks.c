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

#include "tveng.h"
#include "callbacks.h"
#include "interface.h"
#include "v4linterface.h"
#include "x11stuff.h"
/* Manages config values for zconf (it saves me some typing) */
#define ZCONF_DOMAIN "/zapping/internal/callbacks/"
#include "zconf.h"
#include "zmisc.h"
#include "plugins.h"
#include "zconf.h"
#include "zvbi.h"
#include "ttxview.h"

gboolean flag_exit_program; /* set this flag to TRUE to exit the program */
extern GtkWidget * ToolBox; /* Here is stored the Toolbox (if any) */
/* the mode set when we went fullscreen (used by main.c too) */
enum tveng_capture_mode restore_mode;

extern tveng_device_info * main_info; /* About the device we are using */
extern gboolean disable_preview; /* TRUE if preview (fullscreen)
				    doesn't work */

int cur_tuned_channel = 0; /* Currently tuned channel */

GtkWidget * main_window; /* main Zapping window */

extern GList * plugin_list; /* The plugins we have */

/* Starts and stops callbacks */
gboolean startup_callbacks(void)
{
  /* Init values to their defaults */
  zcc_int(30, "X coord of the Zapping window", "x");
  zcc_int(30, "Y coord of the Zapping window", "y");
  zcc_int(640, "Width of the Zapping window", "w");
  zcc_int(480, "Height of the Zapping window", "h");
  zcc_int(0, "Currently tuned channel", "cur_tuned_channel");
  cur_tuned_channel = zcg_int(NULL, "cur_tuned_channel");
  zcc_bool(FALSE, "Hide th extra controls", "hide_extra");

  return TRUE;
}

void shutdown_callbacks(void)
{
  zcs_int(cur_tuned_channel, "cur_tuned_channel");
}

/* Gets the geometry of the main window */
static void UpdateCoords(GdkWindow * window)
{
  int x, y, w, h;
  gdk_window_get_origin(window, &x, &y);
  gdk_window_get_size(window, &w, &h);
  
  zcs_int(x, "x");
  zcs_int(y, "y");
  zcs_int(w, "w");
  zcs_int(h, "h");
  zconf_set_integer(tveng_stop_everything(main_info),
		    "/zapping/options/main/capture_mode");
}

void
on_exit2_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GtkWidget * widget = lookup_widget(GTK_WIDGET(menuitem), "zapping");
  GList * p;
  UpdateCoords(widget->window);

  /* Tell the widget that the GUI is going to be closed */
  p = g_list_first(plugin_list);
  while (p)
    {
      plugin_remove_gui(GNOME_APP(widget), 
			(struct plugin_info*)p->data);
      p = p->next;
    }

  flag_exit_program = TRUE;
  gtk_main_quit();
}

void
on_about1_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  gtk_widget_show(create_about2());
}

void
on_plugin_writing1_activate            (GtkMenuItem     *menuitem,
					gpointer         user_data)
{
  GnomeHelpMenuEntry help_ref = { NULL,
				  "plugin_devel.html" };
  enum tveng_capture_mode cur_mode;

  cur_mode = tveng_stop_everything(main_info);

  help_ref.name = gnome_app_id;
  gnome_help_display (NULL, &help_ref);

  if (tveng_restart_everything(cur_mode, main_info) == -1)
    ShowBox(main_info->error, GNOME_MESSAGE_BOX_ERROR);
}

void
on_main_help1_activate                 (GtkMenuItem     *menuitem,
					gpointer         user_data)
{
  static GnomeHelpMenuEntry help_ref = { NULL,
					 "index.html" };
  enum tveng_capture_mode cur_mode;

  cur_mode = tveng_stop_everything(main_info);

  help_ref.name = gnome_app_id;
  gnome_help_display (NULL, &help_ref);

  if (tveng_restart_everything(cur_mode, main_info) == -1)
    ShowBox(main_info->error, GNOME_MESSAGE_BOX_ERROR);
}

void
on_hide_menubars1_activate             (GtkMenuItem     *menuitem,
					gpointer         user_data)
{
  if (zcg_bool(NULL, "hide_extra"))
    {
      zcs_bool(FALSE, "hide_extra");
      gtk_widget_show(lookup_widget(main_window, "Inputs"));
      gtk_widget_show(lookup_widget(main_window, "Standards")); 
    }
  else
    {
      zcs_bool(TRUE, "hide_extra");
      gtk_widget_hide(lookup_widget(main_window, "Inputs"));
      gtk_widget_hide(lookup_widget(main_window, "Standards"));
    }
}

gboolean
on_zapping_delete_event                (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
  GList * p;

  UpdateCoords(widget->window);

  /* Tell the widget that the GUI is going to be closed */
  p = g_list_first(plugin_list);
  while (p)
    {
      plugin_remove_gui(GNOME_APP(widget), 
			 (struct plugin_info*)p->data);
      p = p->next;
    }

  flag_exit_program = TRUE;
  gtk_main_quit();

  return FALSE;
}

/* Activate an standard */
void on_standard_activate              (GtkMenuItem     *menuitem,
					gpointer        user_data)
{
  gint std = GPOINTER_TO_INT(user_data);
  GtkWidget * main_window = lookup_widget(GTK_WIDGET(menuitem), "zapping");

  if (std == main_info->cur_standard)
    return; /* Do nothing if this standard is already active */

  if (tveng_set_standard_by_index(std, main_info) == -1) /* Set the
							 standard */
    {
      ShowBox(main_info -> error, GNOME_MESSAGE_BOX_ERROR);
      return;
    }

  /* redesign menus */
  update_standards_menu(main_window, main_info);
}

/* Activate an input */
void on_input_activate              (GtkMenuItem     *menuitem,
				     gpointer        user_data)
{
  gint input = GPOINTER_TO_INT (user_data);
  GtkWidget * main_window = lookup_widget(GTK_WIDGET(menuitem), "zapping");

  if (main_info -> cur_input == input)
    return; /* Nothing if no change is needed */

  if (tveng_set_input_by_index(input, main_info) == -1) /* Set the
							 input */
    {
      ShowBox(_("Cannot set input"), GNOME_MESSAGE_BOX_ERROR);
      return;
    }

  update_standards_menu(main_window, main_info);
}

/* Activate a TV channel */
void on_channel_activate              (GtkMenuItem     *menuitem,
				       gpointer        user_data)
{
  gint num_channel = GPOINTER_TO_INT(user_data);
  int mute = 0;

  tveng_tuned_channel * channel =
    tveng_retrieve_tuned_channel_by_index(num_channel); 

  if (!channel)
    {
      printf(_("Cannot tune given channel %d (no such channel)\n"), 
	     num_channel);
      return;
    }

  if (zconf_get_boolean(NULL, "/zapping/options/main/avoid_noise"))
    {
      mute = tveng_get_mute(main_info);
      
      if (!mute)
	tveng_set_mute(1, main_info);
    }

  if (tveng_tune_input(channel->freq, main_info) == -1) /* Set the
						       input freq*/
    ShowBox(_("Cannot tune input"), GNOME_MESSAGE_BOX_ERROR);

  if (zconf_get_boolean(NULL, "/zapping/options/main/avoid_noise"))
    {
      /* Sleep a little so the noise dissappears */
      usleep(100000);
      
      if (!mute)
	tveng_set_mute(0, main_info);
    }

  cur_tuned_channel = num_channel; /* Set the current channel to this */
}

void
on_channel2_activate                   (GtkMenuItem     *menuitem,
			       	        gpointer        user_data)
{
  GtkWidget * zapping =
    GTK_WIDGET(gtk_object_get_user_data(GTK_OBJECT(menuitem)));
  GtkWidget * Channels = lookup_widget(zapping, "Channels");

  gtk_option_menu_set_history(GTK_OPTION_MENU(Channels),
			      GPOINTER_TO_INT(user_data));
  on_channel_activate(NULL, user_data);
}

void
on_controls_clicked                    (GtkButton       *button,
                                        gpointer         user_data)
{
  if (ToolBox)
    {
      gdk_window_raise(ToolBox -> window);
      return;
    }

  ToolBox = create_control_box(main_info);

  gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);

  gtk_object_set_user_data(GTK_OBJECT(ToolBox), button);

  gtk_widget_show(ToolBox);
}

gboolean
on_control_box_delete_event            (GtkWidget      *widget,
					GdkEvent       *event,
					gpointer        user_data)
{
  GtkWidget * related_button;

  related_button =
    GTK_WIDGET(gtk_object_get_user_data(GTK_OBJECT(widget)));

  gtk_widget_set_sensitive(related_button, TRUE);

  ToolBox = NULL;

  return FALSE;
}

void
on_go_fullscreen1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  restore_mode = main_info->current_mode;

  zmisc_switch_mode(TVENG_CAPTURE_PREVIEW, main_info);
}

void
on_go_windowed1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  zmisc_switch_mode(restore_mode, main_info);
}

void
on_videotext1_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  /* Stop any current capture mode, and start TTX */
  zmisc_switch_mode(TVENG_NO_CAPTURE, main_info);
}

void
on_new_ttxview_activate		       (GtkMenuItem	*menuitem,
					gpointer	user_data)
{
  gtk_widget_show(build_ttxview());
}

void
on_go_capturing2_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  if (zmisc_switch_mode(TVENG_CAPTURE_READ, main_info) == -1)
    ShowBox(main_info->error, GNOME_MESSAGE_BOX_ERROR);
}

void
on_go_previewing2_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  if (zmisc_switch_mode(TVENG_CAPTURE_WINDOW, main_info) == -1)
    ShowBox(main_info->error, GNOME_MESSAGE_BOX_ERROR);
}

void
on_channel_up1_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  int num_channels = tveng_tuned_channel_num();
  GtkWidget * Channels = lookup_widget(GTK_WIDGET(menuitem),
					     "Channels");

  int new_channel;

  if (num_channels == 0) /* If there are no tuned channels stop
			    processing */
    return;

  new_channel = cur_tuned_channel - 1;
  if (new_channel < 0)
    new_channel = num_channels - 1;

  /* Simulate a callback */
  on_channel_activate(NULL, GINT_TO_POINTER(new_channel));
  
  /* Update the option menu */
  gtk_option_menu_set_history(GTK_OPTION_MENU (Channels),
			      new_channel);
}

void
on_channel_down1_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  int num_channels = tveng_tuned_channel_num();
  GtkWidget * Channels = lookup_widget(GTK_WIDGET(menuitem),
					     "Channels");

  int new_channel;

  if (num_channels == 0) /* If there are no tuned channels stop
			    processing */
    return;

  new_channel = cur_tuned_channel + 1;
  if (new_channel >= num_channels)
    new_channel = 0;

  /* Simulate a callback */
  on_channel_activate(NULL, GINT_TO_POINTER(new_channel));
  
  /* Update the option menu */
  gtk_option_menu_set_history(GTK_OPTION_MENU (Channels),
			      new_channel);
}

gboolean on_fullscreen_event (GtkWidget * widget, GdkEvent * event,
			      gpointer user_data)
{
  GtkWidget * window = GTK_WIDGET(user_data);
  GtkMenuItem * channel_up1 =
    GTK_MENU_ITEM(lookup_widget(window, "channel_up1"));
  GtkMenuItem * channel_down1 =
    GTK_MENU_ITEM(lookup_widget(window, "channel_down1"));
  GtkMenuItem * go_windowed1 =
    GTK_MENU_ITEM(lookup_widget(window, "channel_down1"));
  GtkMenuItem * exit2 =
    GTK_MENU_ITEM(lookup_widget(window, "exit2"));

  if (event->type == GDK_KEY_PRESS)
    {
      GdkEventKey * kevent = (GdkEventKey*) event;
      switch (kevent->keyval)
	{
	case GDK_Page_Up:
	case GDK_KP_Page_Up:
	  on_channel_up1_activate(channel_up1, NULL);
	  break;
	case GDK_Page_Down:
	case GDK_KP_Page_Down:
	  on_channel_down1_activate(channel_down1, NULL);
	  break;
	case GDK_Escape:
	  on_go_windowed1_activate(go_windowed1, NULL);
	  break;
	  /* Let control-Q exit the app */
	case GDK_q:
	  if (kevent->state & GDK_CONTROL_MASK)
	    {
	      extern gboolean was_fullscreen;
	      was_fullscreen = TRUE;
	      on_go_windowed1_activate(go_windowed1, NULL);
	      on_exit2_activate(exit2, NULL);
	    }
	  break;
	}
      return TRUE; /* Event processing done */
    }
  return FALSE; /* We aren't interested in this event, pass it on */
}

static void
change_pixmenuitem_label		(GtkWidget	*menuitem,
					 const gchar	*new_label)
{
  GtkWidget *widget = GTK_BIN(menuitem)->child;

  gtk_label_set_text(GTK_LABEL(widget), new_label);
}

gboolean
on_tv_screen_button_press_event        (GtkWidget       *widget,
					GdkEvent        *event,
					gpointer        user_data)
{
  gint i;
  GtkWidget * zapping = lookup_widget(widget, "zapping");
  GdkEventButton * bevent = (GdkEventButton *) event;
  GtkWidget *spixmap;

  if (event->type != GDK_BUTTON_PRESS)
    return FALSE;

  if (bevent->button == 3)
    {
      GtkMenu * menu = GTK_MENU(create_popup_menu1());
      GtkWidget * menuitem;
      tveng_tuned_channel * tuned;
      /* it needs to be realized before operating on it */
      gtk_widget_realize(GTK_WIDGET(menu));
      if ((main_info->num_inputs == 0) ||
	  (!(main_info->inputs[main_info->cur_input].flags &
	     TVENG_INPUT_TUNER)))
	{
	  menuitem = z_gtk_pixmap_menu_item_new(_("No tuner"),
						GNOME_STOCK_PIXMAP_CLOSE);
	  gtk_widget_set_sensitive(menuitem, FALSE);
	  gtk_widget_show(menuitem);
	  gtk_menu_insert(menu, menuitem, 1);
	}
      else if (tveng_tuned_channel_num() == 0)
	{
	  menuitem = z_gtk_pixmap_menu_item_new(_("No tuned channels"),
						GNOME_STOCK_PIXMAP_CLOSE);
	  gtk_widget_set_sensitive(menuitem, FALSE);
	  gtk_widget_show(menuitem);
	  gtk_menu_insert(menu, menuitem, 1);
	}
      else
	for (i = tveng_tuned_channel_num()-1; i >= 0; i--)
	  {
	    tuned = tveng_retrieve_tuned_channel_by_index(i);
	    g_assert(tuned != NULL);
	    menuitem =
	      z_gtk_pixmap_menu_item_new(tuned->name,
					 GNOME_STOCK_PIXMAP_PROPERTIES);
	    gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			       GTK_SIGNAL_FUNC(on_channel2_activate),
			       GINT_TO_POINTER(i));
	    gtk_object_set_user_data(GTK_OBJECT(menuitem), zapping);
	    gtk_widget_show(menuitem);
	    gtk_menu_insert(menu, menuitem, 1);
	  }
      if (disable_preview)
	{
	  widget = lookup_widget(GTK_WIDGET(menu), "go_fullscreen2");
	  gtk_widget_set_sensitive(widget, FALSE);
	  gtk_widget_hide(widget);
	  widget = lookup_widget(GTK_WIDGET(menu), "go_previewing2");
	  gtk_widget_set_sensitive(widget, FALSE);
	  gtk_widget_hide(widget);
	}
      if (!zvbi_get_object())
	{
	  widget = lookup_widget(GTK_WIDGET(menu), "separador6");
	  gtk_widget_set_sensitive(widget, FALSE);
	  gtk_widget_hide(widget);
	  widget = lookup_widget(GTK_WIDGET(menu), "videotext2");
	  gtk_widget_set_sensitive(widget, FALSE);
	  gtk_widget_hide(widget);
	  widget = lookup_widget(GTK_WIDGET(menu), "new_ttxview2");
	  gtk_widget_set_sensitive(widget, FALSE);
	  gtk_widget_hide(widget);
	}
      /* Remove capturing item if it's redundant */
      if ((!zvbi_get_object()) && (disable_preview))
	{
	  gtk_widget_hide(lookup_widget(GTK_WIDGET(menu),
					"separador3"));
	  widget = lookup_widget(GTK_WIDGET(menu), "go_capturing2");
	  gtk_widget_set_sensitive(widget, FALSE);
	  gtk_widget_hide(widget);
	}
      if (zcg_bool(NULL, "hide_extra"))
	{
	  widget = lookup_widget(GTK_WIDGET(menu), "hide_menubars1");
	  change_pixmenuitem_label(widget, _("Show extra controls"));
	  set_tooltip(widget,
		      _("Show the Inputs and Standards menu"));
	  spixmap =
	    gnome_stock_pixmap_widget_at_size(widget,
					      GNOME_STOCK_PIXMAP_BOOK_OPEN,
					      16, 16);
	  /************* THIS SHOULD NEVER BE DONE ***********/
	  gtk_object_destroy(GTK_OBJECT(GTK_PIXMAP_MENU_ITEM(widget)->pixmap));
	  GTK_PIXMAP_MENU_ITEM(widget)->pixmap = NULL;
	  /********** BUT THERE'S NO OTHER WAY TO DO IT ******/
	  gtk_pixmap_menu_item_set_pixmap(GTK_PIXMAP_MENU_ITEM(widget),
					  spixmap);
	  gtk_widget_show(spixmap);
	}
      gtk_menu_popup(menu, NULL, NULL, NULL,
		     NULL, bevent->button, bevent->time);
      gtk_object_set_user_data(GTK_OBJECT(menu), zapping);
      return TRUE;
    }
  return FALSE;
}


