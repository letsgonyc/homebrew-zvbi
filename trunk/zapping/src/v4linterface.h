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

#ifndef __V4LINTERFACE_H__
#define __V4LINTERFACE_H__

#include <gnome.h>
#include "tveng.h"
#include "callbacks.h"
#include "zmodel.h"

/* 
   Update the menu from where we can choose the standard. Widget is
   any widget in the same window as the standard menu (we lookup() it)
 */
void update_standards_menu(GtkWidget * widget, tveng_device_info *
			   info);

/* 
   Update the menu from where we can choose the input. Widget is
   any widget in the same window as the standard menu (we lookup() it)
 */
void update_inputs_menu(GtkWidget * widget, tveng_device_info *
			info);

/* 
   Update the menu from where we can choose the TV channel. Widget is
   any widget in the same window as the standard menu (we lookup() it)
 */
void update_channels_menu(GtkWidget* widget, tveng_device_info * info);

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
 * Sets the given input.
 */
void
z_switch_input			(struct tveng_enum_input *input,
				 tveng_device_info *info);

/**
 * Sets the given standard
 */
void
z_switch_standard		(struct tveng_enumstd *standard,
				 tveng_device_info *info);

/* Do the startup/shutdown */
void startup_v4linterface	(void);
void shutdown_v4linterface	(void);

#endif
