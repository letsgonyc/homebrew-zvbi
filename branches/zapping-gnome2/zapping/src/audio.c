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


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include "../common/fifo.h" // current_time()
#include <math.h>
#include <unistd.h>

#include "audio.h"
#define ZCONF_DOMAIN "/zapping/options/audio/"
#include "zconf.h"
#include "properties.h"
#include "interface.h"
#include "zmisc.h"
#include "mixer.h"
#include "osd.h"
#include "remote.h"
#include "callbacks.h"
#include "globals.h"
#include "v4linterface.h"


extern audio_backend_info esd_backend;
#if USE_OSS
extern audio_backend_info oss_backend;
#endif
#if HAVE_ARTS
extern audio_backend_info arts_backend;
#endif

static audio_backend_info *backends [] =
{
  &esd_backend,
#if USE_OSS
  &oss_backend,
#endif
#if HAVE_ARTS
  &arts_backend
#endif
};

#define num_backends (sizeof(backends)/sizeof(backends[0]))

typedef struct mhandle {
  gint		owner; /* backend owning this handle */
  gpointer	handle; /* for the backend */
} mhandle;

void mixer_setup ( void )
{
  int cur_line = zcg_int(NULL, "record_source");

  if (!cur_line)
    return; /* Use system settings */

  cur_line--;

  mixer_set_recording_line(cur_line);
  mixer_set_volume(cur_line, zcg_int(NULL, "record_volume"));
}

/* Generic stuff */
gpointer
open_audio_device (gboolean stereo, gint rate, enum audio_format
		   format)
{
  gint cur_backend = zcg_int(NULL, "backend");
  gpointer handle = backends[cur_backend]->open(stereo, rate, format);
  mhandle *mhandle;

  if (!handle)
    {
      ShowBox(_("Cannot open the configured audio device.\n"
		"You might want to setup another kind of audio\n"
		"device in the Properties dialog."),
	      GTK_MESSAGE_WARNING);
      return NULL;
    }

  mhandle = (struct mhandle *) g_malloc0(sizeof(*mhandle));
  mhandle->handle = handle;
  mhandle->owner = cur_backend;

  /* make sure we record from the appropiate source */
  mixer_setup();

  return mhandle;
}

void
close_audio_device (gpointer handle)
{
  mhandle *mhandle = (struct mhandle *) handle;

  backends[mhandle->owner]->close(mhandle->handle);

  g_free(mhandle);
}

void
read_audio_data (gpointer handle, gpointer dest, gint num_bytes,
		 double *timestamp)
{
  mhandle *mhandle = (struct mhandle *) handle;

  backends[mhandle->owner]->read(mhandle->handle, dest, num_bytes,
				timestamp);
}

static void
on_backend_activate	(GObject		*menuitem,
			 gpointer		pindex)
{
  gint index = GPOINTER_TO_INT(pindex);
  GObject * audio_backends =
    G_OBJECT(g_object_get_data(menuitem, "user-data"));
  gint cur_sel =
    GPOINTER_TO_INT(g_object_get_data(audio_backends, "cur_sel"));
  GtkWidget **boxes =
    (GtkWidget**)g_object_get_data(audio_backends, "boxes");

  if (cur_sel == index)
    return;

  g_object_set_data(audio_backends, "cur_sel", pindex);
  gtk_widget_hide(boxes[cur_sel]);
  gtk_widget_set_sensitive(boxes[cur_sel], FALSE);

  gtk_widget_show(boxes[index]);
  gtk_widget_set_sensitive(boxes[index], TRUE);
}

static void
build_mixer_lines		(GtkWidget	*optionmenu)
{
  GtkMenuShell *menu = GTK_MENU_SHELL
    (gtk_option_menu_get_menu(GTK_OPTION_MENU(optionmenu)));
  GtkWidget *menuitem;
  gchar *label;
  gint i;

  for (i=0;(label = mixer_get_description(i));i++)
    {
      menuitem = gtk_menu_item_new_with_label(label);
      free(label);
      gtk_widget_show(menuitem);
      gtk_menu_shell_append(menu, menuitem);
    }
}

static void
on_record_source_changed	(GtkOptionMenu	*optionmenu,
				 GtkRange	*hscale)
{
  int min, max;
  GtkAdjustment *adj;
  gint cur_sel = z_option_menu_get_active (GTK_WIDGET (optionmenu));

  gtk_widget_set_sensitive(GTK_WIDGET(hscale), cur_sel);

  if (!cur_sel)
    return;

  /* note that it's cur_sel-1, the 0 entry is "Use system settings" */
  g_assert(!mixer_get_bounds(cur_sel-1, &min, &max));
  
  adj = gtk_range_get_adjustment(hscale);

  adj -> lower = min;
  adj -> upper = max;

  gtk_adjustment_changed(adj);
}

