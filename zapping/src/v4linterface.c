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

/* XXX gtk+ 2.3 GtkOptionMenu, GnomeColorPicker */
#undef GTK_DISABLE_DEPRECATED
#undef GNOME_DISABLE_DEPRECATED

#include "site_def.h"

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gnome.h>
#include <math.h>
#include <ctype.h>

/* Routines for building GUI elements dependant on the v4l device
   (such as the number of inputs and so on) */
#include "tveng.h"
#include "v4linterface.h"
#define ZCONF_DOMAIN "/zapping/options/main/"
#include "zconf.h"
#include "zmisc.h"
#include "zspinslider.h"
#include "interface.h"
#include "zvbi.h"
#include "osd.h"
#include "globals.h"
#include "audio.h"
#include "mixer.h"
#include "properties-handler.h"
#include "xawtv.h"
#include "subtitle.h"
#include "capture.h"
#include "remote.h"

struct control_window;

struct control_window *ToolBox = NULL; /* Pointer to the last control box */
ZModel *z_input_model = NULL;

/* Minimize updates */
static gboolean freeze = FALSE, needs_refresh = FALSE;
static gboolean rebuild_channel_menu = TRUE;

static void
update_bundle				(ZModel		*model _unused_,
					 tveng_device_info *info _unused_)
{
  if (freeze)
    {
      needs_refresh = TRUE;
      return;
    }

  if (rebuild_channel_menu)
    zapping_rebuild_channel_menu (zapping);
}

static void
freeze_update (void)
{
  freeze = TRUE;
  needs_refresh = FALSE;
}

static void
thaw_update (void)
{
  freeze = FALSE;

  if (needs_refresh)
    update_bundle(z_input_model, zapping->info);

  needs_refresh = FALSE;
}

/*
 *  Control box
 */

struct control {
  struct control *		next;

  tveng_device_info *		info;

  tv_control *			ctrl;
  tv_callback *			tvcb;

  GtkWidget *			widget;
};

struct control_window
{
  GtkWidget *			window;
  GtkWidget *			hbox;

  struct control *		controls;

  /* This is only used for building */
  GtkWidget *			table;
  guint				index;
};

static void
on_tv_control_destroy		(tv_control *		ctrl _unused_,
				 void *			user_data)
{
  struct control *c = user_data;

  c->ctrl = NULL;
  c->tvcb = NULL;
}

static void
free_control			(struct control *	c)
{
  tv_callback_remove (c->tvcb);
  g_free (c);
}

static struct control *
add_control			(struct control_window *cb,
				 tveng_device_info *	info,
				 tv_control *		ctrl,
				 GtkWidget *		label,
				 GtkWidget *		crank,
				 GObject *		object,
				 const gchar *		signal,
				 void *			g_callback,
				 void *			tv_callback)
{
  struct control *c, **cp;

  g_assert (cb != NULL);
  g_assert (info != NULL);
  g_assert (ctrl != NULL);
  g_assert (crank != NULL);

  for (cp = &cb->controls; (c = *cp); cp = &c->next)
    ;

  c = g_malloc0 (sizeof (*c));
  
  c->info = info;
  c->ctrl = ctrl;

  c->widget = crank;

  gtk_table_resize (GTK_TABLE (cb->table), cb->index + 1, 2);

  if (label)
    {
      gtk_widget_show (label);

      gtk_table_attach (GTK_TABLE (cb->table), label,
			0, 1, cb->index, cb->index + 1,
			(GtkAttachOptions) (GTK_FILL),
			(GtkAttachOptions) (0), 3, 3);
    }

  gtk_widget_show (crank);

  gtk_table_attach (GTK_TABLE (cb->table), crank,
		    1, 2, cb->index, cb->index + 1,
                    (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
                    (GtkAttachOptions) (0), 3, 3);

  if (object && signal && g_callback)
    g_signal_connect (object, signal, G_CALLBACK (g_callback), c);

  if (tv_callback)
    {
      c->tvcb = tv_control_add_callback (ctrl, tv_callback,
					 on_tv_control_destroy, c);
    }

  *cp = c;

  return c;
}

static GtkWidget *
control_symbol			(tv_control *		ctrl)
{
  static const struct {
    tv_control_id		id;
    const char *		stock_id;
  } pixmaps [] = {
    { TV_CONTROL_ID_BRIGHTNESS,	"zapping-brightness" },
    { TV_CONTROL_ID_CONTRAST,	"zapping-contrast" },
    { TV_CONTROL_ID_SATURATION,	"zapping-saturation" },
    { TV_CONTROL_ID_HUE,	"zapping-hue" },
  };
  GtkWidget *symbol;
  guint i;

  symbol = NULL;

  for (i = 0; i < G_N_ELEMENTS (pixmaps); ++i)
    if (0 && ctrl->id == pixmaps[i].id)
      {
	symbol = gtk_image_new_from_stock (pixmaps[i].stock_id,
					   GTK_ICON_SIZE_BUTTON);
	gtk_misc_set_alignment (GTK_MISC (symbol), 1.0, 0.5);
	symbol = z_tooltip_set_wrap (symbol, ctrl->label);
	break;
      }

  if (!symbol)
    {
      symbol = gtk_label_new (ctrl->label);
      gtk_widget_show (symbol);
      gtk_misc_set_alignment (GTK_MISC (symbol), 1.0, 0.5);
    }

  return symbol;
}

static void
on_control_slider_changed	(GtkAdjustment *	adjust,
				 gpointer		user_data)
{
  struct control *c = user_data;

  TV_CALLBACK_BLOCK (c->tvcb, tveng_set_control
		     (c->ctrl, (int) adjust->value, c->info));
}

static void
on_tv_control_integer_changed	(tv_control *		ctrl,
				 void *			user_data)
{
  struct control *c = user_data;
  ZSpinSlider *sp;

  sp = Z_SPINSLIDER (c->widget);

  SIGNAL_HANDLER_BLOCK (sp->spin_adj,
			on_control_slider_changed,
			z_spinslider_set_int_value (sp, ctrl->value));
}

static void
create_slider			(struct control_window *cb,
				 tveng_device_info *	info,
				 tv_control *	ctrl)
{ 
  GObject *adj; /* Adjustment object for the slider */
  GtkWidget *spinslider;

  /* XXX use tv_control.step */

  adj = G_OBJECT (gtk_adjustment_new ((gfloat) ctrl->value,
				      (gfloat) ctrl->minimum,
				      (gfloat) ctrl->maximum,
				      (gfloat) 1, (gfloat) 10, (gfloat) 10));

  spinslider = z_spinslider_new (GTK_ADJUSTMENT (adj), NULL,
				 NULL, (gfloat) ctrl->reset, 0);

  z_spinslider_set_int_value (Z_SPINSLIDER (spinslider), ctrl->value);

  add_control (cb, info, ctrl, control_symbol (ctrl), spinslider,
	       adj, "value-changed", on_control_slider_changed,
	       on_tv_control_integer_changed);
}

static void
on_control_checkbutton_toggled	(GtkToggleButton *	tb,
				 gpointer		user_data)
{
  struct control *c = user_data;

  if (c->ctrl->id == TV_CONTROL_ID_MUTE)
    {
      TV_CALLBACK_BLOCK (c->tvcb, tv_mute_set
			 (c->info, gtk_toggle_button_get_active (tb)));
      /* Update tool button & menu XXX switch to callback */
      set_mute (3, /* controls */ FALSE, /* osd */ FALSE);
    }
  else
    {
      TV_CALLBACK_BLOCK (c->tvcb, tveng_set_control
			 (c->ctrl, gtk_toggle_button_get_active (tb), c->info));
    }
}

static void
on_tv_control_boolean_changed	(tv_control *		ctrl,
				 void *			user_data)
{
  struct control *c = user_data;

  SIGNAL_HANDLER_BLOCK (c->widget, on_control_checkbutton_toggled,
			gtk_toggle_button_set_active
			(GTK_TOGGLE_BUTTON (c->widget), ctrl->value));
}

static void
create_checkbutton		(struct control_window *cb,
				 tveng_device_info *	info,
				 tv_control *		ctrl)
{
  GtkWidget *check_button;

  check_button = gtk_check_button_new_with_label (ctrl->label);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
				ctrl->value);

  add_control (cb, info, ctrl, /* label */ NULL, check_button,
	       G_OBJECT (check_button), "toggled",
	       on_control_checkbutton_toggled,
	       on_tv_control_boolean_changed);
}

static void
on_control_menuitem_activate	(GtkMenuItem *		menuitem,
				 gpointer		user_data)
{
  struct control *c = user_data;
  gint value;

  value = z_object_get_int_data (G_OBJECT (menuitem), "value");

  tveng_set_control (c->ctrl, value, c->info);
}

static void
create_menu			(struct control_window *cb,
				 tveng_device_info *	info,
				 tv_control *		ctrl)
{
  GtkWidget *label; /* This shows what the menu is for */
  GtkWidget *option_menu; /* The option menu */
  GtkWidget *menu; /* The menu displayed */
  GtkWidget *menu_item; /* Each of the menu items */
  struct control *c;
  guint i;

  label = gtk_label_new (ctrl->label);
  gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);

  option_menu = gtk_option_menu_new ();
  menu = gtk_menu_new ();
  gtk_widget_show (menu);
  gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);

  c = add_control (cb, info, ctrl, label, option_menu,
		   NULL, NULL, NULL, NULL);

  /* Start querying menu_items and building the menu */
  for (i = 0; ctrl->menu[i] != NULL; i++)
    {
      menu_item = gtk_menu_item_new_with_label (_(ctrl->menu[i]));
      gtk_widget_show (menu_item);

      g_object_set_data (G_OBJECT (menu_item), "value", 
			 GINT_TO_POINTER (i));

      g_signal_connect (G_OBJECT (menu_item), "activate",
			G_CALLBACK (on_control_menuitem_activate), c);

      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
    }

  gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu),
			       (guint) ctrl->value);
}

