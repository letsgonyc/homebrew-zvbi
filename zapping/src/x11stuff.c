/*
 * Zapping (TV viewer for the Gnome Desktop)
 *
 * Copyright (C) 2000-2001 I�aki Garc�a Etxebarria
 * Copyright (C) 2002-2003 Michael H. Schimek
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
 *
 * Some of this code is based on the xscreensaver source code:
 * Copyright (c) 1991-1998 by Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/*
 * the routines contained here are all the X-specific stuff in Zapping
 */

#include "site_def.h"
#include "config.h"

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>

#define ZCONF_DOMAIN "/zapping/options/main/"
#include "zconf.h"
#include "x11stuff.h"
#include "zmisc.h"
#include "globals.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>


/*
 * Returns a pointer to the data contained in the given GdkImage
 */
gpointer
x11_get_data(GdkImage * image)
{
  return (image -> mem);
}

/*
 * Returns the byte order of the X server
 */
GdkByteOrder
x11_get_byte_order(void)
{
  if (ImageByteOrder(GDK_DISPLAY()) == LSBFirst)
    return GDK_LSB_FIRST;

  return GDK_MSB_FIRST;
}

/*
 * Returns the bits per pixel needed for a GdkImage. -1 on error.
 */
gint
x11_get_bpp(void)
{
  GdkImage * tmp_image;
  static gint depth = -1;

  if (depth > 0)
    return depth;

  tmp_image =
    gdk_image_new(GDK_IMAGE_FASTEST, gdk_visual_get_system(), 16, 16);

  if (!tmp_image)
    return -1;

  depth = tmp_image->depth;

  if (depth == 24 &&
      tmp_image->bpp == 4)
    depth = 32;

  g_object_unref (G_OBJECT (tmp_image));

  return depth;
}

/* to be replaced by 2.2 gdk_display_get_name() */
const gchar *
x11_display_name (void)
{
  return (const gchar *) DisplayString (GDK_DISPLAY());
}



/*
 * Maps and unmaps a window of the given (screen) geometry, thus
 * forcing an expose event in that area
 */
void
x11_force_expose(gint x, gint y, guint w, guint h)
{
  XSetWindowAttributes xswa;
  Window win;
  unsigned long mask;

  xswa.override_redirect = TRUE;
  xswa.backing_store = NotUseful;
  xswa.save_under = FALSE;
  mask = ( CWSaveUnder | CWBackingStore | CWOverrideRedirect );

  win = XCreateWindow(GDK_DISPLAY(), GDK_ROOT_WINDOW(), x, y, w, h, 
		      0, CopyFromParent, InputOutput, CopyFromParent,
		      mask, &xswa);

  XMapWindow(GDK_DISPLAY(), win);
  XUnmapWindow(GDK_DISPLAY(), win);

  XDestroyWindow(GDK_DISPLAY(), win);
}

#if 0
static void
_x11_force_expose(gint x, gint y, gint w, gint h)
{
  Display *dpy = GDK_DISPLAY();
  XWindowAttributes wts;
  Window root, rroot, parent, *children;
  uint nchildren, i;
  XErrorHandler olderror;
  XExposeEvent event;

  event.type = Expose;
  event.count = 0;

  root=GDK_ROOT_WINDOW();
  XQueryTree(dpy, root, &rroot, &parent, &children, &nchildren);
    
  /* enter error-ignore mode */
  olderror = XSetErrorHandler(xerror);
  for (i=0; i<nchildren; i++) {
    XGetWindowAttributes(dpy, children[i], &wts);
    if (!(wts.map_state & IsViewable))
      continue;
    if (wts.class != InputOutput)
      continue;
    if ((wts.x >= x+w) || (wts.y >= x+h) ||
    	(wts.x+wts.width < x) || (wts.y+wts.height < y))
      continue;
    
    event.window = children[i];
    event.x = 0;
    event.y = 0;
    event.width = wts.width;
    event.height = wts.height;
    XSendEvent(GDK_DISPLAY(), children[i], False,
  	       ExposureMask, (XEvent*)&event);
  }
  XSync (GDK_DISPLAY(), False);
  XSetErrorHandler(olderror);
  /* leave error-ignore mode */

  if (children)
    XFree((char *) children);
}
#endif

/*
 * Returns TRUE if the window is viewable
 */
gboolean
x11_window_viewable(GdkWindow *window)
{
  XWindowAttributes wts;

  XGetWindowAttributes(GDK_DISPLAY(), GDK_WINDOW_XWINDOW(window), &wts);

  return ((wts.map_state & IsViewable) ? TRUE : FALSE);
}



/*
	Window property & event helpers
*/

/* FIXME not reentrant, add mutex or someth */
static XErrorHandler	old_error_handler	= 0;
static Bool		bad_window		= False;

static int
bad_window_handler		(Display *		display,
				 XErrorEvent *		error)
{
  if (error->error_code == BadWindow)
    bad_window = True;
  else if (old_error_handler)
    return old_error_handler (display, error);

  return 0;
}

static int
get_window_property		(Display *		display,
				 Window			window,
				 Atom			property,
				 Atom			req_type,
				 unsigned long *	nitems_return,
				 void **		prop_return)
{
  int status;
  Atom type;
  int format;
  unsigned long bytes_after;

  bad_window = False;
  
  status = XGetWindowProperty (display, window, property,
                    	       0, (65536 / sizeof (long)),
			       False, req_type,
			       &type, &format,
			       nitems_return, &bytes_after,
			       (unsigned char **) prop_return);
  if (bad_window)
    {
      status = BadWindow;
    }
  else if (status == Success)
    {
      if (type == None)
        status = BadAtom;
    }

  return status;
}

static void
send_event			(Display *		display,
				 Window			window,
				 Atom			message_type,
				 long			l0,
				 long			l1)
{
  XEvent xev;
  int status;

  CLEAR (xev);

  xev.type = ClientMessage;
  xev.xclient.display = display;
  xev.xclient.window = window;
  xev.xclient.send_event = TRUE;
  xev.xclient.message_type = message_type;
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = l0;
  xev.xclient.data.l[1] = l1;

  status = XSendEvent (display, DefaultRootWindow (display),
		       False,
		       SubstructureNotifyMask |
		       SubstructureRedirectMask,
		       &xev);
  if (0)
    fprintf (stderr, "%d = XSendEvent(message_type=%u l0=%ld l1=%ld)\n",
	     status, (unsigned int) message_type, l0, l1);

  XSync (display, False);
}

static void
gtk_window_send_x11_event	(GtkWindow *		window,
				 Atom			message_type,
				 long			l0,
				 long			l1)
{
  GdkWindow *toplevel;

  if (window->frame)
    toplevel = window->frame;
  else
    toplevel = GTK_WIDGET (window)->window;

  g_assert (toplevel != NULL);
  g_assert (GTK_WIDGET_MAPPED (window));

  send_event (GDK_WINDOW_XDISPLAY (toplevel),
	      GDK_WINDOW_XWINDOW (toplevel),
	      message_type,
	      l0, l1);
}

/*
	WindowManager hints for stay-on-top option
*/

/* A function not directly supported by Gnome/Gtk+: Tell the window manager
   to keep our (video) window above all other windows. */

#ifndef X11STUFF_WM_HINTS_DEBUG
#define X11STUFF_WM_HINTS_DEBUG 1
#endif

/*
   Tested:		proto
   IceWM 1.2.2		Gnome
   Sawfish 1.2		NetWM
   Metacity 2.6.3	NetWM
*/

static void
dummy_window_something		(GtkWindow *		window _unused_,
				 gboolean		on _unused_)
{ }