/* Audio */
static void
audio_setup		(GtkWidget	*page)
{
  GtkWidget *widget;
  GtkWidget *audio_backends =
    lookup_widget(page, "audio_backends");
  GtkWidget **boxes = (GtkWidget**)g_malloc0(num_backends*sizeof(boxes[0]));
  GtkWidget *vbox39 = lookup_widget(page, "vbox39");
  GtkWidget *menuitem;
  GtkWidget *menu = gtk_menu_new();
  guint cur_backend = zcg_int(NULL, "backend");
  GtkWidget *record_source = lookup_widget(page, "record_source");
  gint cur_input = zcg_int(NULL, "record_source");
  GtkWidget *record_volume = lookup_widget(page, "record_volume");
  guint i;

  /* Hook on dialog destruction so there's no mem leak */
  g_object_set_data_full(G_OBJECT (audio_backends), "boxes", boxes,
			   (GtkDestroyNotify)g_free);

  for (i=0; i<num_backends; i++)
    {
      menuitem = gtk_menu_item_new_with_label(_(backends[i]->name));
      g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(on_backend_activate),
			 GINT_TO_POINTER(i));
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
      g_object_set_data(G_OBJECT(menuitem), "user-data",
			audio_backends);
      gtk_widget_show(menuitem);

      boxes[i] = gtk_vbox_new(FALSE, 3);
      if (backends[i]->add_props)
	backends[i]->add_props(GTK_BOX(boxes[i]));
      gtk_box_pack_start(GTK_BOX(vbox39), boxes[i], FALSE, FALSE, 0);
      if (i == cur_backend)
	gtk_widget_show(boxes[i]);
      else
	gtk_widget_set_sensitive(boxes[i], FALSE);
    }

  gtk_option_menu_set_menu(GTK_OPTION_MENU (audio_backends), menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU (audio_backends),
			      cur_backend);
  g_object_set_data(G_OBJECT (audio_backends), "cur_sel",
		      GINT_TO_POINTER(cur_backend));

  /* Avoid noise while changing channels */
  widget = lookup_widget(page, "checkbutton1");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/main/avoid_noise"));

  /* Force use of mixer for volume control */
  widget = lookup_widget(page, "force_mixer");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/audio/force_mixer"));

  /* Start zapping muted */
  widget = lookup_widget(page, "checkbutton3");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/main/start_muted"));

  /* Recording source and volume */
  build_mixer_lines(record_source);
  z_option_menu_set_active(record_source, cur_input);
  g_signal_connect(G_OBJECT(record_source), "changed",
		   G_CALLBACK(on_record_source_changed),
		   record_volume);
  on_record_source_changed(GTK_OPTION_MENU (record_source),
			   GTK_RANGE(record_volume));
  
  gtk_adjustment_set_value(gtk_range_get_adjustment(GTK_RANGE(record_volume)),
			   zcg_int(NULL, "record_volume"));
}

static void
audio_apply		(GtkWidget	*page)
{
  GtkWidget *widget;
  GtkWidget *audio_backends;
  gint selected;
  GtkWidget **boxes;
  GtkWidget *record_source;
  GtkWidget *record_volume;

  widget = lookup_widget(page, "checkbutton1"); /* avoid noise */
  zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/main/avoid_noise");

  widget = lookup_widget(page, "force_mixer");
  zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/audio/force_mixer");

  widget = lookup_widget(page, "checkbutton3"); /* start muted */
  zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/main/start_muted");  

  /* Apply the properties */
  audio_backends = lookup_widget(page, "audio_backends");
  selected = z_option_menu_get_active(audio_backends);
  boxes = (GtkWidget**)g_object_get_data(G_OBJECT(audio_backends),
					   "boxes");

  if (backends[selected]->apply_props)
    backends[selected]->apply_props(GTK_BOX(boxes[selected]));

  zcs_int(selected, "backend");

  record_source = lookup_widget(page, "record_source");
  zcs_int(z_option_menu_get_active(record_source), "record_source");

  record_volume = lookup_widget(page, "record_volume");
  zcs_int((int) gtk_range_get_adjustment(GTK_RANGE(record_volume))->value,
	  "record_volume");
}

static void
add				(GtkDialog	*dialog)
{
  SidebarEntry general_options[] = {
    { N_("Audio"), "gnome-grecord.png", "vbox39",
      audio_setup, audio_apply }
  };
  SidebarGroup groups[] = {
    { N_("General Options"), general_options, acount(general_options) }
  };

  standard_properties_add(dialog, groups, acount(groups), "zapping.glade2");
}

gboolean
audio_set_mute			(gint			mute)
{
  int cur_line = zcg_int (NULL, "record_source");
  gboolean force = zcg_bool (NULL, "force_mixer");

  if (main_info->audio_mutable && !force)
    {
      if (tveng_set_mute (!!mute, main_info) < 0)
        {
	  printv ("tveng_set_mute failed\n");
	  return FALSE;
	}
    }
  else if (cur_line > 0) /* !use system settings */
    {
      if (mixer_set_mute (cur_line - 1, !!mute) < 0)
	{
	  printv ("mixer_set_mute failed\n");
	  return FALSE;
	}

      tveng_set_mute (0, main_info);
    }

  return TRUE;
}

