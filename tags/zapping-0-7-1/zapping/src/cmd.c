/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2002 I�aki Garc�a Etxebarria
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

/**
 * Provides the functionality in the Python interface of Zapping.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

#include "cmd.h"
#include "interface.h"
#include "plugins.h"
#include "zmisc.h"
#include "zconf.h"
#include "globals.h"
#include "audio.h"
#include "remote.h"

static PyObject* py_quit (PyObject *self _unused_,
			  PyObject *args _unused_)
{
  GList *p;
  int x, y, w, h;
  gboolean quit_muted;

  if (!zapping)
    py_return_false;

  quit_muted = TRUE;

  /* Error ignored */
  zconf_get_boolean (&quit_muted, "/zapping/options/main/quit_muted");

  if (quit_muted)
    {
      /* Error ignored */
      tv_quiet_set (zapping->info, TRUE);
    }

  /* Save the currently tuned channel */
  zconf_set_int (cur_tuned_channel,
		 "/zapping/options/main/cur_tuned_channel");

  flag_exit_program = TRUE;

  gdk_window_get_origin(GTK_WIDGET (zapping)->window, &x, &y);
  gdk_window_get_geometry(GTK_WIDGET (zapping)->window, NULL, NULL, &w, &h,
			  NULL);
  
  zconf_set_int (x, "/zapping/internal/callbacks/x");
  zconf_set_int (y, "/zapping/internal/callbacks/y");
  zconf_set_int (w, "/zapping/internal/callbacks/w");
  zconf_set_int (h, "/zapping/internal/callbacks/h");

  zconf_set_int ((int) to_old_tveng_capture_mode (zapping->display_mode,
						  zapping->info->capture_mode),
		 "/zapping/options/main/capture_mode");

  zmisc_switch_mode (DISPLAY_MODE_NONE,
		     CAPTURE_MODE_NONE,
		     zapping->info);

  /* Tell the widget that the GUI is going to be closed */
  p = g_list_first(plugin_list);
  while (p)
    {
      plugin_remove_gui (&zapping->app, (struct plugin_info *) p->data);
      p = p->next;
    }

  gtk_main_quit ();

  py_return_true;
}

static void
resolve_mode			(display_mode *		d,
				 capture_mode *		c,
				 const gchar *		mode_str)
{
  if (0 == strcasecmp (mode_str, "preview"))
    {
      *d = DISPLAY_MODE_WINDOW;
      *c = CAPTURE_MODE_OVERLAY;
    }
  else if (0 == strcasecmp (mode_str, "fullscreen"))
    {
      *d = DISPLAY_MODE_FULLSCREEN;
      *c = CAPTURE_MODE_OVERLAY;
    }
  else if (0 == strcasecmp (mode_str, "capture"))
    {
      *d = DISPLAY_MODE_WINDOW;
      *c = CAPTURE_MODE_READ;
    }
  else if (0 == strcasecmp (mode_str, "teletext"))
    {
      *d = DISPLAY_MODE_WINDOW;
      *c = CAPTURE_MODE_TELETEXT;
    }
  else
    {
      ShowBox ("Unknown display mode \"%s\", possible choices are:\n"
	       "preview, fullscreen, capture and teletext",
	       GTK_MESSAGE_ERROR, mode_str);
      *d = DISPLAY_MODE_NONE;
      *c = CAPTURE_MODE_NONE;
    }
}

static gboolean
switch_mode			(display_mode dmode,
				 capture_mode cmode)
{
  if (0)
    fprintf (stderr, "switch_mode: %d %d\n", dmode, cmode);

  if (-1 == zmisc_switch_mode (dmode, cmode, zapping->info))
    {
      ShowBox(zapping->info->error, GTK_MESSAGE_ERROR);
      return FALSE;
    }

  return TRUE;
}

static PyObject *
py_switch_mode			(PyObject *		self _unused_,
				 PyObject *		args)
{
  display_mode old_dmode;
  capture_mode old_cmode;
  display_mode new_dmode;
  capture_mode new_cmode;
  char *mode_str;

  if (!zapping)
    py_return_false;

  if (!ParseTuple (args, "s", &mode_str))
    g_error ("zapping.switch_mode(s)");

  old_dmode = zapping->display_mode;
  old_cmode = zapping->info->capture_mode;

  resolve_mode (&new_dmode, &new_cmode, mode_str);

  if (!switch_mode (new_dmode, new_cmode))
    py_return_false;

  if (zapping->display_mode != old_dmode
      || zapping->info->capture_mode != old_cmode)
    {
      last_dmode = old_dmode;
      last_cmode = old_cmode;
    }

  py_return_none;
}