/**
 * window_on_top:
 * @window:
 * @on:
 *
 * Tell the WM to keep the window on top of other windows.
 * You must call wm_hints_detect () and gtk_widget_show (window) first.
 */
void (* x11_window_on_top)	(GtkWindow *		window,
				 gboolean		on)
  = dummy_window_something;

/**
 * window_fullscreen:
 * @window:
 * @on:
 *
 * Tell the WM to display the window on top of other windows,
 * maximized, without decoration.
 * You must call wm_hints_detect () and gtk_widget_show (window) first.
 */
void (* x11_window_fullscreen)	(GtkWindow *		window,
				 gboolean		on)
  = dummy_window_something;

/**
 * window_background:
 * @window:
 * @on:
 *
 * Tell the WM to keep the window below most other windows.
 * You must call wm_hints_detect () and gtk_widget_show (window) first.
 */
void (* x11_window_below)	(GtkWindow *		window,
				 gboolean		on)
  = dummy_window_something;

static GdkFilterReturn
wm_event_handler		(GdkXEvent *		xevent _unused_,
				 GdkEvent *		event _unused_,
				 gpointer		data _unused_)
{
  return GDK_FILTER_REMOVE; /* ignore */
}

/* www.freedesktop.org/standards */

enum {
  _NET_WM_STATE_REMOVE,
  _NET_WM_STATE_ADD,
  _NET_WM_STATE_TOGGLE
};

static Atom _XA_NET_SUPPORTED;
static Atom _XA_NET_WM_STATE;
static Atom _XA_NET_WM_STATE_ABOVE;
static Atom _XA_NET_WM_STATE_BELOW;
static Atom _XA_NET_WM_STATE_FULLSCREEN;

static void
net_wm_window_on_top		(GtkWindow *		window,
				 gboolean		on)
{
  gtk_window_send_x11_event (window,
			     _XA_NET_WM_STATE,
			     on ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE,
			     _XA_NET_WM_STATE_ABOVE);
}

static void
net_wm_fullscreen		(GtkWindow *		window,
				 gboolean		on)
{
  gtk_window_send_x11_event (window,
			     _XA_NET_WM_STATE,
			     on ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE,
			     _XA_NET_WM_STATE_FULLSCREEN);
}

static void
net_wm_below			(GtkWindow *		window,
				 gboolean		on)
{
  gtk_window_send_x11_event (window,
			     _XA_NET_WM_STATE,
			     on ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE,
			     _XA_NET_WM_STATE_BELOW);
}

enum {
  WIN_LAYER_BELOW = 2,
  WIN_LAYER_NORMAL = 4,
  WIN_LAYER_ONTOP = 6,
  WIN_LAYER_DOCK = 8,
  WIN_LAYER_ABOVE_DOCK = 10
};

static Atom _XA_WIN_SUPPORTING_WM_CHECK;
static Atom _XA_WIN_LAYER;

static void
gnome_window_on_top		(GtkWindow *		window,
				 gboolean		on)
{
  gtk_window_send_x11_event (window,
			     _XA_WIN_LAYER,
			     on ? WIN_LAYER_ONTOP : WIN_LAYER_NORMAL,
			     0);
}

static void
gnome_fullscreen		(GtkWindow *		window,
				 gboolean		on)
{
  gtk_window_set_decorated (window, FALSE);
  gtk_window_send_x11_event (window,
			     _XA_WIN_LAYER,
			     on ? WIN_LAYER_ABOVE_DOCK : WIN_LAYER_NORMAL,
			     0);
}

static void
gnome_below			(GtkWindow *		window,
				 gboolean		on)
{
  gtk_window_send_x11_event (window,
			     _XA_WIN_LAYER,
			     on ? WIN_LAYER_BELOW : WIN_LAYER_NORMAL,
			     0);
}

/**
 * wm_hints_detect:
 * Check if we can tell the WM to keep a window on top of other windows.
 * This must be called before window_on_top().
 *
 * @returns:
 * TRUE if possible.
 */
gboolean
wm_hints_detect			(void)
{
  Display *display;
  Window root;

  display = gdk_x11_get_default_xdisplay ();
  g_assert (display != 0);

  root = DefaultRootWindow (display);

  _XA_NET_SUPPORTED		= XInternAtom (display, "_NET_SUPPORTED", False);
  _XA_NET_WM_STATE		= XInternAtom (display, "_NET_WM_STATE", False);
  _XA_NET_WM_STATE_FULLSCREEN	= XInternAtom (display, "_NET_WM_STATE_FULLSCREEN", False);
  _XA_NET_WM_STATE_ABOVE	= XInternAtom (display, "_NET_WM_STATE_ABOVE", False);
  _XA_NET_WM_STATE_BELOW	= XInternAtom (display, "_NET_WM_STATE_BELOW", False);

  /* Netwm compliant */

  {
    Atom *atoms;
    unsigned long nitems;

    atoms = NULL;

    if (Success == get_window_property (display, root, _XA_NET_SUPPORTED,
					AnyPropertyType, &nitems,
					(void **) &atoms))
      {
	unsigned int i;

	printv ("WM supports _NET_SUPPORTED (%lu)\n", nitems);

	for (i = 0; i < nitems; i++)
	  {
	    /* atoms[i] != _XA_... */
	    char *atom_name = XGetAtomName (display, atoms[i]);

	    if (X11STUFF_WM_HINTS_DEBUG)
	      printv ("  atom %s\n", atom_name);

	    /* E.g. IceWM 1.2.2 _NET yes, _ABOVE no, but _LAYER */
	    if (0 == strcmp (atom_name, "_NET_WM_STATE_ABOVE"))
	      {
		XFree (atom_name);
		XFree (atoms);

		x11_window_on_top = net_wm_window_on_top;
		x11_window_fullscreen = net_wm_fullscreen;
		x11_window_below = net_wm_below;

		gdk_add_client_message_filter
		  (gdk_x11_xatom_to_atom (_XA_NET_WM_STATE),
		   wm_event_handler, 0);

		return TRUE;
	      }

	    XFree (atom_name);
	  }

	XFree (atoms);

	printv ("  ... nothing useful\n");
      }

    _XA_WIN_SUPPORTING_WM_CHECK	=
      XInternAtom (display, "_WIN_SUPPORTING_WM_CHECK", False);
    _XA_WIN_LAYER		=
      XInternAtom (display, "_WIN_LAYER", False);
  }

  /* Gnome compliant */

  {
    void *args;
    unsigned long nitems;

    args = NULL;

    if (Success == get_window_property (display, root,
					_XA_WIN_SUPPORTING_WM_CHECK,
					AnyPropertyType, &nitems, &args))
      if (nitems > 0)
	{
	  printv ("WM supports _WIN_SUPPORTING_WM_CHECK\n");

	  /* FIXME: check capabilities */

	  XFree (args);

	  x11_window_on_top = gnome_window_on_top;
	  x11_window_fullscreen = gnome_fullscreen;
	  x11_window_below = gnome_below;

	  gdk_add_client_message_filter
	    (gdk_x11_xatom_to_atom (_XA_WIN_LAYER),
	     wm_event_handler, 0);

	  return TRUE;
	}
  }

  printv ("No WM hints\n");

  return FALSE;
}



/*
	XFree86VidMode helpers
*/

/* This is used to select a suitable screen resolution for fullscreen
   video display. I.e. instead of scaling the video we can switch to
   TV-like monitor frequencies ("Modeline" in XF86Config). */