static void
on_control_button_clicked	(GtkButton *		button _unused_,
				 gpointer		user_data)
{
  struct control *c = user_data;

  tveng_set_control (c->ctrl, 1, c->info);
}

static void
create_button			(struct control_window *cb,
				 tveng_device_info *	info,
				 tv_control *		ctrl)
{
  GtkWidget *button;

  button = gtk_button_new_with_label (ctrl->label);

  add_control (cb, info, ctrl, /* label */ NULL, button,
	       G_OBJECT (button), "clicked",
	       on_control_button_clicked, NULL);
}

static void
on_color_set			(GnomeColorPicker *	colorpicker _unused_,
				 guint			arg1,
				 guint			arg2,
				 guint			arg3,
				 guint			arg4 _unused_,
				 gpointer		user_data)
{
  struct control *c = user_data;
  guint color;

  color  = (arg1 >> 8) << 16;	/* red */
  color += (arg2 >> 8) << 8;	/* green */
  color += (arg3 >> 8);		/* blue */
  /* arg4 alpha ignored */

  tveng_set_control (c->ctrl, (int) color, c->info);
}

static void
create_color_picker		(struct control_window *cb,
				 tveng_device_info *	info,
				 tv_control *		ctrl)
{
  GtkWidget *label;
  GnomeColorPicker *color_picker;
  gchar *buffer;

  label = gtk_label_new (ctrl->label);
  gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);

  color_picker = GNOME_COLOR_PICKER (gnome_color_picker_new ());
  gnome_color_picker_set_use_alpha (color_picker, FALSE);
  gnome_color_picker_set_i8 (color_picker,
			     (ctrl->value & 0xff0000) >> 16,
			     (ctrl->value & 0xff00) >> 8,
			     (ctrl->value & 0xff),
			     0);

  /* TRANSLATORS: In controls box, color picker control,
     something like "Adjust Chroma-Key" for overlay. */
  buffer = g_strdup_printf (_("Adjust %s"), ctrl->label);
  gnome_color_picker_set_title (color_picker, buffer);
  g_free (buffer);

  add_control (cb, info, ctrl, label, GTK_WIDGET(color_picker),
	       G_OBJECT (color_picker), "color-set",
	       on_color_set, NULL);
}

static void
add_controls			(struct control_window *cb,
				 tveng_device_info *	info)
{
  tv_control *ctrl;

  if (cb->hbox)
    {
      struct control *c;

      while ((c = cb->controls))
	{
	  cb->controls = c->next;
	  free_control (c);
	}
      
      gtk_container_remove (GTK_CONTAINER (cb->window), cb->hbox);
    }

  cb->hbox = gtk_hbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (cb->hbox), 6);
  gtk_container_add (GTK_CONTAINER (cb->window), cb->hbox);

  /* Update the values of all the controls */
  if (-1 == tveng_update_controls (info))
    {
      /* FIXME aborting is no proper way to handle this.
      ShowBox ("Tveng critical error, Zapping will exit NOW.",
	       GTK_MESSAGE_ERROR);
      g_error ("tveng critical: %s", info->error);
      */
    }

  cb->table = NULL;
  cb->index = 0;

  for (ctrl = tv_next_control (info, NULL); ctrl; ctrl = ctrl->_next)
    {
      const tv_video_standard *vs;

      if (ctrl->id == TV_CONTROL_ID_HUE)
	if ((vs = tv_cur_video_standard (info)))
	  if (!(vs->videostd_set & TV_VIDEOSTD_SET_NTSC))
	    {
	      /* Useless. XXX connect to standard change callback. */
	      tveng_set_control (ctrl, ctrl->reset, info);
	      continue;
	    }

      if ((cb->index % 20) == 0)
	{
	  if (cb->table)
	    {
	      gtk_widget_show (cb->table);
	      gtk_box_pack_start_defaults (GTK_BOX (cb->hbox), cb->table);
	    }

	  cb->table = gtk_table_new (1, 2, FALSE);
	}

      switch (ctrl->type)
	{
	case TV_CONTROL_TYPE_INTEGER:
	  create_slider (cb, info, ctrl);
	  break;

	case TV_CONTROL_TYPE_BOOLEAN:
	  create_checkbutton (cb, info, ctrl);
	  break;

	case TV_CONTROL_TYPE_CHOICE:
	  create_menu (cb, info, ctrl);
	  break;

	case TV_CONTROL_TYPE_ACTION:
	  create_button (cb, info, ctrl);
	  break;

	case TV_CONTROL_TYPE_COLOR:
	  create_color_picker (cb, info, ctrl);
	  break;

	default:
	  g_warning ("Type %d of control %s is not supported",
		     ctrl->type, ctrl->label);
	  continue;
	}

      cb->index++;
    }

  if (cb->table)
    {
      gtk_widget_show (cb->table);
      gtk_box_pack_start_defaults (GTK_BOX (cb->hbox), cb->table);
    }

  gtk_widget_show (cb->hbox);
}

static gboolean
on_control_window_key_press	(GtkWidget *		widget,
				 GdkEventKey *		event,
				 gpointer		user_data)
{
  switch (event->keyval)
    {
    case GDK_Escape:
      gtk_widget_destroy (widget);
      return TRUE; /* handled */

    case GDK_c:
    case GDK_C:
      if (event->state & GDK_CONTROL_MASK)
	{
	  gtk_widget_destroy (widget);
	  return TRUE; /* handled */
	}

    default:
      break;
    }

  return on_channel_enter (widget, event, user_data)
    || on_user_key_press (widget, event, user_data)
    || on_picture_size_key_press (widget, event, user_data)
    || on_channel_key_press (widget, event, user_data);
}

static void
on_control_window_destroy	(GtkWidget *		widget _unused_,
				 gpointer		user_data)
{
  struct control_window *cb = user_data;
  struct control *c;

  ToolBox = NULL;

  while ((c = cb->controls))
    {
      cb->controls = c->next;
      free_control (c);
    }

  g_free (cb);

  /* See below.
     gtk_widget_set_sensitive (lookup_widget (GTK_WIDGET (zapping),
					      "toolbar-controls"), TRUE);
  */
}

static void
create_control_window		(void)
{
  struct control_window *cb;

  cb = g_malloc0 (sizeof (*cb));

  cb->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (cb->window), _("Controls"));
  g_signal_connect (G_OBJECT (cb->window), "destroy",
		    G_CALLBACK (on_control_window_destroy), cb);
  g_signal_connect (G_OBJECT(cb->window), "key-press-event",
		    G_CALLBACK (on_control_window_key_press), cb);

  add_controls (cb, zapping->info);

  gtk_widget_show (cb->window);

  /* Not good because it may just raise a hidden control window.
     gtk_widget_set_sensitive (lookup_widget (GTK_WIDGET (zapping),
					      "toolbar-controls"), FALSE);
   */

  ToolBox = cb;
}

static PyObject *
py_control_box			(PyObject *		self _unused_,
				 PyObject *		args _unused_)
{
  if (ToolBox == NULL)
    create_control_window ();
  else
    gtk_window_present (GTK_WINDOW (ToolBox->window));

  py_return_none;
}

void
update_control_box		(tveng_device_info *	info)
{
  struct control *c;
  tv_control *ctrl;

  if (!ToolBox)
    return;

  c = ToolBox->controls;

  for (ctrl = tv_next_control (info, NULL); ctrl; ctrl = ctrl->_next)
    {
      /* XXX Is this safe? Unlikely. */
      if (!c || c->ctrl != ctrl)
	goto rebuild;

      switch (ctrl->type)
	{
	case TV_CONTROL_TYPE_INTEGER:
	  on_tv_control_integer_changed (ctrl, c);
	  break;

	case TV_CONTROL_TYPE_BOOLEAN:
	  on_tv_control_boolean_changed (ctrl, c);
	  break;

	case TV_CONTROL_TYPE_CHOICE:
	  gtk_option_menu_set_history
	    (GTK_OPTION_MENU (c->widget), (guint) ctrl->value);
	  break;

	case TV_CONTROL_TYPE_ACTION:
	  break;

	case TV_CONTROL_TYPE_COLOR:
	  gnome_color_picker_set_i8
	    (GNOME_COLOR_PICKER (c->widget),
	     (ctrl->value & 0xff0000) >> 16,
	     (ctrl->value & 0xff00) >> 8,
	     (ctrl->value & 0xff),
	     0);
	  break;

	default:
	  g_warning ("Type %d of control %s is not supported",
		     ctrl->type, ctrl->label);
	  continue;
	}

      c = c->next;
    }

  return;
  
 rebuild:
  add_controls (ToolBox, info);
}



/* XXX these functions change an a/v source property but
   do not switch away from the current tveng_tuned_channel.
   Maybe there should be a -1 tuned channel intended to
   change on the fly, and a channel history to properly
   switch back (channel up, down etc). We also need a
   function to find a tuned channel already matching the
   new configuration. */

