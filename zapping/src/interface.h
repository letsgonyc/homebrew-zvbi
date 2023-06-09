/*
 *  Zapping (TV viewer for the Gnome Desktop)
 *
 * Copyright (C) 2001 I�aki Garc�a Etxebarria
 * Copyright (C) 2003 Michael H. Schimek
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

/* $Id: interface.h,v 1.20 2007-08-30 14:14:34 mschimek Exp $ */

#ifndef INTERFACE_H
#define INTERFACE_H

#include <gtk/gtk.h>

extern GtkWidget *
find_widget			(GtkWidget *		parent,
				 const gchar *		name);
extern GtkWidget *
lookup_widget			(GtkWidget *		parent,
				 const gchar *		name);
extern void
register_widget			(GtkWidget *		parent,
				 GtkWidget *		widget,
				 const char *		name);
extern GtkWidget *
build_widget			(const gchar *		name,
				 const gchar *		file);

#endif /* INTERFACE_H */

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
