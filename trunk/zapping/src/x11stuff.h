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
#ifndef __X11STUFF_H__
#define __X11STUFF_H__

/*
 * the routines contained here are all the X-specific stuff in Zapping
 */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <gtk/gtk.h>

#include <tveng.h>

/*
 * Returns a pointer to the data contained in the given GdkImage
 */
gpointer
x11_get_data(GdkImage * image);

/*
 * Returns the byte order of the X server
 */
GdkByteOrder
x11_get_byte_order(void);

/*
 * Returns the bits per pixel needed for a GdkImage. -1 on error.
 */
gint
x11_get_bpp(void);

/*
 * Returns a pointer to a clips array (that you need to free()
 * afterwards if not NULL).
 * Pass the GdkWindow that you want to get the clip status of.
 * num_clips get filled with the number of clips in the array.
 */
struct tveng_clip *
x11_get_clips(GdkWindow * win, gint x, gint y, gint w, gint h, gint *
	      num_clips);

/*
 * Maps and unmaps a window of the given (screen) geometry, thus
 * forcing an expose event in that area
 */
void
x11_force_expose(gint x, gint y, gint w, gint h);

/*
 * Returns TRUE if the window is viewable
 */
gboolean
x11_window_viewable(GdkWindow *window);

/*
 * Sets the X screen saver on/off
 */
void
x11_set_screensaver(gboolean on);

/**
 * XvImage handling (SHM, etc)
 */
typedef struct {
  gint			w, h;
  gpointer		data;
  gint			data_size; /* in bytes */
  struct _xvzImagePrivate *private; /* X-related data, not interesting */
} xvzImage;

/**
 * Create a new XV image with the given attributes, returns NULL on error.
 */
xvzImage * xvzImage_new(enum tveng_frame_pixformat pixformat,
			gint width, gint height);

/**
 * Puts the image in the given drawable, scales to the drawable's size.
 */
void xvzImage_put(xvzImage *image, GdkWindow *window, GdkGC *gc);

/**
 * Frees the data associated with the image
 */
void xvzImage_destroy(xvzImage *image);

/**
 * Tries to grab a port for displaying the xvzImages. FALSE on error.
 * Must be called before all the rest of xvz routines, or they won't
 * work.
 */
gboolean xvz_grab_port(tveng_device_info *info);

/**
 * Ungrabs any previously grabbed port.
 */
void xvz_ungrab_port(tveng_device_info *info);

/**
 * Inits the video backends.
 */
void startup_xvz(void);

/**
 * Closes the video backends.
 */
void shutdown_xvz(void);

/* some useful constants */
#ifndef OFF
#define OFF FALSE
#endif
#ifndef ON
#define ON TRUE
#endif

/**
 * Struct for video backends
 */
typedef struct {
  /* A descriptive name for the backend */
  char		*name;
  /* see xvz_grab_port, open devices in here, FALSE on error */
  gboolean	(*grab)(tveng_device_info *info);
  /* see xvz_ungrab_port, close devices */
  void		(*ungrab)(tveng_device_info *info);
  /* Create a suitable image, will always be called with the port
     grabbed */
  xvzImage*	(*image_new)(enum tveng_frame_pixformat pixformat,
			     gint width, gint height);
  /* Destroy the given image */
  void		(*image_destroy)(xvzImage *image);
  /* Put the image in the drawable, do scaling as necessary */
  void		(*image_put)(xvzImage *image, GdkWindow *window,
			     GdkGC *gc);
} video_backend;

extern void (* window_on_top)(GtkWidget *widget, gboolean on);
extern int wm_detect (void);

#endif /* x11stuff.h */