#ifndef X11STUFF_VIDMODE_DEBUG
#define X11STUFF_VIDMODE_DEBUG 0
#endif

/**
 * x11_vidmode_clear_state:
 * @vs:
 */
void
x11_vidmode_clear_state		(x11_vidmode_state *	vs)
{
  if (!vs)
    return;

  vs->_old.vm = NULL;

  vs->_new.vm = NULL;		/* don't restore */
  vs->_new.pt.x = 1 << 20;	/* implausible, don't restore */
  vs->_new.vp.x = 1 << 20;
}

#ifdef HAVE_VIDMODE_EXTENSION

#include <X11/extensions/xf86vmode.h>

struct vidmode {
  x11_vidmode_info	pub;
  Display *		display;
  int			screen_number;
  XF86VidModeModeInfo	info;
};

#define VIDMODE(p) PARENT (p, struct vidmode, pub)
#define CVIDMODE(p) CONST_PARENT (p, struct vidmode, pub)

/**
 * x11_vidmode_list_delete:
 * @list:
 *
 * Deletes a list of struct x11_vidmode_info as returned
 * by x11_vidmode_list_new().
 **/
void
x11_vidmode_list_delete		(x11_vidmode_info *	list)
{
  struct vidmode *v;

  while (list)
    {
      v = VIDMODE (list);
      list = v->pub.next;
      free (v);
    }
}

/**
 * x11_vidmode_list_new:
 * Creates a sorted list of selectable VidModes, i.e. all valid
 * Modelines in XF86Config including those switchable with kp+
 * and kp-. The list must be freed with x11_vidmode_list_delete().
 * Result can be NULL for various reasons.
 */
x11_vidmode_info *
x11_vidmode_list_new		(const char *		display_name,
				 int			screen_number)
{
  Display *display;
  int event_base;
  int error_base;
  int major_version;
  int minor_version;
  XF86VidModeModeInfo **mode_info;
  int mode_count;
  x11_vidmode_info *list;
  int i;

  if (display_name)
    {
      display = XOpenDisplay (display_name);

      if (NULL == display)
	{
	  printv ("%s: Cannot open display '%s'\n",
		  __FUNCTION__, display_name);
	  return NULL;
	}
    }
  else
    {
      display = GDK_DISPLAY ();
    }

  if (-1 == screen_number)
    screen_number = XDefaultScreen (display);

  if (!XF86VidModeQueryExtension (display, &event_base, &error_base))
    {
      printv ("XF86VidMode extension not available\n");
      return NULL;
    }

  if (!XF86VidModeQueryVersion (display, &major_version, &minor_version))
    {
      printv ("XF86VidMode extension not usable\n");
      return NULL;
    }

  printv ("XF86VidMode base %d, %d, version %d.%d\n",
	  event_base, error_base,
	  major_version, minor_version);

  if (2 != major_version)
    {
      printv ("Unknown XF86VidMode version\n");
      return NULL;
    }

  /* This lists all ModeLines in XF86Config and all default modes,
     except those exceeding monitor limits and the virtual screen size,
     as logged in /var/log/X[Free86]*.log */
  if (!XF86VidModeGetAllModeLines (display, screen_number,
				   &mode_count, &mode_info))
    {
      if (X11STUFF_VIDMODE_DEBUG)
	printv ("No mode lines\n");
      return NULL;
    }

  list = NULL;

  for (i = 0; i < mode_count; i++)
    {
      XF86VidModeModeInfo *m = mode_info[i];
      struct vidmode *v;
      Status valid;

      if (m->privsize > 0)
	{
	  XFree (m->private);
	  m->private = NULL;
	  m->privsize = 0;
	}

      valid = XF86VidModeValidateModeLine (display, screen_number, m);

#if 0
      /* For some reason this flags modes as invalid which actually
         have been accepted at X server startup and can be selected
         with Ctrl-Alt-nk+/-. x11_vidmode_switch() needs all
	 selectable modes listed, so we ignore this. */
      if (Success == valid)
#else
      if (1)
#endif
	{
	  /* XXX flags: see xvidtune.c; special treatment of
	     V_INTERLACE/V_DBLSCAN necessary? */

	  if (X11STUFF_VIDMODE_DEBUG)
	    printv ("Valid mode dot=%u flags=%08x "
		    "hd=%u hss=%u hse=%u ht=%u "
		    "vd=%u vss=%u vse=%u vt=%u valid=%d\n",
		    m->dotclock, m->flags,
		    m->hdisplay, m->hsyncstart, m->hsyncend, m->htotal,
		    m->vdisplay, m->vsyncstart, m->vsyncend, m->vtotal,
		    valid);

	  if ((v = calloc (1, sizeof (*v))))
	    {
	      unsigned int dothz = m->dotclock * 1000;
	      x11_vidmode_info *vv, **vvv;

	      v->pub.width	= m->hdisplay;
	      v->pub.height	= m->vdisplay;
	      v->pub.hfreq	= dothz / (double) m->htotal;
	      v->pub.vfreq	= dothz / (double)(m->htotal * m->vtotal);
	      v->pub.aspect	= 1.0; /* Sigh. */

	      v->display	= display;
	      v->screen_number	= screen_number;
	      v->info		= *m;

	      for (vvv = &list; (vv = *vvv); vvv = &vv->next)
		if (v->pub.width == vv->width)
		  {
		    if (v->pub.height == vv->height)
		      {
		        if (v->pub.vfreq > vv->vfreq)
		          break;
		      }
		    else if (v->pub.height > vv->height)
		      break;
		  }
		else if (v->pub.width > vv->width)
		  break;

	      if (v)
	        {
	          v->pub.next = vv;
	          *vvv = &v->pub;
		}
	    }
	}
      else
	{
	  if (X11STUFF_VIDMODE_DEBUG)
	    printv ("Ignore bad mode dot=%u hd=%u vd=%u\n",
		    m->dotclock, m->hdisplay, m->vdisplay);
	}
    }

  XFree (mode_info);

  return list;
}

/**
 * x11_vidmode_by_name:
 * @list: List of VidModes as returned by x11_vidmode_list_new().
 * @name: VidMode name "width [sep height [sep vfreq [sep]]]".
 *   All whitespace is ignored, sep can be any sequence of non-digits,
 *   characters following the last sep are ignored. Numbers strtoul.
 *
 * Returns a pointer to the first VidMode matching the name, NULL
 * when the mode is not in the list. (This is intended to select
 * a mode from config file.)
 */
const x11_vidmode_info *
x11_vidmode_by_name		(const x11_vidmode_info *list,
				 const gchar *		name)
{
  unsigned int val[3] = { 0, 0, 0 };
  const x11_vidmode_info *info;
  double fmin;
  unsigned int i;
  
  if (!name)
    return NULL;

  for (i = 0; i < 3; i++)
    {
      while (isspace (*name))
        ++name;

      if (0 == *name || !isdigit (*name))
        break;

      val[i] = strtoul (name, (char **) &name, 0);

      while (isspace (*name))
        ++name;
      while (*name && !isdigit (*name))
        ++name;
    }

  if (val[0] == 0)
    return NULL;

  info = NULL;
  fmin = 1e20;

  for (; list; list = list->next)
    if (   (list->width == val[0])
        && (val[1] == 0 || list->height == val[1]))
      {
        double d;
      
        if (val[2] == 0)
	  {
	    info = list;
	    break;
	  }
	
	d = fabs (list->vfreq - val[2]);

	if (d < fmin)
	  {
	    info = list;
	    fmin = d;
	  }
      }

  return info;
}

