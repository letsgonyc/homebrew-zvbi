/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000-2001 I�aki Garc�a Etxebarria
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

#ifndef __V4LINTERFACE_H__
#define __V4LINTERFACE_H__

#include "frequencies.h"
#include "zmodel.h"
#include "zmisc.h"

/* 
   Creates a control box suited for setting up all the controls this
   device can have
*/
GtkWidget * create_control_box(tveng_device_info * info);

/*
  Rebuilds the control box if it's open. Call whenever the device
  controls change.
*/
void
update_control_box(tveng_device_info * info);

/* Notification of standard/input changes */
extern ZModel *z_input_model;

/**
 * Sets the given input, based on its hash.
 */
void
z_switch_input			(int hash, tveng_device_info *info);

/**
 * Sets the given standard, based on its hash.
 */
void
z_switch_standard		(int hash, tveng_device_info *info);

/**
 * Sets the given channel.
 */
void
z_switch_channel		(tveng_tuned_channel	*channel,
				 tveng_device_info	*info);

gboolean
channel_key_press		(GdkEventKey *		event);

gboolean
on_channel_key_press		(GtkWidget *	widget,
				 GdkEventKey *	event,
				 gpointer	user_data);

/**
 * Sets the given channel.
 */
void
z_set_main_title		(tveng_tuned_channel	*channel,
				 gchar *default_name);
/**
 * Stores the current values of the known controls in the given
 * struct. num_controls and list are filled in appropiately.
 * used when saving 
 */
void
store_control_values		(gint		*num_controls,
				 tveng_tc_control **list,
				 tveng_device_info *info);

/* Returns whether something (useful) was added */
gboolean
add_channel_entries		(GtkMenu *menu,
				 gint pos,
				 gint menu_max_entries,
				 tveng_device_info *info);

/* Do the startup/shutdown */
void startup_v4linterface	(tveng_device_info *info);
void shutdown_v4linterface	(void);

/* XXX called by glade */
extern gboolean channel_up_cmd		(GtkWidget *	widget,
					 gint		argc,
					 gchar **	argv,
					 gpointer	user_data);
extern gboolean channel_down_cmd	(GtkWidget *	widget,
					 gint		argc,
					 gchar **	argv,
					 gpointer	user_data);

extern gdouble videostd_inquiry(void);

#endif