gboolean
z_switch_video_input		(guint hash, tveng_device_info *info)
{
  tv_video_line *l;
  capture_mode old_mode;

  if (!(l = tv_video_input_by_hash (info, hash)))
    {
#if 0 /* annoying */
      ShowBox("Couldn't find input with hash %x",
	      GTK_MESSAGE_ERROR, hash);
#endif
      return FALSE;
    }

  if (l == tv_cur_video_input (info))
    return TRUE;

  old_mode = tv_get_capture_mode (info);
  if (CAPTURE_MODE_READ == old_mode)
    {
      if (!capture_stop ())
	{
	  ShowBox ("%s", GTK_MESSAGE_ERROR, tv_get_errstr (info));
	  return FALSE;
	}
    }

  if (!tv_set_video_input (info, l))
    {
      if (CAPTURE_MODE_READ == old_mode) {
	capture_start (info, zapping->display_window);
      }

      ShowBox("Couldn't switch to video input %s\n%s",
	      GTK_MESSAGE_ERROR,
	      l->label, tv_get_errstr(info));
      return FALSE;
    }
  else
    {
      python_command_printf (NULL, "zapping.closed_caption(0)");
    }

  if (CAPTURE_MODE_READ == old_mode) {
    capture_start (info, zapping->display_window);
  }

  zmodel_changed(z_input_model);

  return TRUE;
}

gboolean
z_switch_audio_input		(guint hash, tveng_device_info *info)
{
  tv_audio_line *l;

  if (!(l = tv_audio_input_by_hash (info, hash)))
    {
#if 0 /* annoying */
      ShowBox("Couldn't find audio input with hash %x",
	      GTK_MESSAGE_ERROR, hash);
#endif
      return FALSE;
    }

  if (!tv_set_audio_input (info, l))
    {
      ShowBox("Couldn't switch to audio input %s\n%s",
	      GTK_MESSAGE_ERROR,
	      l->label, tv_get_errstr(info));
      return FALSE;
    }

  zmodel_changed(z_input_model);

  return TRUE;
}

gboolean
z_switch_standard		(guint hash, tveng_device_info *info)
{
  tv_video_standard *s;
  capture_mode old_mode;
  tv_bool r;
#ifdef HAVE_LIBZVBI
  vbi3_decoder *vbi;
#endif

  for (s = tv_next_video_standard (info, NULL);
       s; s = tv_next_video_standard (info, s))
    if (s->hash == hash)
      break;

  if (!s)
    {
#if 0 /* annoying */
      if (info->video_standards)
	ShowBox("Couldn't find standard with hash %x",
		GTK_MESSAGE_ERROR, hash);
#endif
      return FALSE;
    }

  if (s == tv_cur_video_standard (info))
    return TRUE;

#ifdef HAVE_LIBZVBI
  if ((vbi = zvbi_get_object ()))
    zvbi_stop (/* destroy_decoder */ FALSE);
#endif

  old_mode = tv_get_capture_mode (info);
  if (CAPTURE_MODE_READ == old_mode)
    {
      if (!capture_stop ())
	{
	  ShowBox ("%s", GTK_MESSAGE_ERROR, tv_get_errstr (info));

#ifdef HAVE_LIBZVBI
	  if (NULL != vbi)
	    {
	      /* Error ignored. */
	      zvbi_start ();
	    }
#endif
	  return FALSE;
	}
    }

  r = tv_set_video_standard (info, s);

  if (CAPTURE_MODE_READ == old_mode) {
    capture_start (info, zapping->display_window);
  }

#ifdef HAVE_LIBZVBI
  if (NULL != vbi)
    {
      /* Error ignored. */
      zvbi_start ();
    }
#endif

  if (!r)
    {
      ShowBox("Couldn't switch to standard %s\n%s",
	      GTK_MESSAGE_ERROR,
	      s->label, tv_get_errstr (info));
      return FALSE;
    }

  return TRUE;
}

/* Returns a newly allocated copy of the string, normalized */
static char* normalize(const char *string)
{
  int i = 0;
  const char *strptr=string;
  char *result;

  g_assert(string != NULL);

  result = strdup(string);

  g_assert(result != NULL);

  while (*strptr != 0) {
    if (*strptr == '_' || *strptr == '-' || *strptr == ' ') {
      strptr++;
      continue;
    }
    result[i] = tolower(*strptr);

    strptr++;
    i++;
  }
  result[i] = 0;

  return result;
}

/* nomalize and compare */
static int normstrcmp (const char * in1, const char * in2)
{
  char *s1 = normalize(in1);
  char *s2 = normalize(in2);

  g_assert(in1 != NULL);
  g_assert(in2 != NULL);

  /* Compare the strings */
  if (!strcmp(s1, s2)) {
    free(s1);
    free(s2);
    return 1;
  } else {
    free(s1);
    free(s2);
    return 0;
  }
}

gboolean
zconf_get_controls		(tveng_tuned_channel *	channel,
				 const gchar *		path)
{
  gchar *zcname;
  guint n_controls;
  guint i;

  zcname = g_strconcat (path, "/num_controls", NULL);
  zconf_get_uint (&n_controls, zcname);
  g_free (zcname);

  if (0 == n_controls)
    return TRUE;

  zcname = g_strconcat (path, "/controls", NULL);

  for (i = 0; i < n_controls; ++i)
    {
      gchar *path_i;
      gchar *zcname_i;
      const gchar *name;

      if (!zconf_get_nth (i, &path_i, zcname))
	{
	  g_warning ("Saved control %u is malformed, skipping", i);
	  continue;
	}

      zcname_i = g_strconcat (path_i, "/name", NULL);
      name = zconf_get_string (NULL, zcname_i);
      g_free (zcname_i);

      if (NULL != name)
	{
	  gfloat value;

	  zcname_i = g_strconcat (path_i, "/value", NULL);
	  /* Error ignored. */
	  zconf_get_float (&value, zcname_i);
	  g_free (zcname_i);

	  /* Error ignored. */
	  tveng_tuned_channel_set_control (channel, name, value);
	}

      g_free (path_i);
    }

  g_free (zcname);

  return TRUE;
}

void
zconf_create_controls		(tveng_tc_control *	tcc,
				 guint			num_controls,
				 const gchar *		path)
{
  guint i;

  for (i = 0; i < num_controls; i++)
    {
      gchar *name;

      name = g_strdup_printf ("%s/controls/%d/name", path, i);
      zconf_set_string (tcc[i].name, name);
      zconf_set_description ("Control name", name);
      g_free (name);

      name = g_strdup_printf ("%s/controls/%d/value", path, i);
      zconf_set_float (tcc[i].value, name);
      zconf_set_description ("Control value", name);
      g_free (name);
    }
}

#ifdef HAVE_LIBZVBI

gboolean
zconf_get_ttx_encodings		(tveng_tuned_channel *	channel,
				 const gchar *		path)
{
  gchar *zcname;
  guint n_encodings;
  guint i;

  zcname = g_strconcat (path, "/num_ttx_encodings", NULL);
  zconf_get_uint (&n_encodings, zcname);
  g_free (zcname);

  if (0 == n_encodings)
    return TRUE;

  zcname = g_strconcat (path, "/ttx_encodings", NULL);

  for (i = 0; i < n_encodings; ++i)
    {
      gchar *path_i;
      gchar *zcname_i;
      vbi3_pgno pgno;

      if (!zconf_get_nth (i, &path_i, zcname))
	{
	  g_warning ("Saved ttx_encoding %u is malformed, skipping", i);
	  continue;
	}

      zcname_i = g_strconcat (path_i, "/pgno", NULL);
      pgno = zconf_get_int (NULL, zcname_i);
      g_free (zcname_i);

      if (0 != pgno)
	{
	  vbi3_ttx_charset_code charset_code;

	  zcname_i = g_strconcat (path_i, "/charset_code", NULL);
	  /* Error ignored. */
	  charset_code = zconf_get_int (NULL, zcname_i);
	  g_free (zcname_i);

	  /* Error ignored. */
	  tveng_tuned_channel_set_ttx_encoding (channel, pgno, charset_code);
	}

      g_free (path_i);
    }

  g_free (zcname);

  return TRUE;
}

void
zconf_create_ttx_encodings	(tveng_ttx_encoding *	tcc,
				 guint			n_ttx_encodings,
				 const gchar *		path)
{
  guint i;

  for (i = 0; i < n_ttx_encodings; ++i)
    {
      if (0 != tcc[i].pgno &&
	  (vbi3_ttx_charset_code) -1 != tcc[i].charset_code)
	{
	  gchar *name;

	  name = g_strdup_printf ("%s/ttx_encodings/%d/pgno", path, i);
	  zconf_set_int ((gint) tcc[i].pgno, name);
	  zconf_set_description ("Page number", name);
	  g_free (name);

	  name = g_strdup_printf ("%s/ttx_encodings/%d/charset_code", path, i);
	  zconf_set_int ((gint) tcc[i].charset_code, name);
	  zconf_set_description ("Character set code", name);
	  g_free (name);
	}
    }
}

#endif