/**
 * x11_vidmode_current:
 * @list: List of VidModes as returned by x11_vidmode_list_new().
 *
 * Returns a pointer to the current VidMode, NULL when the mode
 * is not in the list or some other problem occurred.
 */
const x11_vidmode_info *
x11_vidmode_current		(const x11_vidmode_info *list)
{
  XF86VidModeModeLine mode_line;
  int dot_clock;
  const struct vidmode *vl;
  const struct vidmode *vm;

  if (!list)
    return NULL;

  vl = CONST_PARENT (list, struct vidmode, pub);

  if (!XF86VidModeGetModeLine (vl->display,
			       vl->screen_number,
			       &dot_clock,
			       &mode_line))
    {
      if (X11STUFF_VIDMODE_DEBUG)
	printv ("XF86VidModeGetModeLine() failed\n");
      return NULL;
    }

  if (mode_line.privsize > 0)
    {
      XFree (mode_line.private);
      mode_line.private = NULL;
      mode_line.privsize = 0;
    }

  for (vm = vl; vm; vm = CONST_PARENT (vm->pub.next, struct vidmode, pub))
    if (   vm->info.dotclock	== dot_clock
	&& vm->info.hdisplay	== mode_line.hdisplay
	&& vm->info.hsyncstart	== mode_line.hsyncstart
	&& vm->info.hsyncend	== mode_line.hsyncend
	&& vm->info.htotal	== mode_line.htotal
	&& vm->info.vdisplay	== mode_line.vdisplay
	&& vm->info.vsyncstart	== mode_line.vsyncstart
	&& vm->info.vsyncend	== mode_line.vsyncend
	&& vm->info.vtotal	== mode_line.vtotal)
      break;

  if (!vm)
    {
      if (X11STUFF_VIDMODE_DEBUG)
	printv ("Current VidMode dot=%u hd=%u vd=%u not in list\n",
		dot_clock, mode_line.hdisplay, mode_line.vdisplay);
      return NULL;
    }

  return &vm->pub;
}

/**
 * x11_vidmode_current:
 * @vlist: List of VidModes as returned by x11_vidmode_list_new().
 * @slist: List of Xinerama screens, used to determine actual screen
 *  sizes when Xinerama is enabled.
 * @vm: VidMode to switch to, must be member of @list.
 * @vs: If given save settings for x11_vidmode_restore() here.
 *
 * Switches to VidMode @vm, can be NULL to keep the current mode.
 * Then centers the viewport over the root window, and if
 * necessary moves the pointer into sight.
 */
gboolean
x11_vidmode_switch		(const x11_vidmode_info *vlist,
				 const tv_screen *	slist,
				 const x11_vidmode_info *vm,
				 x11_vidmode_state *	vs)
{
  const struct vidmode *vl;
  x11_vidmode_state state;
  Window root;
  Window dummy1;
  int x;
  int y;
  int dummy2;
  unsigned int w;
  unsigned int h;
  unsigned int dummy3;
  int px;
  int py;
  int warp;

  if (!vlist)
    return FALSE;

  vl = CONST_PARENT (vlist, struct vidmode, pub);

  if (!vs)
    vs = &state;

  x11_vidmode_clear_state (vs);

  root = DefaultRootWindow (vl->display);

  if (slist)
    {
      /* Get geometry of vl->screen. */

      for (; slist; slist = slist->next)
	if (slist->screen_number == vl->screen_number)
	  break;

      if (!slist)
	return FALSE;

      x	= slist->x;
      y	= slist->y;
      w = slist->width;
      h = slist->height;
    }
  else
    {
      XGetGeometry (vl->display,
		    /* drawable */ root,
		    /* root */ &dummy1,
		    &x, &y, &w, &h,
		    /* border_width */ &dummy3,
		    /* depth */ &dummy3);
    }

  XF86VidModeGetViewPort (vl->display,
			  vl->screen_number,
			  &vs->_old.vp.x,
			  &vs->_old.vp.y);

  XQueryPointer (vl->display,
		 /* window */ root,
		 /* root */ &dummy1,
		 /* child */ &dummy1,
		 /* root_x */ &vs->_old.pt.x,
		 /* root_y */ &vs->_old.pt.y,
		 /* win_x */ &dummy2,
		 /* win_y */ &dummy2,
		 /* mask */ &dummy3);

  /* Switch to requested mode, if any */

  {
    const x11_vidmode_info *cur_vm;

    cur_vm = x11_vidmode_current (vlist);

    vs->_old.vm = cur_vm;

    if (vm)
      {
	if (cur_vm != vm)
	  {
	    XF86VidModeModeInfo info;

	    info = CVIDMODE (vm)->info;
	    if (XF86VidModeSwitchToMode (vl->display,
					 vl->screen_number,
					 &info))
	      {
		/* Might not be exactly what we asked for,
		   or even anything we know. */
		if (!(vm = x11_vidmode_current (vlist)))
		  return FALSE;

		vs->_new.vm = vm;
	      }
	    else
	      {
		if (X11STUFF_VIDMODE_DEBUG)
		  printv ("XF86VidModeSwitchToMode() failed\n");
		
		return FALSE;
	      }
	  }
      }
    else if (cur_vm)
      {
	vm = cur_vm;
      }
    else
      {
	return FALSE;
      }
  }

  /* Center ViewPort */

  px = (w - vm->width) >> 1; /* screen relative */
  py = (h - vm->height) >> 1;

  assert (px >= 0 && py >= 0);

  if (XF86VidModeSetViewPort (CVIDMODE (vm)->display,
			      CVIDMODE (vm)->screen_number,
			      px, py))
    {
      vs->_new.vp.x = px;
      vs->_new.vp.y = py;
    }
  else
    {
      if (X11STUFF_VIDMODE_DEBUG)
	printv ("XF86VidModeSetViewPort() failed\n");

      px = vs->_old.vp.x;
      py = vs->_old.vp.y;
    }

  /* Make pointer visible */

  warp = 0;

  px += x; /* root relative */
  py += y;

  if (vs->_old.pt.x < px)
    {
      warp = 1;
    }
  else if (vs->_old.pt.x > px + vm->width - 16)
    {
      px += vm->width - 16;
      warp = 1;
    }
  else
    {
      px = vs->_old.pt.x;
    }

  if (vs->_old.pt.y < py)
    {
      warp = 1;
    }
  else if (vs->_old.pt.y > py + vm->height - 16)
    {
      py += vm->height - 16;
      warp = 1;
    }
  else
    {
      py = vs->_old.pt.y;
    }

  if (warp)
    {
      XWarpPointer (CVIDMODE (vm)->display,
		    /* src_window */ None,
		    /* dst_window */ root,
		    /* src_x, y, width, height */ 0, 0, 0, 0,
		    /* dst_x, y */ px, py);

      vs->_new.pt.x = px;
      vs->_new.pt.y = py;
    }

  XSync (CVIDMODE (vm)->display, False);

  return TRUE;
}

/**
 * x11_vidmode_current:
 * @list: List of VidModes as returned by x11_vidmode_list_new(),
 *        must be the same as given to the corresponding
 *        x11_vidmode_switch().
 * @vs: Settings saved with x11_vidmode_switch(), can be NULL.
 *
 * Restores from a x11_vidmode_switch(), except a third party
 * (i.e. the user) changed the settings since x11_vidmode_switch().
 * Calls x11_vidmode_clear_state() to prevent restore twice.
 */
