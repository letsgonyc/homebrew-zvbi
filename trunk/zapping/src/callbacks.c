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
#include "osd.h"

/* set this flag to TRUE to exit the program */
extern volatile gboolean flag_exit_program;
extern GtkWidget *ToolBox; /* Control box, if any */
/* the mode set when we went fullscreen (used by main.c too) */
enum tveng_capture_mode restore_mode;

extern tveng_tuned_channel * global_channel_list;
extern tveng_device_info * main_info; /* About the device we are using */
extern gint disable_preview; /* TRUE if preview (fullscreen)
				    doesn't work */

int cur_tuned_channel = 0; /* Currently tuned channel */

extern GtkWidget * main_window; /* main Zapping window */

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
  zcc_bool(TRUE, "Show the Closed Caption", "closed_caption");
  cur_tuned_channel = zcg_int(NULL, "cur_tuned_channel");
  zcc_bool(FALSE, "Hide the extra controls", "hide_extra");
  zcc_int(0x1, "Subtitles page", "zvbi_page");

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
  zconf_set_integer(main_info->current_mode,
		    "/zapping/options/main/capture_mode");
  zmisc_switch_mode(TVENG_NO_CAPTURE, main_info);
}

void
on_exit2_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GtkWidget * widget = lookup_widget(GTK_WIDGET(menuitem), "zapping");
  GList * p;

  flag_exit_program = TRUE;

  UpdateCoords(widget->window);

  /* Tell the widget that the GUI is going to be closed */
  p = g_list_first(plugin_list);
  while (p)
    {
      plugin_remove_gui(GNOME_APP(widget), 
			(struct plugin_info*)p->data);
      p = p->next;
    }

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
on_hide_controls1_activate             (GtkMenuItem     *menuitem,
					gpointer         user_data)
{
  if (zcg_bool(NULL, "hide_controls"))
    {
      zcs_bool(FALSE, "hide_controls");
      gtk_widget_show(lookup_widget(main_window, "dockitem1"));
      gtk_widget_show(lookup_widget(main_window, "dockitem2")); 
      gtk_widget_queue_resize(main_window);

      z_change_menuitem(lookup_widget(GTK_WIDGET(main_window),
				      "hide_controls2"),
			GNOME_STOCK_PIXMAP_BOOK_YELLOW,
			_("Hide controls"),
			_("Hide the menu and the toolbar"));
    }
  else
    {
      zcs_bool(TRUE, "hide_controls");
      gtk_widget_hide(lookup_widget(main_window, "dockitem1"));
      gtk_widget_hide(lookup_widget(main_window, "dockitem2"));
      gtk_widget_queue_resize(main_window);

      z_change_menuitem(lookup_widget(GTK_WIDGET(main_window),
				      "hide_controls2"),
			GNOME_STOCK_PIXMAP_BOOK_OPEN,
			_("Show controls"),
			_("Show the menu and the toolbar"));
    }
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
      gtk_widget_show(lookup_widget(main_window, "frame6"));

      z_change_menuitem(lookup_widget(GTK_WIDGET(main_window),
				      "hide_menubars2"),
			GNOME_STOCK_PIXMAP_BOOK_BLUE,
			_("Hide extra controls"),
			_("Hide Input, Standards and subtitle selection"));
    }
  else
    {
      zcs_bool(TRUE, "hide_extra");
      gtk_widget_hide(lookup_widget(main_window, "Inputs"));
      gtk_widget_hide(lookup_widget(main_window, "Standards"));
      gtk_widget_hide(lookup_widget(main_window, "frame6"));

      z_change_menuitem(lookup_widget(GTK_WIDGET(main_window),
				      "hide_menubars2"),
			GNOME_STOCK_PIXMAP_BOOK_OPEN,
			_("Show extra controls"),
			_("Show Input, Standards and subtitle selection"));
    }
}