tveng_tc_control *
tveng_tc_control_by_id		(tveng_device_info *	info,
				 tveng_tc_control *	tcc,
				 guint			num_controls,
				 tv_control_id		id)
{
  const tv_control *c;
  guint i;

  c = tv_control_by_id (info, id);

  if (NULL != c)
    for (i = 0; i < num_controls; ++i)
      if (normstrcmp (c->label, tcc[i].name))
	return tcc + i;

  return NULL;
}

gint
load_control_values		(tveng_device_info *	info,
				 tveng_tc_control *	tcc,
				 guint			num_controls)
{
  tv_control *ctrl;
  guint i;
  gint mute = 0;

  g_assert (info != NULL);

  if (!tcc || num_controls == 0)
    return 0;

  for (ctrl = tv_next_control (info, NULL); ctrl; ctrl = ctrl->_next)
    for (i = 0; i < num_controls; i++)
      if (normstrcmp (ctrl->label, tcc[i].name))
	{
	  gint value;

	  value = (ctrl->minimum
		   + (ctrl->maximum - ctrl->minimum)
		   * tcc[i].value);
#if 0
	  if (ctrl->id == TV_CONTROL_ID_MUTE)
	    {
	      if (skip_mute)
		mute = value;
	      else
		set_mute (value, /* controls */ TRUE, /* osd */ FALSE);
	    }
	  else
#endif
	    {
	      tveng_set_control (ctrl, value, info);
	    }

	  break;
	}

  return mute;
}

void
store_control_values		(tveng_device_info *	info,
				 tveng_tc_control **	tccp,
				 guint *		num_controls_p)
{
  tveng_tc_control *tcc;
  guint num_controls;
  tv_control *ctrl;
  guint i;

  g_assert (info != NULL);
  g_assert (tccp != NULL);
  g_assert (num_controls_p != NULL);

  tcc = NULL;
  num_controls = 0;

  for (ctrl = tv_next_control (info, NULL); ctrl; ctrl = ctrl->_next)
    num_controls++;

  if (num_controls > 0)
    {
      tcc = g_malloc (sizeof (*tcc) * num_controls);

      ctrl = NULL;
      i = 0;

      while (i < num_controls && (ctrl = tv_next_control (info, ctrl)))
	{
	  int value;

	  value = ctrl->value;

	  g_strlcpy (tcc[i].name, ctrl->label, 32);
	  tcc[i].name[31] = 0;

	  if (ctrl->maximum > ctrl->minimum)
	    tcc[i].value = (((gfloat) value) - ctrl->minimum)
	      / ((gfloat) ctrl->maximum - ctrl->minimum);
	  else
	    tcc[i].value = 0;

	  i++;
	}
    }

  *tccp = tcc;
  *num_controls_p = num_controls;
}

void
zconf_get_sources		(tveng_device_info *	info,
				 gboolean               mute)
{
  gint video_input;
  gint audio_input;
  gint video_standard;
  tveng_tuned_channel *ch;

  video_input = zcg_int (NULL, "current_input");
  if (video_input)
    z_switch_video_input (video_input, info);

  audio_input = zcg_int (NULL, "current_audio_input");
  if (audio_input)
    z_switch_audio_input (audio_input, info);

  video_standard = zcg_int (NULL, "current_standard");
  if (video_standard)
    z_switch_standard (video_standard, info);

  cur_tuned_channel = zcg_int (NULL, "cur_tuned_channel");
  ch = tveng_tuned_channel_nth (global_channel_list,
				(guint) cur_tuned_channel);
  if (NULL != ch)
    {
      if (mute)
	{
	  tveng_tc_control *mute;

	  if ((mute = tveng_tc_control_by_id (info,
					      ch->controls,
					      ch->num_controls,
					      TV_CONTROL_ID_MUTE)))
	    mute->value = 1; /* XXX sub-optimal */
	}

      z_switch_channel (ch, info);
    }
}

void
zconf_set_sources		(tveng_device_info *	info)
{
  const tv_video_line *vl;
  const tv_audio_line *al;
  const tv_video_standard *vs;

  vl = tv_cur_video_input (info);
  zcs_uint (vl ? vl->hash : 0, "current_input");

  al = tv_cur_audio_input (info);
  zcs_uint (al ? al->hash : 0, "current_audio_input");

  vs = tv_cur_video_standard (info);
  zcs_uint (vs ? vs->hash : 0, "current_standard");

  zcs_int (cur_tuned_channel, "cur_tuned_channel");
}





/*
  Substitute the special search keywords by the appropiate thing,
  returns a newly allocated string, and g_free's the given string.
  Valid search keywords:
  $(alias) -> tc->name
  $(index) -> tc->index
  $(id) -> tc->rf_name
*/
static
gchar *substitute_keywords	(gchar		*string,
				 tveng_tuned_channel *tc,
				 const gchar    *default_name)
{
  gint i;
  gchar *found, *buffer = NULL, *p;
  const gchar *search_keys[] =
  {
    "$(alias)",
    "$(index)",
    "$(id)",
    "$(input)",
    "$(standard)",
    "$(freq)",
    "$(title)",
    "$(rating)"
  };
  gint num_keys = sizeof(search_keys)/sizeof(*search_keys);

  if ((!string) || (!*string) || (!tc))
    {
      g_free(string);
      return g_strdup("");
    }

  for (i=0; i<num_keys; i++)
     while ((found = strstr(string, search_keys[i])))
     {
       switch (i)
	 {
	 case 0:
	   if (tc->null_name)
	     buffer = g_strdup(tc->null_name);
	   else if (default_name)
	     buffer = g_strdup(default_name);
	   else
	     buffer = g_strdup(_("Unknown"));
	   break;
	 case 1:
	   buffer = g_strdup_printf("%d", tc->index+1);
	   break;
	 case 2:
	   if (tc->null_rf_name)
	     buffer = g_strdup(tc->null_rf_name);
	   else
	     buffer = g_strdup(_("Unnamed"));
	   break;
	 case 3:
	   {
	     const tv_video_line *l;

	     if ((l = tv_video_input_by_hash (zapping->info, tc->input)))
	       buffer = g_strdup (l->label);
	     else
	       buffer = g_strdup (_("No input"));
	   
	     break;
	   }
	 case 4:
	   {
	     const tv_video_standard *s;

	     if ((s = tv_video_standard_by_hash (zapping->info, tc->standard)))
	       buffer = g_strdup (s->label);
	     else
	       buffer = g_strdup (_("No standard"));

	     break;
	   }
	 case 5:
	   buffer = g_strdup_printf("%d", tc->frequ / 1000);
	   break;
#if 0 /* Temporarily removed. ifdef HAVE_LIBZVBI */
	 case 6: /* title */
	   buffer = zvbi_current_title();
	   break;
	 case 7: /* rating */
	   buffer = g_strdup(zvbi_current_rating());
	   break;
#else
	 case 6: /* title */
	 case 7: /* rating */
	   buffer = g_strdup("");
	   break;
#endif
	 default:
	   g_assert_not_reached();
	   break;
	 }

       *found = 0;
       
       p = g_strconcat(string, buffer,
		       found+strlen(search_keys[i]), NULL);
       g_free(string);
       g_free(buffer);
       string = p;
     }

  return string;
}

void
z_set_main_title	(tveng_tuned_channel	*channel,
			 const gchar *default_name)
{
  tveng_tuned_channel ch;
  gchar *buffer = NULL;

  CLEAR (ch);

  if (!channel)
    channel = &ch;

  if (channel != &ch || channel->null_name || default_name)
    buffer = substitute_keywords(g_strdup(zcg_char(NULL, "title_format")),
				 channel, default_name);
  if (buffer && *buffer && zapping)
    gtk_window_set_title(GTK_WINDOW(zapping), buffer);
  else if (zapping)
    gtk_window_set_title(GTK_WINDOW(zapping), "Zapping");

  g_free(buffer);

  if (channel != &ch)
    cur_tuned_channel = channel->index;
}

/* Do not save the control values in the first switch_channel */
static gboolean first_switch = TRUE;