void
x11_vidmode_restore		(const x11_vidmode_info *list,
				 x11_vidmode_state *	vs)
{
  Window root;
  Window dummy1;
  int vpx;
  int vpy;
  int ptx;
  int pty;
  int dummy2;
  unsigned int dummy3;

  if (!list)
    return;

  if (!vs)
    return;

  root = DefaultRootWindow (CVIDMODE (list)->display);

  XF86VidModeGetViewPort (CVIDMODE (list)->display,
			  CVIDMODE (list)->screen_number,
			  &vpx,
			  &vpy);

  XQueryPointer (CVIDMODE (list)->display,
		 /* window */ root,
		 /* root */ &dummy1,
		 /* child */ &dummy1,
		 /* root_x */ &ptx,
		 /* root_y */ &pty,
		 /* win_x */ &dummy2,
		 /* win_y */ &dummy2,
		 /* mask */ &dummy3);

  if (vs->_old.vm && vs->_new.vm)
    {
      const x11_vidmode_info *cur_vm;

      cur_vm = x11_vidmode_current (list);

      if (vs->_new.vm != cur_vm)
	goto done; /* user changed vidmode, keep that */

      if (vs->_old.vm != cur_vm)
	{
	  XF86VidModeModeInfo info;

	  info = CVIDMODE (vs->_old.vm)->info;
	  if (!XF86VidModeSwitchToMode (CVIDMODE (list)->display,
					CVIDMODE (list)->screen_number,
					&info))
	    {
	      if (X11STUFF_VIDMODE_DEBUG)
		printv ("Cannot restore old mode, "
			"XF86VidModeSwitchToMode() failed\n");
	      goto done;
	    }
	}
    }

  if (vs->_new.vp.x == vpx
      && vs->_new.vp.y == vpy)
    {
      XF86VidModeSetViewPort (CVIDMODE (list)->display,
			      CVIDMODE (list)->screen_number,
			      vs->_old.vp.x,
			      vs->_old.vp.y);
    }
  else
    {
      goto done; /* user moved viewport, keep that */
    }

  if (abs (vs->_new.pt.x - ptx) < 10
      && abs (vs->_new.pt.y - pty) < 10)
    XWarpPointer (CVIDMODE (list)->display,
		  /* src_window */ None,
		  /* dst_window */ root,
		  /* src_x, y, width, height */ 0, 0, 0, 0,
		  /* dst_x, y */ vs->_old.pt.x, vs->_old.pt.y);
  /* else user moved pointer, keep that */

 done:
  XSync (CVIDMODE (list)->display, False);

  x11_vidmode_clear_state (vs);
}

#else /* !HAVE_VIDMODE_EXTENSION */

void
x11_vidmode_list_delete		(x11_vidmode_info *	list)
{
}

x11_vidmode_info *
x11_vidmode_list_new		(const char *		display_name,
				 int			screen)
{
  printv ("VidMode extension support not compiled in\n");

  return NULL;
}

const x11_vidmode_info *
x11_vidmode_by_name		(const x11_vidmode_info *list,
				 const gchar *		name)
{
  return NULL;
}

const x11_vidmode_info *
x11_vidmode_current		(const x11_vidmode_info *list)
{
  return NULL;
}

gboolean
x11_vidmode_switch		(const x11_vidmode_info *vlist,
				 const tv_screen *	slist,
				 const x11_vidmode_info *vm,
				 x11_vidmode_state *	vs)
{
  return FALSE;
}

void
x11_vidmode_restore		(const x11_vidmode_info *list,
				 x11_vidmode_state *	vs)
{
}

#endif /* !HAVE_VIDMODE_EXTENSION */

/*
 *  Screensaver
 */

/* This is used to temporarily disable any screensaver functions (blanking,
   animation, monitor power down) while watching TV. */

#ifndef X11STUFF_SCREENSAVER_DEBUG
#define X11STUFF_SCREENSAVER_DEBUG 0
#endif

#ifdef HAVE_DPMS_EXTENSION
/* xc/doc/hardcopy/Xext/DPMSLib.PS.gz */
#include <X11/Xproto.h> /* CARD16 */
#include <X11/extensions/dpms.h>
#endif

/*
   Tested:				on		check-test (TTX mode)
   xset s 7 (blank/noblank)		blocked		blanks
   sleep 1; xset s active		deactivates	no action
   xset dpms 7				blocked		blanks
   sleep 1; xset dpms force off		forces on+	no action
   xscreensaver 4.06:
     detect start & kill		ok
     transition from / to xset s 7	blocked
     blank after 1m			blocked		blanks
     dpms blank 1m			blocked+	blanks
     ++
   xautolock 2.1 (w/MIT saver)+++	blocked		blanks

   UNTESTED:
   KScreensaver

  + after work-around
  ++ Various methods to detect inactivity (XIdle, monitoring kernel
     device etc) UNTESTED or NOT BLOCKED.
  +++ XIdle UNTESTED. Without MIT or XIdle xautolock scans all
      windows for KeyPress events, that is NOT BLOCKED.
*/

static Atom _XA_SCREENSAVER_VERSION;
static Atom _XA_SCREENSAVER;
static Atom _XA_DEACTIVATE;

static gboolean		screensaver_enabled;
static unsigned int	screensaver_level;
static gboolean		kscreensaver;
static gboolean		dpms_usable;
static guint		screensaver_timeout_id;

static gboolean
find_xscreensaver_window	(Display *		display,
				 Window *		window_return)
{
  Window root = RootWindowOfScreen (DefaultScreenOfDisplay (display));
  Window root2, parent, *kids;
  unsigned int nkids;
  unsigned int i;

  if (!XQueryTree (display, root, &root2, &parent, &kids, &nkids)
      || root != root2 || parent
      || !kids || nkids == 0)
    return FALSE;

  /* We're walking the list of root-level windows and trying to find
     the one that has a particular property on it.  We need to trap
     BadWindow errors while doing this, because it's possible that
     some random window might get deleted in the meantime.  (That
     window won't have been the one we're looking for.) */
  old_error_handler = XSetErrorHandler (bad_window_handler);

  for (i = 0; i < nkids; i++)
    {
      void *vec;
      unsigned long nitems;

      XSync (display, False);

      if (Success == get_window_property (display, kids[i],
                                          _XA_SCREENSAVER_VERSION,
                                          XA_STRING, &nitems, &vec))
        {
	  /* FIXME not true; anything to free here?
	  assert (0 == nitems); */

	  XSetErrorHandler (old_error_handler);
          *window_return = kids[i];
          XFree (kids);
	  return TRUE;
	}
    }

  XSetErrorHandler (old_error_handler);
  XFree (kids);

  return FALSE;
}

/* Here we reset the idle counter at regular intervals,
   an inconvenience outweight by a number of advantages:
   No need to remember and restore saver states. It works
   nicely with other apps because no synchronization is
   neccessary. When we unexpectedly bite the dust, the
   screensaver kicks in as usual, no cleanup necessary. */