gboolean
on_zapping_delete_event                (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
  GList * p;

  flag_exit_program = TRUE;

  UpdateCoords(widget->window);

  /* Tell the widget that the GUI is going to be closed */
  p = g_list_first(plugin_list);
  while (p)
    {
      plugin_remove_gui(GNOME_APP(widget), 
			 (struct plugin_info*)p->data);
      p = p->next;
    }

  gtk_main_quit();

  return FALSE;
}

/* Activate a TV channel */
void on_channel_activate              (GtkMenuItem     *menuitem,
				       gpointer        user_data)
{
  gint num_channel = GPOINTER_TO_INT(user_data);

  tveng_tuned_channel * channel =
    tveng_retrieve_tuned_channel_by_index(num_channel, global_channel_list);

  if (!channel)
    {
      g_warning("Cannot tune given channel %d (no such channel)",
		num_channel);
      return;
    }

  z_switch_channel(channel, main_info);
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
on_closed_caption1_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GtkCheckMenuItem *button = GTK_CHECK_MENU_ITEM(menuitem);
  gboolean status = button->active;

  zcs_bool(status, "closed_caption");

  if (status)
    osd_on(lookup_widget(main_window, "tv_screen"), main_window);
  else
    osd_off();
}

void
on_videotext1_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  /* Stop any current capture mode, and start TTX */
  zmisc_switch_mode(TVENG_NO_CAPTURE, main_info);
}

void
on_vbi_info1_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  gtk_widget_show(build_vbi_info());
}

