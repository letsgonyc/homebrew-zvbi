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

//#include <tveng.h>

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

/* Keep-window-on-top routines */

extern void
(* window_on_top)		(GtkWindow *window, gboolean on);
extern gboolean
wm_hints_detect			(void);

/* VidMode routines for fullscreen mode */

typedef struct _x11_vidmode_info x11_vidmode_info;

struct _x11_vidmode_info {
  x11_vidmode_info *	next;

  unsigned int		width;
  unsigned int		height;
  double		hfreq;		/* Hz */
  double		vfreq;		/* Hz */
  double		aspect;		/* pixel y/x */
};

typedef struct _x11_vidmode_state x11_vidmode_state;

struct _x11_vidmode_state {
  /* <Private> */
  struct {
    x11_vidmode_info *	  vm;
    struct {
      int		    x;
      int		    y;
    }			  pt, vp;
  }			old, new;
};

extern void
x11_vidmode_list_delete		(x11_vidmode_info *	list);
extern x11_vidmode_info *
x11_vidmode_list_new		(void);
extern x11_vidmode_info *
x11_vidmode_by_name		(x11_vidmode_info *	list,
				 const gchar *		name);
extern x11_vidmode_info *
x11_vidmode_current		(x11_vidmode_info *	list);
extern void
x11_vidmode_clear_state		(x11_vidmode_state *	vs);
extern gboolean
x11_vidmode_switch		(x11_vidmode_info *	list,
				 x11_vidmode_info *	vm,
				 x11_vidmode_state *	vs);
extern void
x11_vidmode_restore		(x11_vidmode_info *	list,
				 x11_vidmode_state *	vs);

/* Screensaver routines */

#define X11_SCREENSAVER_ON		0
#define X11_SCREENSAVER_DISPLAY_ACTIVE	(1 << 0) /* for overlay modes */
#define X11_SCREENSAVER_CPU_ACTIVE	(1 << 1) /* for capture modes */

extern void
x11_screensaver_set		(unsigned int		level);
extern void
x11_screensaver_control		(gboolean		enable);
extern void
x11_screensaver_init		(void);

/* DGA routines */

typedef struct _x11_dga_parameters x11_dga_parameters;

struct _x11_dga_parameters {
  void *		base;			/* frame buffer */
  unsigned int		size;			/* 2048 * 768, bytes */

  unsigned int		width;			/* 1024 */
  unsigned int		height;			/* 768 */

  /* XXX rgb1? 1rgb? 1bgr? bgr1? le/be? */
  unsigned int		depth;			/* 15 */

  unsigned int		bytes_per_line;		/* 2048 */
  unsigned int		bits_per_pixel;		/* 16 */
};

gboolean
x11_dga_query			(x11_dga_parameters *	par,
				 const char *		display_name,
				 int			bpp_hint);

static __inline__ gboolean
x11_dga_present			(x11_dga_parameters *	par)
{
  return par->base != 0;
}

#endif /* x11stuff.h */