static gboolean
screensaver_timeout		(gpointer		unused _unused_)
{
  Display *display = GDK_DISPLAY ();
  Window window;
  gboolean xss;

  /* xscreensaver is a client, it may come and go at will. */
  xss = find_xscreensaver_window (display, &window);

  if (X11STUFF_SCREENSAVER_DEBUG)
    printv ("screensaver_timeout() xss=%d dpms=%d\n", xss, dpms_usable);

  if (xss)
    {
      /* xscreensaver overrides the X savers, so it takes
	 priority here. This call unblanks the display and/or
	 re-starts the idle counter. Error ignored, response ignored. */
      send_event (display, window, _XA_SCREENSAVER, (long) _XA_DEACTIVATE, 0);
    }
  /* xscreensaver 4.06 doesn't appear to call XForceScreenSaver() on
     _XA_DEACTIVATE when DPMS was enabled, only when MIT or SGI
     screensaver extensions are used, so we're forced to do it anyway.*/ 
  /* else */
    {
      /* Restart the idle counter, this impacts the internal
	 saver, MIT-SCREEN-SAVER extension and DPMS extension
	 idle counter (in XFree86 4.2). Curiously it forces
	 the monitor back on after blanking by the internal
	 saver, but not after DPMS blanking. */
      XForceScreenSaver (display, ScreenSaverReset);

#ifdef HAVE_DPMS_EXTENSION

      if (dpms_usable)
	{
	  CARD16 power_level;
	  BOOL state;

	  if (DPMSInfo (display, &power_level, &state))
	    {
	      if (X11STUFF_SCREENSAVER_DEBUG)
		printv ("DPMSInfo(level=%d enabled=%d)\n",
			(int) power_level, (int) state);

	      if (power_level != DPMSModeOn)
		/* Error ignored */
		DPMSForceLevel (display, DPMSModeOn);
	    }
	}

#endif

    }

  return TRUE; /* call again */
}

/**
 * x11_screensaver_set:
 * @level:
 *
 * Switch the screensaver on or off. @level can be one of
 * X11_SCREENSAVER_ON or X11_SCREENSAVER_DISPLAY_ACTIVE.
 * When the function is disabled it does nothing.
 */
void
x11_screensaver_set		(unsigned int		level)
{
  Display *display = GDK_DISPLAY ();

  if (X11STUFF_SCREENSAVER_DEBUG)
    printv ("x11_screensaver_set (level=%d) enabled=%d dpms=%d\n",
	    (int) level, screensaver_enabled, dpms_usable);

  if (screensaver_enabled)
    {
      if (level == X11_SCREENSAVER_ON)
	{
	  if (NO_SOURCE_ID != screensaver_timeout_id)
	    {
	      g_source_remove (screensaver_timeout_id);
	      screensaver_timeout_id = NO_SOURCE_ID;
	    }

	  /* Stolen from xawtvdecode, untested. */
#ifndef __NetBSD__
	  if (kscreensaver)
	    {
	      /* Error ignored. */
	      system ("dcop kdesktop KScreensaverIface "
		      "enable true &>/dev/null");
	    }
#endif
	}
      else
	{
	  if (level & X11_SCREENSAVER_CPU_ACTIVE)
	    {
	      /* to do - this should block APM */
	    }
	  
	  if (level & X11_SCREENSAVER_DISPLAY_ACTIVE)
	    {
	      if (NO_SOURCE_ID == screensaver_timeout_id)
		{
		  /* Make sure the display is on now. */
		  screensaver_timeout (NULL);	      
		  XSync (display, False);
		  
		  screensaver_timeout_id =
		    g_timeout_add (5432 /* ms */,
				   (GSourceFunc) screensaver_timeout,
				   NULL);
		}
	    }

	  /* Stolen from xawtvdecode, untested. */
#ifndef __NetBSD__
	  if (kscreensaver)
	    {
	      /* Error ignored. */
	      system ("dcop kdesktop KScreensaverIface "
		      "enable false &>/dev/null");
	    }
#endif
	}
    }

  screensaver_level = level;
}

/**
 * x11_screensaver_control:
 * @enable:
 *
 * Enable or disable the x11_screensaver_set() function.
 */
void
x11_screensaver_control		(gboolean		enable)
{
  if (enable)
    {
      screensaver_enabled = TRUE;
      x11_screensaver_set (screensaver_level);
    }
  else if (screensaver_level != X11_SCREENSAVER_ON)
    {
      unsigned int level = screensaver_level;

      x11_screensaver_set (X11_SCREENSAVER_ON);

      screensaver_enabled = FALSE;
      screensaver_level = level;
    }
}

void
x11_screensaver_init		(void)
{
  Display *display = GDK_DISPLAY ();
#ifdef HAVE_DPMS_EXTENSION
  int event_base, error_base;
  int major_version, minor_version;
#endif

  _XA_SCREENSAVER_VERSION = XInternAtom (display, "_SCREENSAVER_VERSION", False);
  _XA_SCREENSAVER	  = XInternAtom (display, "SCREENSAVER", False);
  _XA_DEACTIVATE	  = XInternAtom (display, "DEACTIVATE", False);

  screensaver_enabled	  = FALSE;
  screensaver_level	  = X11_SCREENSAVER_ON;
  dpms_usable		  = FALSE;
  screensaver_timeout_id  = NO_SOURCE_ID;

#ifdef HAVE_DPMS_EXTENSION

  if (!DPMSQueryExtension (display, &event_base, &error_base))
    {
      printv ("DPMS extension not available\n");
      return;
    }

  if (!DPMSGetVersion (display, &major_version, &minor_version))
    {
      printv ("DPMS extension not usable\n");
      return;
    }

  printv ("DPMS base %d, %d, version %d.%d\n",
	  event_base, error_base,
	  major_version, minor_version);

  if (1 != major_version)
    {
      printv ("Unknown DPMS version\n");
      return;
    }

  if (!DPMSCapable (display))
    {
      printv ("Display does not support DPMS\n");
      return;
    }

  dpms_usable = TRUE;

#else /* !HAVE_DPMS_EXTENSION */

  printv ("DPMS extension support not compiled in\n");

#endif /* !HAVE_DPMS_EXTENSION */

#ifndef __NetBSD__
  /* Stolen from xawdecode, untested. */
  kscreensaver =
    (0 == system ("dcop kdesktop KScreensaverIface isEnabled "
		  "2>/dev/null | grep true &>/dev/null"));

  printv ("KScreensaver %spresent\n", kscreensaver ? "" : "not ");
#endif
}

/*
 *  XVideo helpers
 */

#ifdef HAVE_XV_EXTENSION

#include <X11/extensions/Xvlib.h>

#define FOURCC(a, b, c, d)						\
  (+ ((unsigned int)(a) << 0)						\
   + ((unsigned int)(b) << 8)						\
   + ((unsigned int)(c) << 16)						\
   + ((unsigned int)(d) << 24))