void
on_zvbi_page_changed		       (GtkSpinButton	*widget,
					gpointer	user_data)
{
  extern int zvbi_page;

  zvbi_page = dec2bcd(gtk_spin_button_get_value_as_int(widget));

  zcs_int(zvbi_page, "zvbi_page");
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
  int num_channels = tveng_tuned_channel_num(global_channel_list);
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
  int num_channels = tveng_tuned_channel_num(global_channel_list);
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

/* the returned string needs to be g_free'ed */
static gchar *
build_channel_tooltip(tveng_tuned_channel * tuned_channel)
{
  gchar * buffer;

  if ((!tuned_channel) || (!tuned_channel->accel_key) ||
      (tuned_channel->accel_key == GDK_VoidSymbol))
    return NULL;

  buffer = gdk_keyval_name(tuned_channel->accel_key);

  if (!buffer)
    return NULL;

  return g_strdup_printf("%s%s%s%s",
        	 (tuned_channel->accel_mask&GDK_CONTROL_MASK)?"Ctl+":"",
	       (tuned_channel->accel_mask&GDK_MOD1_MASK)?"Alt+":"",
	       (tuned_channel->accel_mask&GDK_SHIFT_MASK)?"Shift+":"",
	       buffer);

}

gboolean
on_tv_screen_button_press_event        (GtkWidget       *widget,
					GdkEvent        *event,
					gpointer        user_data)
{
  gint i;
  GtkWidget * zapping = lookup_widget(widget, "zapping");
  GdkEventButton * bevent = (GdkEventButton *) event;
  gchar * tooltip;

  if (event->type != GDK_BUTTON_PRESS)
    return FALSE;

  switch (bevent->button)
    {
    case 3:
      {
	GtkMenu * menu = GTK_MENU(create_popup_menu1());
	GtkWidget * menuitem;
	GtkMenu * submenu;
	tveng_tuned_channel * tuned;
	/* it needs to be realized before operating on it */
	gtk_widget_realize(GTK_WIDGET(menu));
	gtk_widget_hide(lookup_widget(GTK_WIDGET(menu), "channel_list1"));
	if (tveng_tuned_channel_num(global_channel_list) == 0)
	  {
	    menuitem = z_gtk_pixmap_menu_item_new(_("No tuned channels"),
						  GNOME_STOCK_PIXMAP_CLOSE);
	    gtk_widget_set_sensitive(menuitem, FALSE);
	    gtk_widget_show(menuitem);
	    gtk_menu_insert(menu, menuitem, 1);
	  }
	else
	  {
	    if (tveng_tuned_channel_num(global_channel_list) <= 7)
	      for (i = tveng_tuned_channel_num(global_channel_list)-1;
		   i >= 0; i--)
		{
		  tuned = tveng_retrieve_tuned_channel_by_index(i,
					global_channel_list);
		  g_assert(tuned != NULL);
		  menuitem =
		    z_gtk_pixmap_menu_item_new(tuned->name,
					       GNOME_STOCK_PIXMAP_PROPERTIES);
		  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
				     GTK_SIGNAL_FUNC(on_channel2_activate),
				     GINT_TO_POINTER(i));
		  gtk_object_set_user_data(GTK_OBJECT(menuitem), zapping);
		  tooltip = build_channel_tooltip(tuned);
		  if (tooltip)
		    {
		      set_tooltip(menuitem, tooltip);
		      g_free(tooltip);
		    }
		  gtk_widget_show(menuitem);
		  gtk_menu_insert(menu, menuitem, 1);
		}
	    else
	      {
		menuitem = lookup_widget(GTK_WIDGET(menu),
					 "channel_list1");
		gtk_widget_show(menuitem);
		submenu = GTK_MENU(gtk_menu_new());
		gtk_widget_show(GTK_WIDGET(submenu));
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem),
					  GTK_WIDGET(submenu));
		for (i = tveng_tuned_channel_num(global_channel_list)-1;
		     i >= 0; i--)
		  {
		    tuned = tveng_retrieve_tuned_channel_by_index(i,
				global_channel_list);
		    g_assert(tuned != NULL);
		    menuitem =
		      z_gtk_pixmap_menu_item_new(tuned->name,
					 GNOME_STOCK_PIXMAP_PROPERTIES);
		    gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
				       GTK_SIGNAL_FUNC(on_channel2_activate),
				       GINT_TO_POINTER(i));
		    gtk_object_set_user_data(GTK_OBJECT(menuitem),
					     zapping);
		    tooltip = build_channel_tooltip(tuned);
		    if (tooltip)
		      {
			set_tooltip(menuitem, tooltip);
			g_free(tooltip);
		      }
		    gtk_widget_show(menuitem);
		    gtk_menu_insert(submenu, menuitem, 1);
		  }
	      }
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
	  z_change_menuitem(lookup_widget(GTK_WIDGET(menu),
					  "hide_menubars1"),
			    GNOME_STOCK_PIXMAP_BOOK_OPEN,
			    _("Show extra controls"),
			    _("Show Input, Standards and subtitle selection"));

	if (zcg_bool(NULL, "hide_controls"))
	  z_change_menuitem(lookup_widget(GTK_WIDGET(menu),
					  "hide_controls1"),
			    GNOME_STOCK_PIXMAP_BOOK_OPEN,
			    _("Show controls"),
			    _("Show the menu and the toolbar"));

	process_ttxview_menu_popup(main_window, bevent, menu);
	gtk_menu_popup(menu, NULL, NULL, NULL,
		       NULL, bevent->button, bevent->time);
	gtk_object_set_user_data(GTK_OBJECT(menu), zapping);
      }
      return TRUE;
    case 4:
      on_channel_up1_activate(GTK_MENU_ITEM(lookup_widget(widget,
							  "channel_up1")),
			      NULL);
      return TRUE;
    case 5:
      on_channel_down1_activate(GTK_MENU_ITEM(lookup_widget(widget,
					      "channel_down1")),
				NULL);
      return TRUE;
    default:
      break;
    }
  return FALSE;
}

void
on_pal_big_activate		       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  gdk_window_resize(main_window->window, 768, 576);
}

void
on_rec601_pal_big_activate	       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  gdk_window_resize(main_window->window, 720, 576);
}

void
on_ntsc_big_activate		       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  gdk_window_resize(main_window->window, 640, 480);
}

void
on_pal_small_activate		       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  gdk_window_resize(main_window->window, 768/2, 576/2);
}

void
on_rec601_pal_small_activate	       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  gdk_window_resize(main_window->window, 704/2, 576/2);
}

void
on_ntsc_small_activate		       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  gdk_window_resize(main_window->window, 640/2, 480/2);
}