gboolean
audio_get_mute			(gint *			mute)
{
  int cur_line = zcg_int (NULL, "record_source");
  gboolean force = zcg_bool (NULL, "force_mixer");

  if (main_info->audio_mutable && !force)
    {
      if ((*mute = tveng_get_mute (main_info)) < 0)
        {
          printv ("tveng_get_mute failed\n");
          return FALSE;
        }
    }
  else if (cur_line > 0) /* !use system settings */
    {
      if ((*mute = mixer_get_mute (cur_line - 1)) < 0)
        {
          printv ("mixer_get_mute failed\n");
          return FALSE;
        }
    }

  return TRUE;
}	


static PyObject*
py_volume_incr			(PyObject *self, PyObject *args)
{
  int cur_line = zcg_int (NULL, "record_source");
  int min, max, range, step, cur;
  int value = +1;
  int ok = PyArg_ParseTuple (args, "|i", &value);

  if (!ok)
    g_error ("zapping.volume_incr(|i)");

  if (value < -100 || value > +100)
    goto done;

  if (!cur_line)
    goto done;

  cur_line--; /* 0 is "system setting" */

  if (mixer_get_bounds (cur_line, &min, &max) == -1)
    goto done;

  range = max - min + 1;
  step = (int)(((double) value) * range / 100.0);
  if (value != 0 && step == 0)
    step = (value >> 31) | 1;

  cur = zcg_int(NULL, "record_volume");

  cur += step;

  if (cur > max)
    cur = max;
  else if (cur < min)
    cur = min;

  if (mixer_set_volume(cur_line, cur) == -1)
    goto done;

  zcs_int(cur, "record_volume");

#ifdef HAVE_LIBZVBI
  /* NLS: Record volume */
  osd_render_markup(NULL, _("<span foreground=\"blue\">%3d %%</span>"),
		    (cur - min) * 100 / range);
#endif

 done:
  Py_INCREF(Py_None);

  return Py_None;
}

/*
 *  Global mute function. XXX switch to callbacks.
 *  mode: 0=sound, 1=mute, 2=toggle, 3=just update GUI
 *  controls: update controls box
 *  osd: show state on screen
 */
gboolean
set_mute				(gint	        mode,
					 gboolean	controls,
					 gboolean	osd)
{
  static gboolean recursion = FALSE;
  gint mute;

  if (recursion)
    return TRUE;

  recursion = TRUE;

  /* Get current state */

  if (mode >= 2)
    {
      if (!audio_get_mute (&mute))
        goto failure;

      if (mode == 2)
	mute = !mute;
    }
  else
    mute = !!mode;

  /* Set new state */

  if (mode <= 2)
    if (!audio_set_mute (mute))
      goto failure;

  /* Update GUI */

  {
    GtkWidget *button;
    GtkCheckMenuItem *check;

    button = lookup_widget (main_window, "tb-mute");

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)) != mute)
      {
	SIGNAL_HANDLER_BLOCK (button, on_remote_command1,
			      gtk_toggle_button_set_active
			      (GTK_TOGGLE_BUTTON (button), mute));
      }

    check = GTK_CHECK_MENU_ITEM (lookup_widget (main_window, "mute2"));

    if (check->active != mute)
      {
	SIGNAL_HANDLER_BLOCK (check, on_remote_command1,
			      gtk_check_menu_item_set_active (check, mute));
      }

    if (controls)
      update_control_box (main_info);

    if (osd)
      {
	if (main_info->current_mode == TVENG_CAPTURE_PREVIEW ||
	    !GTK_WIDGET_VISIBLE (lookup_widget (main_window, "bonobodockitem2")))
	  osd_render_markup (NULL, mute ?
			   _("<blue>audio off</blue>") :
			   _("<yellow>AUDIO ON</yellow>"));
      }
  }

  recursion = FALSE;
  return TRUE;

 failure:
  recursion = FALSE;
  return FALSE;
}

static PyObject* py_mute (PyObject *self, PyObject *args)
{
  int value = 2; /* toggle by default */
  int ok = PyArg_ParseTuple (args, "|i", &value);

  if (!ok)
    {
      g_warning ("zapping.mute(|i)");
      py_return_false;
    }

  set_mute (value, TRUE, TRUE);

  py_return_true;
}

void startup_audio ( void )
{
  guint i;

  property_handler audio_handler =
  {
    add: add
  };

  prepend_property_handler(&audio_handler);

  zcc_int(0, "Default audio backend", "backend");
  zcc_int(0, "Recording source", "record_source");
  zcc_int(0xffff, "Recording volume", "record_volume");

  for (i=0; i<num_backends; i++)
    if (backends[i]->init)
      backends[i]->init();

  cmd_register ("mute", py_mute, METH_VARARGS,
		"Toggles the mute state", "zapping.mute([new_state])");
  cmd_register ("volume_incr", py_volume_incr, METH_VARARGS,
		"Changes the sounds volume", "zapping.volume_incr(-10)");
}

void shutdown_audio ( void )
{
  guint i;

  for (i=0; i<num_backends; i++)
    if (backends[i]->shutdown)
      backends[i]->shutdown();
}