void
z_switch_channel		(tveng_tuned_channel *	channel,
				 tveng_device_info *	info)
{
  gboolean was_first_switch = first_switch;
  tveng_tuned_channel *tc;
  gboolean in_global_list;
  gboolean avoid_noise;
  const tv_video_line *vi;
  gboolean caption_enabled;

  if (!channel)
    return;

  in_global_list = tveng_tuned_channel_in_list (global_channel_list, channel);

  if (in_global_list &&
      (tc = tveng_tuned_channel_nth (global_channel_list,
				     (guint) cur_tuned_channel)))
    {
      if (!first_switch)
	{
	  g_free(tc->controls);
	  if (zcg_bool(NULL, "save_controls"))
	    store_control_values(info, &tc->controls, &tc->num_controls);
	  else
	    {
	      tc->num_controls = 0;
	      tc->controls = NULL;
	    }
	}
      else
	{
	  first_switch = FALSE;
	}

#ifdef HAVE_LIBZVBI
      tc->caption_pgno = zvbi_caption_pgno;
#endif
    }

  if ((avoid_noise = zcg_bool (NULL, "avoid_noise")))
    tv_quiet_set (zapping->info, TRUE);

  freeze_update();

  /* force rebuild on startup */
  if (was_first_switch)
    zmodel_changed(z_input_model);

  if (channel->input)
    {
    z_switch_video_input(channel->input, info);
    }

  /*
  if (channel->audio_input)
    {
    z_switch_audio_input(channel->audio_input, info);
    }
  */

  if (channel->standard)
    z_switch_standard(channel->standard, info);

  if (avoid_noise)
    reset_quiet (zapping->info, /* delay ms */ 500);

  if ((vi = tv_cur_video_input (info))
      && vi->type == TV_VIDEO_LINE_TYPE_TUNER)
    if (!tv_set_tuner_frequency (info, channel->frequ))
      ShowBox("%s", GTK_MESSAGE_ERROR,
	      tv_get_errstr (info));

  if (in_global_list)
    z_set_main_title(channel, NULL);
  else
    gtk_window_set_title(GTK_WINDOW(zapping), "Zapping");

  thaw_update();

  if (channel->num_controls && zcg_bool(NULL, "save_controls"))
    /* XXX should we save mute state per-channel? */
    load_control_values(info, channel->controls, channel->num_controls);

  update_control_box(info);

#ifdef HAVE_LIBZVBI
  zvbi_caption_pgno = channel->caption_pgno;

  caption_enabled = zconf_get_boolean
    (NULL, "/zapping/internal/callbacks/closed_caption");

  if (caption_enabled
      && zvbi_caption_pgno <= 0)
    {
      zvbi_caption_pgno = zvbi_find_subtitle_page (info);

      /* XXX shall we? It's a little inconvenient, but we don't
	 know what to show on this channel, and deactivating the
	 caption/subtitles button reflects that. TODO: some hint
         that subtitles are available when we receive a ttx
	 page inventory or caption data. */
      if (zvbi_caption_pgno <= 0)
	python_command_printf (NULL, "zapping.closed_caption(0)");
    }

  zvbi_channel_switched();

  if (DISPLAY_MODE_FULLSCREEN == zapping->display_mode
      || DISPLAY_MODE_BACKGROUND == zapping->display_mode)
    osd_render_markup_printf (NULL,
	("<span foreground=\"yellow\">%s</span>"),
			      (NULL == channel->null_name) ?
			      "" : channel->null_name);
#endif

  xawtv_ipc_set_station (GTK_WIDGET (zapping), channel);
}

static void
select_channel			(guint			num_channel)
{
  tveng_tuned_channel * channel =
    tveng_tuned_channel_nth (global_channel_list, num_channel);

  if (!channel)
    {
      g_warning("Cannot tune given channel %d (no such channel)",
		num_channel);
      return;
    }

  z_switch_channel(channel, zapping->info);
}

static gchar			kp_chsel_buf[8];
static gint			kp_chsel_prefix;
static gboolean			kp_clear;
static gboolean			kp_lirc; /* XXX */
static guint			kp_timeout_id = NO_SOURCE_ID;

static gint
channel_txl			(void)
{
  gint txl;

  /* 0 = channel list number, 1 = RF channel number */
  txl = zconf_get_int (NULL, "/zapping/options/main/channel_txl");

  if (txl < 0)
    txl = 0; /* historical: -1 disabled keypad channel number entering */

  return txl;
}

static tveng_tuned_channel *
entered_channel			(gint			txl)
{
  tveng_tuned_channel *tc;

  tc = NULL;

  if (!isdigit (kp_chsel_buf[0]) || txl >= 1)
    tc = tveng_tuned_channel_by_rf_name (global_channel_list, kp_chsel_buf);

  if (NULL == tc)
    tc = tveng_tuned_channel_nth (global_channel_list,
				  strtoul (kp_chsel_buf, NULL, 0));

  return tc;
}

static guint
cur_channel_num			(void)
{
  if (0 != kp_chsel_buf[0])
    {
      tveng_tuned_channel *tc;

      if (kp_timeout_id > 0)
	g_source_remove (kp_timeout_id);

      kp_timeout_id = NO_SOURCE_ID;

      tc = entered_channel (channel_txl ());

      kp_chsel_buf[0] = 0;
      kp_chsel_prefix = 0;

      if (NULL != tc)
	return tc->index;
    }

  return cur_tuned_channel;
}

static PyObject *
py_channel_up			(PyObject *		self,
				 PyObject *		args)
{
  guint n_channels;
  guint new_channel;

  self = self;
  args = args;

  n_channels = tveng_tuned_channel_num (global_channel_list);
  if (0 == n_channels)
    py_return_none;

  new_channel = cur_channel_num () + 1;
  if (new_channel >= n_channels)
    new_channel = 0;

  select_channel (new_channel);

  py_return_none;
}

static PyObject *
py_channel_down			(PyObject *		self,
				 PyObject *		args)
{
  guint n_channels;
  guint cur_channel;
  guint new_channel;

  self = self;
  args = args;

  n_channels = tveng_tuned_channel_num (global_channel_list);
  if (0 == n_channels)
    py_return_none;

  cur_channel = cur_channel_num ();
  if (cur_channel > 0)
    new_channel = cur_channel - 1;
  else
    new_channel = n_channels - 1;

  select_channel (new_channel);

  py_return_none;
}

/* Select a channel by index into the the channel list. */
static PyObject *
py_set_channel			(PyObject *		self,
				 PyObject *		args)
{
  gint n_channels;
  gint i;

  self = self;

  if (!ParseTuple (args, "i", &i))
    py_return_false;

  n_channels = tveng_tuned_channel_num (global_channel_list);
  if (i >= 0 && i < n_channels)
    {
      select_channel ((guint) i);
      py_return_true;
    }

  py_return_false;
}

/* Select a channel by station name ("MSNBCBS", "Linux TV", ...),
   when not found by channel name ("5", "S7", ...) */
static PyObject *
py_lookup_channel		(PyObject *		self,
				 PyObject *		args)
{
  tveng_tuned_channel *tc;
  char *name;

  self = self;

  if (!ParseTuple (args, "s", &name))
    py_return_false;

  tc = tveng_tuned_channel_by_name (global_channel_list, name);

  if (NULL == tc)
    tc = tveng_tuned_channel_by_rf_name (global_channel_list, name);

  if (NULL == tc)
    py_return_false;

  if (0 != kp_chsel_buf[0])
    {
      if (kp_timeout_id > 0)
	g_source_remove (kp_timeout_id);

      kp_timeout_id = NO_SOURCE_ID;

      kp_chsel_buf[0] = 0;
      kp_chsel_prefix = 0;
    }

  z_switch_channel (tc, zapping->info);

  py_return_true;
}

static void
kp_enter			(gint			txl)
{
  tveng_tuned_channel *tc;

  tc = entered_channel (txl);
  if (NULL == tc)
    return;

  z_switch_channel (tc, zapping->info);
}

static void
kp_timeout			(gboolean		timer)
{
  if (timer && kp_chsel_buf[0] != 0)
    kp_enter (channel_txl ());

  if (kp_clear)
    {
      kp_chsel_buf[0] = 0;
      kp_chsel_prefix = 0;
    }
}

#ifndef HAVE_LIBZVBI

static gboolean
kp_timeout2			(gpointer		user_data)
{
  kp_timeout (TRUE);

  kp_timeout_id = NO_SOURCE_ID;

  return FALSE; /* don't call again */
}

#endif

static gboolean
kp_key_press			(GdkEventKey *		event,
				 gint			txl)
{
  if (kp_timeout_id > 0)
    g_source_remove (kp_timeout_id);

  kp_timeout_id = NO_SOURCE_ID;

  switch (event->keyval)
    {
    case GDK_KP_0 ... GDK_KP_9:
      {
	tveng_tuned_channel *tc;
	guint len;

	len = strlen (kp_chsel_buf);

	if (len >= sizeof (kp_chsel_buf) - 1)
	  memcpy (kp_chsel_buf, kp_chsel_buf + 1, len--);

	kp_chsel_buf[len] = event->keyval - GDK_KP_0 + '0';
	kp_chsel_buf[len + 1] = 0;

      show:
	tc = NULL;

	if (txl == 1)
	  {
	    guint match = 0;

	    /* RF channel name completion */

	    len = strlen (kp_chsel_buf);

	    for (tc = tveng_tuned_channel_first (global_channel_list);
		 tc; tc = tc->next)
	    {
	      if (NULL == tc->null_rf_name || tc->null_rf_name[0] == 0)
		{
		  continue;
		}
	      else if (0 == strncmp (tc->null_rf_name, kp_chsel_buf, len))
		{
		  if (strlen (tc->null_rf_name) == len)
		    break; /* exact match */

		  if (match++ > 0)
		    {
		      tc = NULL;
		      break; /* ambiguous */
		    }
		}
	    }

	    if (tc)
	      g_strlcpy (kp_chsel_buf,
			 (NULL == tc->null_rf_name) ? ""
			 : tc->null_rf_name, sizeof (kp_chsel_buf) - 1);
	  }

	kp_clear = FALSE;
#ifdef HAVE_LIBZVBI
	osd_render_markup_printf (kp_timeout,
			   ("<span foreground=\"green\">%s</span>"),
			   kp_chsel_buf);
#else
	kp_timeout_id =
	  g_timeout_add (1500, (GSourceFunc) kp_timeout2, NULL);
#endif
	kp_clear = TRUE;

	if (txl == 0)
	  {
	    guint num = atoi (kp_chsel_buf);

	    /* Switch to channel if the number is unambiguous */

	    if (num == 0
		|| (num * 10) >= tveng_tuned_channel_num (global_channel_list))
	      tc = tveng_tuned_channel_nth (global_channel_list, num);
	  }

	if (!tc)
	  return TRUE; /* unknown channel */

	z_switch_channel (tc, zapping->info);

	kp_chsel_buf[0] = 0;
	kp_chsel_prefix = 0;

	return TRUE;
      }

    case GDK_KP_Decimal:
      if (txl >= 1)
	{
	  const tveng_tuned_channel *tc;
	  const gchar *rf_table;
	  tv_rf_channel ch;
	  const char *prefix;

	  /* Run through all RF channel prefixes incl. nil (== clear) */

	  if (!(rf_table = zconf_get_string (NULL, "/zapping/options/main/current_country")))
	    {
	      tc = tveng_tuned_channel_nth
		(global_channel_list, (guint) cur_tuned_channel);

	      if (!tc || !(rf_table = tc->null_rf_table) || rf_table[0] == 0)
		return TRUE; /* dead key */
	    }

	  if (!tv_rf_channel_table_by_name (&ch, rf_table))
	    return TRUE; /* dead key */

	  if ((prefix = tv_rf_channel_table_prefix (&ch, kp_chsel_prefix)))
	    {
	      g_strlcpy (kp_chsel_buf, prefix, sizeof (kp_chsel_buf) - 1);
	      kp_chsel_buf[sizeof (kp_chsel_buf) - 1] = 0;
	      kp_chsel_prefix++;
	      goto show;
	    }
	}

      kp_clear = TRUE;
#ifdef HAVE_LIBZVBI /* FIXME should no rely on OSD clear time */
      osd_render_markup_printf (kp_timeout,
			 "<span foreground=\"black\">/</span>");
#else
      kp_timeout_id =
	g_timeout_add (1500, (GSourceFunc) kp_timeout2, NULL);
#endif
      return TRUE;
 
    case GDK_KP_Enter:
      kp_enter (txl);

      kp_chsel_buf[0] = 0;
      kp_chsel_prefix = 0;

      return TRUE;

    default:
      break;
    }

  return FALSE; /* don't know, pass it on */
}

