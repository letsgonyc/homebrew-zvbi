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
/*
  This module adds sound support for Zapping. It uses esd's
  capabilities.
*/
#ifndef __SOUND_H__
#define __SOUND_H__

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

/* Start the sound, return FALSE on error */
gboolean startup_sound( void );

/* Shutdown all sound */
void shutdown_sound ( void);

#endif /* SOUND.H */