tv_pixfmt
x11_xv_image_format_to_pixfmt	(const XvImageFormatValues *format)
{
	static struct {
		unsigned int	fourcc;
		tv_pixfmt		pixfmt;
	} fourcc_mapping [] = {
		/* These are Windows FOURCCs. Note the APM driver supports
		   Y211 which is just a half width YUY2, hence not listed. */

		{ FOURCC ('Y','U','V','A'), TV_PIXFMT_YUVA32_LE }, /* Glint */
		{ FOURCC ('U','Y','V','Y'), TV_PIXFMT_UYVY },
		{ FOURCC ('U','Y','N','V'), TV_PIXFMT_UYVY },
		{ FOURCC ('Y','U','Y','2'), TV_PIXFMT_YUYV },
		{ FOURCC ('Y','U','N','V'), TV_PIXFMT_YUYV },
		{ FOURCC ('V','4','2','2'), TV_PIXFMT_YUYV },
		{ FOURCC ('Y','V','Y','U'), TV_PIXFMT_YVYU },
		{ FOURCC ('Y','2','Y','0'), TV_PIXFMT_YVYU }, /* NSC */
		{ FOURCC ('V','Y','U','Y'), TV_PIXFMT_VYUY }, /* APM */
		{ FOURCC ('I','4','2','0'), TV_PIXFMT_YUV420 },
		{ FOURCC ('I','Y','U','V'), TV_PIXFMT_YUV420 },
		{ FOURCC ('Y','V','1','2'), TV_PIXFMT_YVU420 },
		{ FOURCC ('Y','V','U','9'), TV_PIXFMT_YVU410 },
		{ FOURCC ('Y','8','0','0'), TV_PIXFMT_Y8 }, /* NSC */
		{ FOURCC ('Y','Y','Y','Y'), TV_PIXFMT_Y8 }, /* APM */
	};

	if (XvYUV == format->type) {
		unsigned int fourcc;
		unsigned int i;

		fourcc = FOURCC (format->guid[0],
				 format->guid[1],
				 format->guid[2],
				 format->guid[3]);

		for (i = 0; i < N_ELEMENTS (fourcc_mapping); ++i)
			if (fourcc_mapping[i].fourcc == fourcc)
				return fourcc_mapping[i].pixfmt;
	} else if (XvRGB == format->type) {
		tv_pixel_format pixel_format;

		if (XvPlanar == format->format)
			return TV_PIXFMT_UNKNOWN;

		CLEAR (pixel_format);

		pixel_format.bits_per_pixel = format->bits_per_pixel;
		pixel_format.color_depth = format->depth;
		pixel_format.big_endian = (MSBFirst == format->byte_order);
		pixel_format.mask.rgb.r = format->red_mask;
		pixel_format.mask.rgb.g = format->green_mask;
		pixel_format.mask.rgb.b = format->blue_mask;

		if (tv_pixel_format_to_pixfmt (&pixel_format))
			return pixel_format.pixfmt;
	}

	return TV_PIXFMT_UNKNOWN;
}

static __inline__ char
printable			(char			c)
{
	return (c < 0x20 || c >= 0x7F) ? '.' : c;
}

static void
xv_image_format_dump		(const XvImageFormatValues *format,
				 unsigned int		index)
{
	unsigned int i;
	tv_pixfmt pixfmt;

	fprintf (stderr, "    XvImageFormat [%u]:\n"
		 "      id                0x%x ('%c%c%c%c')\n"
		 "      guid              ",
		 index,
		 (unsigned int) format->id,
		 printable (((unsigned int) format->id) >> 0),
		 printable (((unsigned int) format->id) >> 8),
		 printable (((unsigned int) format->id) >> 16),
		 printable (((unsigned int) format->id) >> 24));

	for (i = 0; i < 16; ++i)
		fprintf (stderr, "%02x%s",
			 format->guid[i] & 0xFF,
			 (0x2A8 & (1 << i)) ? "-" : "");

	fprintf (stderr, "\n"
		 "      type              %s\n"
		 "      format            %s\n"
		 "      num_planes        %u\n"
		 "      byte_order        %s\n"
		 "      bits_per_pixel    %u\n",
		 (format->type == XvRGB) ? "RGB" :
		  (format->type == XvYUV) ? "YUV" : "?",
		 (format->format == XvPacked) ? "Packed" :
		  (format->format == XvPlanar) ? "Planar" : "?",
		 (unsigned int) format->num_planes,
		 (format->byte_order == LSBFirst) ? "LSBFirst/NoOrder" :
		  (format->byte_order == MSBFirst) ? "MSBFirst" : "?",
		 (unsigned int) format->bits_per_pixel);

	switch (format->type) {
	case XvRGB:
		fprintf (stderr,
			 "      rgb mask          0x%08x 0x%08x 0x%08x\n"
			 "      depth             %u\n",
			 (unsigned int) format->red_mask,
			 (unsigned int) format->green_mask,
			 (unsigned int) format->blue_mask,
			 (unsigned int) format->depth);
		break;

	case XvYUV:
		fprintf (stderr,
			 "      yuv bits          %u %u %u\n"
			 "      yuv hor. period   %u %u %u\n"
			 "      yuv vert. period  %u %u %u\n",
			 (unsigned int) format->y_sample_bits,
			 (unsigned int) format->u_sample_bits,
			 (unsigned int) format->v_sample_bits,
			 (unsigned int) format->horz_y_period,
			 (unsigned int) format->horz_u_period,
			 (unsigned int) format->horz_v_period,
			 (unsigned int) format->vert_y_period,
			 (unsigned int) format->vert_u_period,
			 (unsigned int) format->vert_v_period);
		break;

	default:
		break;
	}

	fprintf (stderr, "      component_order   ");

	for (i = 0; i < sizeof (format->component_order); ++i)
		fputc (printable (format->component_order[i]), stderr);

	fprintf (stderr, "\n"
		 "      scanline_order    %s\n",
		 (format->scanline_order == XvTopToBottom) ? "TopToBottom" :
		  (format->scanline_order == XvBottomToTop) ? "BottomToTop" :
		   "");

	pixfmt = x11_xv_image_format_to_pixfmt (format);

	fprintf (stderr, "      pixfmt equiv      %s\n",
		 tv_pixfmt_name (pixfmt));
}

static void
xv_adaptor_info_dump		(const XvAdaptorInfo *	adaptor,
				 unsigned int		index)
{
	static struct {
		unsigned int		mask;
		const char *		name;
	} adaptor_types [] = {
		{ XvInputMask,	"Input" },
		{ XvOutputMask,	"Output" },
		{ XvVideoMask,	"Video" },
		{ XvStillMask,	"Still" },
		{ XvImageMask,	"Image" },
	};
	unsigned int type;
	unsigned int count;
	unsigned int i;

	fprintf (stderr, "XvAdaptorInfo [%u]:\n"
		 "  name         \"%s\"\n"
		 "  base_id      0x%x\n"
		 "  num_ports    %u\n"
		 "  type         ",
		 index,
		 adaptor->name,
		 (unsigned int) adaptor->base_id,
		 (unsigned int) adaptor->num_ports);

	type = adaptor->type;
	count = 0;

	for (i = 0; i < N_ELEMENTS (adaptor_types); ++i)
		if (type & adaptor_types[i].mask) {
			fprintf (stderr, "%s%s",
				 count ? "|" : "",
				 adaptor_types[i].name);
			type &= ~adaptor_types[i].mask;
			++count;
		}

	if (type != 0)
		fprintf (stderr, "%s0x%x", count ? "|" : "", type);

	fprintf (stderr, "\n"
		 "  num_formats  %u\n",
		 (unsigned int) adaptor->num_formats);
}

static void
xv_adaptor_dump			(Display *		display,
				 const XvAdaptorInfo *	adaptor,
				 unsigned int		index)
{
	XvImageFormatValues *pImageFormats[2];
	int nImageFormats[2];
	unsigned int i;

	xv_adaptor_info_dump (adaptor, index);

	if (0 == adaptor->num_ports) {
		fprintf (stderr, "  No ports\n");
		return;
	}

	CLEAR (nImageFormats);

	for (i = 0; i < adaptor->num_ports; ++i) {
		XvPortID xvport;

		fprintf (stderr, "  Port [%u]\n", i);

		xvport = adaptor->base_id + i;

		pImageFormats[1] =
			XvListImageFormats (display, xvport,
					    &nImageFormats[1]);

		if (NULL == pImageFormats[1] || 0 == nImageFormats[1])
			continue;

		if (nImageFormats[0] == nImageFormats[1]
		    && 0 == memcmp (pImageFormats[0], pImageFormats[1],
				    sizeof (*pImageFormats[0])
				    * nImageFormats[0])) {
			fprintf (stderr, "    %u formats as above\n",
				 nImageFormats[1]);
		} else {
			int i;

			if (0 == nImageFormats[1])
				fprintf (stderr, "    No image formats\n");
			else
				for (i = 0; i < nImageFormats[1]; ++i)
					xv_image_format_dump
						(pImageFormats[1] + i, i);
		}

		if (nImageFormats[0] > 0)
			XFree (pImageFormats[0]);

		pImageFormats[0] = pImageFormats[1];
		nImageFormats[0] = nImageFormats[1];
	}

	if (nImageFormats[0] > 0)
		XFree (pImageFormats[0]);
}