gboolean
on_channel_enter			(GtkWidget *	widget _unused_,
					 GdkEventKey *	event,
					 gpointer	user_data _unused_)
{
  if (0 == kp_chsel_buf[0])
    return FALSE;

  return kp_key_press (event, channel_txl ());
}

/*
 * Called from alirc.c, preliminary.
 */
gboolean
channel_key_press		(GdkEventKey *		event)
{
  kp_lirc = TRUE;

  return kp_key_press (event, channel_txl ());
}

gboolean
on_channel_key_press			(GtkWidget *	widget _unused_,
					 GdkEventKey *	event,
					 gpointer	user_data _unused_)
{
  tveng_tuned_channel *tc;
  z_key key;
  guint i;

  if (1) /* XXX !disabled */
    if (kp_key_press (event, channel_txl ()))
      {
	kp_lirc = FALSE;
	return TRUE;
      }

  /* Channel accelerators */

  key.key = gdk_keyval_to_lower (event->keyval);
  key.mask = event->state;

  for (i = 0; (tc = tveng_tuned_channel_nth(global_channel_list, i)); i++)
    if (z_key_equal (tc->accel, key))
      {
	select_channel (tc->index);
	return TRUE;
      }

  return FALSE; /* don't know, pass it on */
}

/* ------------------------------------------------------------------------- */

typedef struct {
  tveng_device_info *	info;
  GtkMenuItem *		menu_item;
  tv_callback *		callback;
} source_menu;

static void
on_menu_item_destroy		(gpointer		user_data)
{
  source_menu *sm = user_data;

  tv_callback_remove (sm->callback);
  g_free (sm);
}

static void
append_radio_menu_item		(GtkMenuShell **	menu_shell,
				 GSList **		group,
				 const gchar *		label,
				 gboolean		active _unused_,
				 GCallback		handler,
				 const source_menu *	sm)
{
  GtkWidget *menu_item;

  menu_item = gtk_radio_menu_item_new_with_label (*group, label);
  gtk_widget_show (menu_item);

  *group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (menu_item));

  gtk_menu_shell_append (*menu_shell, menu_item);

  z_signal_connect_const (G_OBJECT (menu_item), "toggled", handler, sm);
}

/* Video standards */

static void
on_video_standard_activate	(GtkMenuItem *		menu_item,
				 gpointer		user_data);
static GtkWidget *
video_standard_menu		(source_menu *		sm);

static void
select_cur_video_standard_item	(GtkMenuShell *		menu_shell,
				 tveng_device_info *	info)
{
  GtkWidget *menu_item;
  const tv_video_standard *vs;
  guint index;

  if (!(vs = tv_cur_video_standard (info)))
    return;

  index = tv_video_standard_position (info, vs);
  menu_item = z_menu_shell_nth_item (menu_shell, index + 1 /* tear-off */);
  g_assert (menu_item != NULL);

  SIGNAL_HANDLER_BLOCK (menu_item, on_video_standard_activate,
			gtk_check_menu_item_set_active
			(GTK_CHECK_MENU_ITEM (menu_item), TRUE));
}

static void
on_tv_video_standard_change	(tveng_device_info *	info _unused_,
				 void *			user_data)
{
  source_menu *sm = user_data;

  if (!tv_cur_video_standard (sm->info))
    {
      gtk_widget_set_sensitive (GTK_WIDGET (sm->menu_item), FALSE);
      gtk_menu_item_remove_submenu (sm->menu_item);
    }
  else
    {
      GtkWidget *w;

      if (!(w = gtk_menu_item_get_submenu (sm->menu_item)))
	{
	  gtk_widget_set_sensitive (GTK_WIDGET (sm->menu_item), TRUE);
	  gtk_menu_item_set_submenu (sm->menu_item, video_standard_menu (sm));
	}
      else
	{
	  GtkMenuShell *menu_shell;

	  menu_shell = GTK_MENU_SHELL (w);
	  select_cur_video_standard_item (menu_shell, sm->info);
	}
    }
}

static void
on_video_standard_activate	(GtkMenuItem *		menu_item,
				 gpointer		user_data)
{
  const source_menu *sm = user_data;
  GtkMenuShell *menu_shell;
  const tv_video_standard *s;
  gboolean success;
  gint index;

  if (!GTK_CHECK_MENU_ITEM (menu_item)->active)
    return;

  menu_shell = GTK_MENU_SHELL (gtk_menu_item_get_submenu (sm->menu_item));
  if (!menu_shell)
    return;

  index = g_list_index (menu_shell->children, menu_item);

  success = FALSE;

  rebuild_channel_menu = FALSE; /* old stuff */

  if (index >= 1
      && (s = tv_nth_video_standard (sm->info,
				     (guint) index - 1 /* tear-off */)))
    TV_CALLBACK_BLOCK (sm->callback,
		       (success = z_switch_standard (s->hash, zapping->info)));

  rebuild_channel_menu = TRUE;

  if (success)
    {
#ifdef HAVE_LIBZVBI
      zvbi_channel_switched ();
#endif
    }
  else
    {
      select_cur_video_standard_item (menu_shell, sm->info);
    }
}

static GtkWidget *
video_standard_menu		(source_menu *		sm)
{
  const tv_video_standard *s;
  GtkMenuShell *menu_shell;
  GtkWidget *menu_item;
  GSList *group;

  if (!(s = tv_next_video_standard (sm->info, NULL)))
    return NULL;

  menu_shell = GTK_MENU_SHELL (gtk_menu_new ());

  menu_item = gtk_tearoff_menu_item_new ();
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (menu_shell, menu_item);

  group = NULL;

  for (; s; s = tv_next_video_standard (sm->info, s))
    append_radio_menu_item (&menu_shell, &group, s->label,
			    /* active */ s == tv_cur_video_standard (sm->info),
			    G_CALLBACK (on_video_standard_activate), sm);

  select_cur_video_standard_item (menu_shell, sm->info);

  if (!sm->callback)
    sm->callback = tv_add_video_standard_callback
      (sm->info, on_tv_video_standard_change, NULL, sm);

  g_assert (sm->callback != NULL);

  return GTK_WIDGET (menu_shell);
}

/* Audio inputs */

static void
on_audio_input_activate		(GtkMenuItem *		menu_item,
				 gpointer		user_data);
static GtkWidget *
audio_input_menu		(source_menu *		sm);

static void
select_cur_audio_input_item	(GtkMenuShell *		menu_shell,
				 tveng_device_info *	info)
{
  GtkWidget *menu_item;
  const tv_audio_line *ai;
  guint index;

  if (!(ai = tv_cur_audio_input (info)))
    return;

  g_assert (ai != NULL);

  index = tv_audio_input_position (info, ai);
  menu_item = z_menu_shell_nth_item (menu_shell, index + 1 /* tear-off */);
  g_assert (menu_item != NULL);

  SIGNAL_HANDLER_BLOCK (menu_item, on_audio_input_activate,
			gtk_check_menu_item_set_active
			(GTK_CHECK_MENU_ITEM (menu_item), TRUE));
}

static void
on_tv_audio_input_change	(tveng_device_info *	info _unused_,
				 void *			user_data)
{
  source_menu *sm = user_data;

  if (!tv_cur_audio_input (sm->info))
    {
      gtk_widget_set_sensitive (GTK_WIDGET (sm->menu_item), FALSE);
      gtk_menu_item_remove_submenu (sm->menu_item);
    }
  else
    {
      GtkWidget *w;

      if (!(w = gtk_menu_item_get_submenu (sm->menu_item)))
	{
	  gtk_widget_set_sensitive (GTK_WIDGET (sm->menu_item), TRUE);
	  gtk_menu_item_set_submenu (sm->menu_item, audio_input_menu (sm));
	}
      else
	{
	  GtkMenuShell *menu_shell;

	  menu_shell = GTK_MENU_SHELL (w);
	  select_cur_audio_input_item (menu_shell, sm->info);
	}
    }
}