static PyObject *
py_toggle_mode			(PyObject *		self _unused_,
				 PyObject *		args)
{
  capture_mode old_cmode;
  capture_mode new_cmode;
  display_mode old_dmode;
  display_mode new_dmode;
  char *mode_str;

  if (!zapping)
    py_return_false;

  mode_str = NULL;

  if (!ParseTuple (args, "|s", &mode_str))
    g_error ("zapping.toggle_mode(|s)");

  old_dmode = zapping->display_mode;
  old_cmode = zapping->info->capture_mode;

  if (mode_str)
    {
      resolve_mode (&new_dmode, &new_cmode, mode_str);
    }
  else
    {
      new_dmode = old_dmode;
      new_cmode = old_cmode;
    }

  if (new_dmode == old_dmode
      && new_cmode == old_cmode)
    {
      if (new_dmode == last_dmode
	  && new_cmode == last_cmode)
	py_return_true;

      if (!switch_mode (last_dmode, last_cmode))
	py_return_false;

      SWAP (new_dmode, last_dmode);
      SWAP (new_cmode, last_cmode);
    }
  else
    {
      if (!switch_mode (new_dmode, new_cmode))
	py_return_false;

      last_dmode = old_dmode;
      last_cmode = old_cmode;
    }

  py_return_true;
}

static PyObject* py_about (PyObject *self _unused_, PyObject *args _unused_)
{
  GtkWidget *about = build_widget ("about", NULL);

  /* Do the setting up libglade can't */
  g_object_set (G_OBJECT (about), "name", PACKAGE,
		"version", VERSION, NULL);

  gtk_widget_show (about);

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* py_resize_screen (PyObject *self _unused_, PyObject *args)
{
  GdkWindow *subwindow;
  GdkWindow *mw;
  gint sw_w, sw_h, mw_w, mw_h;
  int w, h;
  int ok;

  if (!zapping)
    py_return_false;

  subwindow = lookup_widget (GTK_WIDGET (zapping), "tv-screen")->window;
  mw = gdk_window_get_toplevel(subwindow);
  ok = ParseTuple (args, "ii", &w, &h);

  if (!ok)
    g_error ("zapping.resize_screen(ii)");

  gdk_window_get_geometry (mw, NULL, NULL, &mw_w, &mw_h, NULL);
  gdk_window_get_geometry (subwindow, NULL, NULL, &sw_w, &sw_h, NULL);

  w += (mw_w - sw_w);
  h += (mw_h - sw_h);

  gdk_window_resize(mw, w, h);

  py_return_true;
}

static PyObject *
py_hide_controls		(PyObject *		self _unused_,
				 PyObject *		args)
{
  int hide;
  int ok;

  hide = 2; /* toggle */
  ok = ParseTuple (args, "|i", &hide);

  if (!ok)
    g_error ("zapping.hide_controls(|i)");

  zconf_set_boolean (hide, "/zapping/internal/callbacks/hide_controls");

  py_return_none;
}

static PyObject *
py_keep_on_top			(PyObject *		self _unused_,
				 PyObject *		args)
{
  extern gboolean have_wm_hints;
  int keep;
  int ok;

  if (!zapping)
    py_return_false;

  keep = 2; /* toggle */
  ok = ParseTuple (args, "|i", &keep);

  if (!ok)
    g_error ("zapping.keep_on_top(|i)");

  if (have_wm_hints)
    {
      if (keep > 1)
        keep = !zconf_get_boolean (NULL, "/zapping/options/main/keep_on_top");
      zconf_set_boolean (keep, "/zapping/options/main/keep_on_top");

      x11_window_on_top (GTK_WINDOW (zapping), keep);
    }

  py_return_none;
}

static PyObject *py_help (PyObject *self _unused_, PyObject *args _unused_)
{
  /* XXX handle error, maybe use link_id */
  gnome_help_display ("zapping", NULL, NULL);

  py_return_none;
}

void
startup_cmd (void)
{
  cmd_register ("quit", py_quit, METH_VARARGS,
		("Quit"), "zapping.quit()");
  cmd_register ("switch_mode", py_switch_mode, METH_VARARGS,
		("Switch to Fullscreen mode"), "zapping.switch_mode('fullscreen')",
		("Switch to Capture mode"), "zapping.switch_mode('capture')",
		("Switch to Overlay mode"), "zapping.switch_mode('preview')",
		("Switch to Teletext mode"), "zapping.switch_mode('teletext')");
  /* FIXME: This isn't the place for this, create a mode.c containing
     the mode switching logic, it's getting a bit too  complex */
  cmd_register ("toggle_mode", py_toggle_mode, METH_VARARGS,
		("Switch to Fullscreen mode or previous mode"), "zapping.toggle_mode('fullscreen')",
		("Switch to Capture mode or previous mode"), "zapping.toggle_mode('capture')",
		("Switch to Overlay mode or previous mode"), "zapping.toggle_mode('preview')",
		("Switch to Teletext mode or previous mode"), "zapping.toggle_mode('teletext')");
  /* Compatibility (FIXME: Does it really make sense to keep this?) */
  cmd_register ("restore_mode", py_toggle_mode, METH_VARARGS);
  cmd_register ("about", py_about, METH_VARARGS,
		("About Zapping"), "zapping.about()");
  cmd_register ("resize_screen", py_resize_screen, METH_VARARGS);
  cmd_register ("hide_controls", py_hide_controls, METH_VARARGS,
		("Show/hide menu and toolbar"), "zapping.hide_controls()");
  cmd_register ("help", py_help, METH_VARARGS,
		("Zapping help"), "zapping.help()"); 
  cmd_register ("keep_on_top", py_keep_on_top, METH_VARARGS,
		("Keep window on top"), "zapping.keep_on_top()");
}

void
shutdown_cmd (void)
{
}