void
x11_xvideo_dump			(void)
{
  Display *display;
  unsigned int version;
  unsigned int revision;
  unsigned int major_opcode;
  unsigned int event_base;
  unsigned int error_base;
  Window root;
  XvAdaptorInfo *pAdaptors;
  int nAdaptors;
  int i;

  display = GDK_DISPLAY ();

  if (Success != XvQueryExtension (display,
				   &version, &revision,
				   &major_opcode,
				   &event_base, &error_base))
    {
      printv ("XVideo extension not available\n");
      return;
    }

  printv ("XVideo opcode %d, base %d, %d, version %d.%d\n",
	  major_opcode,
	  event_base, error_base,
	  version, revision);

  if (version < 2 || (version == 2 && revision < 2))
    {
      printv ("XVideo extension not usable\n");
      return;
    }

  root = DefaultRootWindow (display);

  if (Success != XvQueryAdaptors (display, root, &nAdaptors, &pAdaptors))
    {
      printv ("XvQueryAdaptors failed\n");
      return;
    }

  if (nAdaptors > 0)
    {
      for (i = 0; i < nAdaptors; ++i)
	xv_adaptor_dump (display, pAdaptors + i, i);

      XvFreeAdaptorInfo (pAdaptors);
    }
  else
    {
      printv ("No XVideo adaptors\n");
    }
}

#else /* !HAVE_XV_EXTENSION */

void
x11_xvideo_dump			(void)
{
	fprintf (stderr, "XVideo extension support not compiled in\n");
}

#endif /* !HAVE_XV_EXTENSION */

/*
 *  Clipping helpers
 */

static tv_bool
children_clips			(tv_clip_vector *	vector,
				 Display *		display,
				 Window			window,
				 Window			stack_level,
				 int			x,
				 int			y,
				 unsigned int		width,
				 unsigned int		height,
				 int			parent_x,
				 int			parent_y)
{
	Window root;
	Window parent;
	Window *children;
	unsigned int nchildren;
	unsigned int i;

	if (!XQueryTree (display, window, &root, &parent,
			 &children, &nchildren))
		return FALSE;

	if (nchildren == 0)
		return TRUE;

	for (i = 0; i < nchildren; ++i)
		if (children[i] == stack_level)
			break;

	if (i == nchildren)
		i = 0;
	else
		++i;

	/* We need to trap BadWindow errors while traversing the
	   list of windows because they might get deleted in the
	   meantime. */
	old_error_handler = XSetErrorHandler (bad_window_handler);

	for (; i < nchildren; ++i) {
		XWindowAttributes wts;
		int x1, y1;
		int x2, y2;
    
		bad_window = FALSE;
    
		XGetWindowAttributes (display, children[i], &wts);
    
		if (bad_window)
			continue;
    
		if (!(wts.map_state & IsViewable))
			continue;
		
		x1 = (wts.x + parent_x - wts.border_width) - x;
		y1 = (wts.y + parent_y - wts.border_width) - y;
		x2 = x1 + wts.width + wts.border_width * 2;
		y2 = y1 + wts.height + wts.border_width * 2;

		if (x2 < 0 || x1 > (int) width ||
		    y2 < 0 || y1 > (int) height)
			continue;

		if (!tv_clip_vector_add_clip_xy (vector,
						 MAX (x1, 0),
						 MAX (y1, 0),
						 MIN (x2, (int) width),
						 MIN (y2, (int) height)))
			return FALSE;
	}

	XSetErrorHandler (old_error_handler);

	XFree ((char *) children);

	return TRUE;
}

/*
 * x11_window_clip_vector:
 * @vector: Initialized tv_clip_vector, clipping rectangles are stored here.
 * @display:
 * @window: Window "displaying" the overlaid video. Regions of the overlay
 *   outside this window add to the clip vector. It also determines the
 *   video layer, which other windows are visible "above" the video.
 * @x, y, width, height: Top, left coordinates and size of the overlay.
 *   Coordinates are relative to the target base address, i.e. the
 *   root window. The overlay can lie partially or completely outside the
 *   @window and @display.
 *
 * Returns a clip vector describing areas obscuring the @x, @y, @width,
 * @height overlay rectangle. Clip coordinates in the vector will be
 * relative to @x, @y.
 *
 * It should be obvious, but just in case: We cannot lock the X server,
 * so the resulting clip vector is only a snapshot. It may be already
 * outdated when returned. 
 *
 * Returns FALSE if unsuccessful, with random vector contents.
 */
tv_bool
x11_window_clip_vector		(tv_clip_vector *	vector,
				 Display *		display,
				 Window			window,
				 int			x,
				 int			y,
				 unsigned int		width,
				 unsigned int		height)
{
	Window root;
	int wx1, wy1;		/* window inner bounds, root relative */
	int wx2, wy2;
	int x2, y2;		/* x + width, y + height */

	tv_clip_vector_clear (vector);

	if (0)
	  fprintf (stderr, "%s overl: %d, %d - %d, %d (%u x %u)\n",
		   __FUNCTION__,
		   x, y, x+width, y+height, width, height);
  
	{
		unsigned int width;
		unsigned int height;
		unsigned int dummy;
		Window child;

		if (!XGetGeometry (display, window, &root,
				   &wx1, &wy1,
				   &width, &height,
				   /* border_width */ &dummy,
				   /* depth */ &dummy))
			return FALSE;

		if (!XTranslateCoordinates (display, window, root,
					    0, 0, &wx1, &wy1, &child))
			return FALSE;

		wx2 = wx1 + width;
		wy2 = wy1 + height;

		if (0)
		  fprintf (stderr, "%s window: %d, %d - %d, %d (%u x %u)\n",
			   __FUNCTION__,
			   wx1, wy1, wx2, wy2, width, height);
	}

	x2 = x + width;
	y2 = y + height;

	if (x >= wx2 || x2 <= wx1 ||
	    y >= wy2 || y2 <= wy1)
		return tv_clip_vector_add_clip_xy
			(vector, 0, 0, width, height);

	if (x < wx1) {
		if (!tv_clip_vector_add_clip_xy
		    (vector, 0, 0, wx1 - x, height))
			return FALSE;
	}

	if (y < wy1) {
		if (!tv_clip_vector_add_clip_xy
		    (vector, 0, 0, width, wy1 - y))
			return FALSE;
	}

	if (x2 > wx2) {
		if (!tv_clip_vector_add_clip_xy
		    (vector, wx2 - x, 0, x2 - wx2, height))
			return FALSE;
	}

	if (y2 > wy2) {
		if (!tv_clip_vector_add_clip_xy
		    (vector, 0, wy2 - y, width, y2 - wy2))
			return FALSE;
	}

	if (!children_clips (vector, display, window, None,
			     x, y, width, height, wx1, wy1))
		return FALSE;

	for (;;) {
		Window parent;
		Window *children;
		unsigned int nchildren;
      
		if (!XQueryTree (display, window, &root, &parent,
				 &children, &nchildren))
			return FALSE;

		if (children)
			XFree ((char *) children);
      
		if (root == parent)
			break;

		window = parent;
	}

	return children_clips (vector, display, root, window,
			       x, y, width, height, 0, 0);
}