static void
on_audio_input_activate		(GtkMenuItem *		menu_item,
				 gpointer		user_data)
{
  const source_menu *sm = user_data;
  GtkMenuShell *menu_shell;
  const tv_audio_line *l;
  gboolean success;
  gint index;

  if (!GTK_CHECK_MENU_ITEM (menu_item)->active)
    return;

  menu_shell = GTK_MENU_SHELL (gtk_menu_item_get_submenu (sm->menu_item));
  if (!menu_shell)
    return;

  index = g_list_index (menu_shell->children, menu_item);

  success = FALSE;

  rebuild_channel_menu = FALSE; /* old stuff */

  if (index >= 1
      && (l = tv_nth_audio_input (sm->info, (guint) index - 1 /* tear-off */)))
    TV_CALLBACK_BLOCK (sm->callback,
		       (success =
			z_switch_audio_input (l->hash, zapping->info)));

  rebuild_channel_menu = TRUE;

  if (success)
    {
#ifdef HAVE_LIBZVBI
      zvbi_channel_switched ();
#endif
    }
  else
    {
      select_cur_audio_input_item (menu_shell, sm->info);
    }
}

static GtkWidget *
audio_input_menu		(source_menu *		sm)
{
  const tv_audio_line *l;
  GtkMenuShell *menu_shell;
  GtkWidget *menu_item;
  GSList *group;

  if (!(l = tv_next_audio_input (sm->info, NULL)))
    return NULL;

  menu_shell = GTK_MENU_SHELL (gtk_menu_new ());

  menu_item = gtk_tearoff_menu_item_new ();
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (menu_shell, menu_item);

  group = NULL;

  for (; l; l = tv_next_audio_input (sm->info, l))
    append_radio_menu_item (&menu_shell, &group, l->label,
			    /* active */ l == tv_cur_audio_input (sm->info),
			    G_CALLBACK (on_audio_input_activate), sm);

  select_cur_audio_input_item (menu_shell, sm->info);

  if (!sm->callback)
    sm->callback = tv_add_audio_input_callback
      (sm->info, on_tv_audio_input_change, NULL, sm);

  g_assert (sm->callback != NULL);

  return GTK_WIDGET (menu_shell);
}

/* Video inputs */

static void
on_video_input_activate		(GtkMenuItem *		menu_item,
				 gpointer		user_data);
static GtkWidget *
video_input_menu		(source_menu *		sm);

static void
select_cur_video_input_item	(GtkMenuShell *		menu_shell,
				 tveng_device_info *	info)
{
  GtkWidget *menu_item;
  const tv_video_line *vi;
  guint index;

  if (!(vi = tv_cur_video_input(info)))
    return;

  index = tv_video_input_position (info, vi);
  menu_item = z_menu_shell_nth_item (menu_shell, index + 1 /* tear-off */);
  g_assert (menu_item != NULL);

  SIGNAL_HANDLER_BLOCK (menu_item, on_video_input_activate,
			gtk_check_menu_item_set_active
			(GTK_CHECK_MENU_ITEM (menu_item), TRUE));
}

static void
on_tv_video_input_change	(tveng_device_info *	info _unused_,
				 void *			user_data)
{
  source_menu *sm = user_data;

  if (!tv_cur_video_input (sm->info))
    {
      gtk_widget_set_sensitive (GTK_WIDGET (sm->menu_item), FALSE);
      gtk_menu_item_remove_submenu (sm->menu_item);
    }
  else
    {
      GtkWidget *w;

      if (!(w = gtk_menu_item_get_submenu (sm->menu_item)))
	{
	  gtk_widget_set_sensitive (GTK_WIDGET (sm->menu_item), TRUE);
	  gtk_menu_item_set_submenu (sm->menu_item, video_input_menu (sm));
	}
      else
	{
	  GtkMenuShell *menu_shell;

	  menu_shell = GTK_MENU_SHELL (w);
	  select_cur_video_input_item (menu_shell, sm->info);

	  /* Usually standards depend on video input, but we don't rebuild the
	     menu here. That happens automatically above in the video standard
	     callbacks. */
	}
    }
}

static void
on_video_input_activate		(GtkMenuItem *		menu_item,
				 gpointer		user_data)
{
  const source_menu *sm = user_data;
  GtkMenuShell *menu_shell;
  const tv_video_line *l;
  gboolean success;
  gint index;

  if (!GTK_CHECK_MENU_ITEM (menu_item)->active)
    return;

  menu_shell = GTK_MENU_SHELL (gtk_menu_item_get_submenu (sm->menu_item));
  if (!menu_shell)
    return;

  index = g_list_index (menu_shell->children, menu_item);

  success = FALSE;

  rebuild_channel_menu = FALSE; /* old stuff */

  if (index >= 1
      && (l = tv_nth_video_input (sm->info, (guint) index - 1 /* tear-off */)))
    TV_CALLBACK_BLOCK (sm->callback,
		       (success =
			z_switch_video_input (l->hash, zapping->info)));

  rebuild_channel_menu = TRUE;

  if (success)
    {
#ifdef HAVE_LIBZVBI
      zvbi_channel_switched ();
#endif
    }
  else
    {
      select_cur_video_input_item (menu_shell, sm->info);
    }
}

static GtkWidget *
video_input_menu		(source_menu *		sm)
{
  const tv_video_line *l;
  GtkMenuShell *menu_shell;
  GtkWidget *menu_item;
  GSList *group;

  if (!(l = tv_next_video_input (sm->info, NULL)))
    return NULL;

  menu_shell = GTK_MENU_SHELL (gtk_menu_new ());

  menu_item = gtk_tearoff_menu_item_new ();
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (menu_shell, menu_item);

  group = NULL;

  for (; l; l = tv_next_video_input (sm->info, l))
    append_radio_menu_item (&menu_shell, &group, l->label,
			    /* active */ l == tv_cur_video_input (sm->info),
			    G_CALLBACK (on_video_input_activate), sm);

  select_cur_video_input_item (menu_shell, sm->info);

  if (!sm->callback)
    sm->callback = tv_add_video_input_callback
      (sm->info, on_tv_video_input_change, NULL, sm);

  g_assert (sm->callback != NULL);

  return GTK_WIDGET (menu_shell);
}

static void
add_source_items		(GtkMenuShell *		menu,
				 gint			pos,
				 tveng_device_info *	info)
{
  source_menu *sm;
  GtkWidget *item;

  {
    sm = g_malloc0 (sizeof (*sm));
    sm->info = info;

    item = z_gtk_pixmap_menu_item_new (_("Video standards"),
				     GTK_STOCK_SELECT_COLOR);
    gtk_widget_show (item);

    sm->menu_item = GTK_MENU_ITEM (item);
    g_object_set_data_full (G_OBJECT (item), "sm", sm,
			    (GtkDestroyNotify) on_menu_item_destroy);

    gtk_menu_shell_insert (menu, item, pos);

    if ((item = video_standard_menu (sm)))
      gtk_menu_item_set_submenu (sm->menu_item, item);
    else
      gtk_widget_set_sensitive (GTK_WIDGET (sm->menu_item), FALSE);
  }

  {
    sm = g_malloc0 (sizeof (*sm));
    sm->info = info;

    item = z_gtk_pixmap_menu_item_new (_("Audio inputs"),
				       "gnome-stock-line-in");
    gtk_widget_show (item);

    sm->menu_item = GTK_MENU_ITEM (item);
    g_object_set_data_full (G_OBJECT (item), "sm", sm,
			    (GtkDestroyNotify) on_menu_item_destroy);

    gtk_menu_shell_insert (menu, item, pos);

    if ((item = audio_input_menu (sm)))
      gtk_menu_item_set_submenu (sm->menu_item, item);
    else
      gtk_widget_set_sensitive (GTK_WIDGET (sm->menu_item), FALSE);
  }

  {
    sm = g_malloc0 (sizeof (*sm));
    sm->info = info;

    item = z_gtk_pixmap_menu_item_new (_("Video inputs"),
				       "gnome-stock-line-in");
    gtk_widget_show (item);

    sm->menu_item = GTK_MENU_ITEM (item);
    g_object_set_data_full (G_OBJECT (item), "sm", sm,
			    (GtkDestroyNotify) on_menu_item_destroy);

    gtk_menu_shell_insert (menu, item, pos);

    if ((item = video_input_menu (sm)))
      gtk_menu_item_set_submenu (sm->menu_item, item);
    else
      gtk_widget_set_sensitive (GTK_WIDGET (sm->menu_item), FALSE);
  }
}

/* ------------------------------------------------------------------------- */

static inline tveng_tuned_channel *
nth_channel			(guint			index)
{
  tveng_tuned_channel *tc;
  guint i;

  for (tc = global_channel_list, i = 0; tc; tc = tc->next)
    if (tc->null_name && tc->null_name[0])
      if (i++ == index)
	break;

  return tc;
}

