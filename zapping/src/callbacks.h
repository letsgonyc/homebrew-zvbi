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
#ifndef __CALLBACKS_H__
#define __CALLBACKS_H__

#include <gnome.h>

#include "tveng.h"
#include "frequencies.h"

/* Starts and stops callbacks */
gboolean startup_callbacks(void);
void shutdown_callbacks(void);

/******************************************************************************
 Stuff located in callbacks.c
******************************************************************************/
void
on_exit2_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_toggle_muted1_activate                     (GtkMenuItem     *menuitem,
				       gpointer         user_data);

void
on_about1_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_plugin_writing1_activate            (GtkMenuItem     *menuitem,
					gpointer         user_data);

void
on_main_help1_activate                 (GtkMenuItem     *menuitem,
					gpointer         user_data);

void
on_hide_controls1_activate             (GtkMenuItem     *menuitem,
					gpointer         user_data);

gboolean
on_zapping_delete_event                (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
on_zapping_configure_event             (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
on_tv_screen_configure_event           (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_controls_clicked                    (GtkButton       *button,
                                        gpointer         user_data);


gboolean
on_control_box_delete_event            (GtkWidget      *widget,
					GdkEvent       *event,
					gpointer        user_data);

void
on_go_fullscreen1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_closed_caption1_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_videotext1_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_vbi_info1_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_new_ttxview_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_go_capturing2_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_go_previewing2_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

gboolean
on_tv_screen_button_press_event        (GtkWidget       *widget,
					GdkEvent        *event,
					gpointer        user_data);

void
on_pal_big_activate		       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_rec601_pal_big_activate	       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_ntsc_big_activate		       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_pal_small_activate		       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_rec601_pal_small_activate	       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_ntsc_small_activate		       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

/******************************************************************************
 Stuff located in properties.c
******************************************************************************/

void
on_propiedades1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_zapping_properties_apply            (GnomePropertyBox *gnomepropertybox,
                                        gint             arg1,
                                        gpointer         user_data);

void
on_zapping_properties_help             (GnomePropertyBox *gnomepropertybox,
                                        gint             arg1,
                                        gpointer         user_data);

void
on_property_item_changed               (GtkWidget * changed_widget,
				        GnomePropertyBox *propertybox);

/******************************************************************************
 Stuff located in channel_editor.c
******************************************************************************/

void
on_channels1_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_channel_name_activate               (GtkWidget	*editable,
                                        gpointer         user_data);

void
on_cancel_channels_clicked             (GtkButton       *button,
                                        gpointer         user_data);

void
on_help_channels_clicked               (GtkButton       *button,
                                        gpointer         user_data);

void
on_channel_list_select_row             (GtkCList        *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_channels_done_clicked               (GtkButton       *button,
                                        gpointer         user_data);

void
on_add_channel_clicked                 (GtkButton       *button,
                                        gpointer         user_data);

void
on_add_all_channels_clicked            (GtkButton       *button,
                                        gpointer         user_data);

void
on_modify_channel_clicked              (GtkButton       *button,
                                        gpointer         user_data);

void
on_remove_channel_clicked              (GtkButton       *button,
                                        gpointer         user_data);

gboolean
on_channel_window_delete_event         (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_clist1_select_row                   (GtkCList        *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
on_channel_list_key_press_event        (GtkWidget       *widget,
                                        GdkEventKey     *event,
                                        gpointer         user_data);

void
on_channel_search_clicked              (GtkButton       *button,
                                        gpointer         user_data);

void
on_cancel_search_clicked               (GtkButton       *button,
                                        gpointer         user_data);

gboolean
on_searching_delete_event              (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

/******************************************************************************
 Stuff located in plugin_properties.c
******************************************************************************/
void
on_plugins1_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_plugin_list_select_row              (GtkCList        *clist,
					gint             row,
					gint             column,
					GdkEvent        *event,
					gpointer         user_data);

void
on_symbol_list_select_row              (GtkCList        *clist,
					gint             row,
					gint             column,
					GdkEvent        *event,
					gpointer         user_data);

void
on_button3_clicked                     (GtkButton       *button,
                                        gpointer         user_data);

void
on_plugin_start_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_plugin_stop_clicked                 (GtkButton       *button,
					gpointer         user_data);

void
on_plugin_help_clicked                 (GtkButton       *button,
					gpointer         user_data);

gboolean
on_plugin_properties_delete_event      (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_plugin_properties_apply             (GnomePropertyBox *gnomepropertybox,
                                        gint             arg1,
                                        gpointer         user_data);

void
on_plugin_properties_help              (GnomePropertyBox *gnomepropertybox,
                                        gint             arg1,
                                        gpointer         user_data);

#endif /* __CALLBACKS_H__ */