static inline void
insert_one_channel		(GtkMenuShell *		menu,
				 guint			index,
				 gint			pos)
{
  tveng_tuned_channel *tc;
  GtkWidget *menu_item;
  gchar *tooltip;

  if (!(tc = nth_channel (index)))
    return;

  menu_item = z_gtk_pixmap_menu_item_new (NULL == tc->null_name ? "" :
					  tc->null_name, GTK_STOCK_PROPERTIES);

  g_signal_connect_swapped (G_OBJECT (menu_item), "activate",
			    G_CALLBACK (select_channel),
			    GINT_TO_POINTER (index));

  if ((tooltip = z_key_name (tc->accel)))
    {
      z_tooltip_set (menu_item, tooltip);
      g_free (tooltip);
    }

  gtk_widget_show (menu_item);
  gtk_menu_shell_insert (menu, menu_item, pos);
}

static inline const gchar *
tuned_channel_nth_name		(guint			index)
{
  tveng_tuned_channel *tc;

  tc = nth_channel (index);

  /* huh? */
  if (!tc || !tc->null_name)
    return _("Unnamed");
  else
    return tc->null_name;
}

/* Returns whether something (useful) was added */
gboolean
add_channel_entries			(GtkMenuShell *menu,
					 gint pos,
					 guint menu_max_entries,
					 tveng_device_info *info)
{
  const guint ITEMS_PER_SUBMENU = 20;
  gboolean sth = FALSE;
  guint num_channels;

  add_source_items (menu, pos, info);

  num_channels = tveng_tuned_channel_num (global_channel_list);

  if (num_channels == 0)
    {
      /* This doesn't count as something added */

      GtkWidget *menu_item;

      /* TRANSLATORS: This is displayed in the channel menu when
         the channel list is empty. */
      menu_item = z_gtk_pixmap_menu_item_new (_("No channels"),
					      GTK_STOCK_CLOSE);
      gtk_widget_set_sensitive (menu_item, FALSE);
      gtk_widget_show (menu_item);
      gtk_menu_shell_insert (menu, menu_item, pos);
    }
  else
    {
      {
	GtkWidget *menu_item;

	/* Separator */

	menu_item = gtk_menu_item_new ();
	gtk_widget_show (menu_item);
	gtk_menu_shell_insert (menu, menu_item, pos);
      }

      sth = TRUE;

      if (num_channels <= ITEMS_PER_SUBMENU
	  && num_channels <= menu_max_entries)
	{
	  while (num_channels > 0)
	    insert_one_channel (menu, --num_channels, pos);
	}
      else
	{
	  while (num_channels > 0)
	    {
	      guint remainder = num_channels % ITEMS_PER_SUBMENU;

	      if (remainder == 1)
		{
		  insert_one_channel (menu, --num_channels, pos);
		}
	      else
		{
		  const gchar *first_name;
		  const gchar *last_name;
		  gchar *buf;
		  GtkMenuShell *submenu;
		  GtkWidget *menu_item;

		  if (remainder == 0)
		    remainder = ITEMS_PER_SUBMENU;

		  first_name = tuned_channel_nth_name (num_channels - remainder);
		  last_name = tuned_channel_nth_name (num_channels - 1);
		  buf = g_strdup_printf ("%s/%s", first_name, last_name);
		  menu_item = z_gtk_pixmap_menu_item_new (buf, "gnome-stock-line-in");
		  g_free (buf);
		  gtk_widget_show (menu_item);
		  gtk_menu_shell_insert (menu, menu_item, pos);

		  submenu = GTK_MENU_SHELL (gtk_menu_new());
		  gtk_widget_show (GTK_WIDGET (submenu));
		  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item),
					     GTK_WIDGET (submenu));

		  menu_item = gtk_tearoff_menu_item_new ();
		  gtk_widget_show (menu_item);
		  gtk_menu_shell_append (submenu, menu_item);

		  while (remainder-- > 0)
		    insert_one_channel (submenu, --num_channels, 1);
		}
	    }
	}
    }

  return sth;
}

/* ------------------------------------------------------------------------- */

gdouble
videostd_inquiry(void)
{
  GtkWidget *dialog, *option, *check;
  gint std_hint;

  std_hint = zconf_get_int (NULL, "/zapping/options/main/std_hint");

  if (std_hint >= 1)
    goto ok;

  dialog = build_widget ("videostd_inquiry", NULL);
  option = lookup_widget (dialog, "optionmenu24");
  check = lookup_widget (dialog, "checkbutton15");

  if (GTK_RESPONSE_ACCEPT != gtk_dialog_run (GTK_DIALOG (dialog)))
    {
      gtk_widget_destroy(dialog);
      return -1;
    }

  std_hint = 1 + z_option_menu_get_active (option);

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check)))
    zconf_set_int (std_hint, "/zapping/options/main/std_hint");

 ok:
  if (std_hint == 1)
    return 30000.0 / 1001;
  else if (std_hint >= 2)
    return 25.0;
  else
    return -1;
}

/*
 *  Preliminary. This should create an object such that we can write:
 *  zapping.control('volume').value += 1;
 *  But for now I just copied py_volume_incr().
 */
static PyObject*
py_control_incr			(PyObject *self _unused_, PyObject *args)
{
  static const struct {
    tv_control_id	id;
    const gchar *	name;
  } controls[] = {
    { TV_CONTROL_ID_BRIGHTNESS,	"brightness" },
    { TV_CONTROL_ID_CONTRAST,	"contrast" },
    { TV_CONTROL_ID_SATURATION,	"saturation" },
    { TV_CONTROL_ID_HUE,	"hue" },
    { TV_CONTROL_ID_MUTE,	"mute" },
    { TV_CONTROL_ID_VOLUME,	"volume" },
    { TV_CONTROL_ID_BASS,	"bass" },
    { TV_CONTROL_ID_TREBLE,	"treble" },
  };
  char *control_name;
  int increment, ok;
  tv_control *tc;
  guint i;

  increment = +1;

  ok = ParseTuple (args, "s|i", &control_name, &increment);

  if (!ok)
    g_error ("zapping.control_incr(s|i)");

  for (i = 0; i < N_ELEMENTS (controls); i++)
    if (0 == strcmp (controls[i].name, control_name))
      break;

  if (i >= N_ELEMENTS (controls))
    goto done;

  tc = NULL;

  while ((tc = tv_next_control (zapping->info, tc)))
    if (tc->id == controls[i].id)
      break;

  if (!tc)
    goto done;

  switch (tc->type)
    {
    case TV_CONTROL_TYPE_INTEGER:
    case TV_CONTROL_TYPE_BOOLEAN:
    case TV_CONTROL_TYPE_CHOICE:
      break;

    default:
      goto done;
    }

  if (tc->id == TV_CONTROL_ID_MUTE)
    {
      set_mute ((increment > 0) ? TRUE : FALSE, TRUE, TRUE);
    }
  else
    {
      if (-1 == tveng_update_control ((tv_control *) tc, zapping->info))
	goto done;

      tveng_set_control ((tv_control *) tc,
			 tc->value + increment * tc->step, zapping->info);

#ifdef HAVE_LIBZVBI
      osd_render_markup_printf (NULL,
				("<span foreground=\"blue\">%s %d %%</span>"),
				tc->label, (tc->value - tc->minimum)
				* 100 / (tc->maximum - tc->minimum));
#endif
    }

 done:
  Py_INCREF(Py_None);

  return Py_None;
}

void
startup_v4linterface(tveng_device_info *info)
{
  z_input_model = ZMODEL(zmodel_new());

  g_signal_connect(G_OBJECT(z_input_model), "changed",
		     G_CALLBACK(update_bundle),
		     info);

  cmd_register ("channel_up", py_channel_up, METH_VARARGS,
		("Switch to higher channel"), "zapping.channel_up()");
  cmd_register ("channel_down", py_channel_down, METH_VARARGS,
		("Switch to lower channel"), "zapping.channel_down()");
  cmd_register ("set_channel", py_set_channel, METH_VARARGS);
  cmd_register ("lookup_channel", py_lookup_channel, METH_VARARGS);
  cmd_register ("control_box", py_control_box, METH_VARARGS,
		("Control window"), "zapping.control_box()");
  cmd_register ("control_incr", py_control_incr, METH_VARARGS,
		("Increase brightness"), "zapping.control_incr('brightness',+1)",
		("Decrease brightness"), "zapping.control_incr('brightness',-1)",
		("Increase hue"), "zapping.control_incr('hue',+1)",
		("Decrease hue"), "zapping.control_incr('hue',-1)",
		("Increase contrast"), "zapping.control_incr('contrast',+1)",
		("Decrease contrast"), "zapping.control_incr('contrast',-1)",
		("Increase saturation"), "zapping.control_incr('saturation',+1)",
		("Decrease saturation"), "zapping.control_incr('saturation',-1)",
		("Increase volume"), "zapping.control_incr('volume',+1)",
		("Decrease volume"), "zapping.control_incr('volume',-1)");

  zcc_char("$(alias) - Zapping", "Title format Z will use", "title_format");
  zcc_bool(FALSE, "Swap the page Up/Down bindings", "swap_up_down");
/*
  zcc_zkey (zkey_from_name ("Page_Up"), "Channel up key", "acc_channel_up");
  zcc_zkey (zkey_from_name ("Page_Down"), "Channel down key", "acc_channel_down");
*/
}

void
shutdown_v4linterface(void)
{
  g_object_unref(G_OBJECT(z_input_model));
}

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
