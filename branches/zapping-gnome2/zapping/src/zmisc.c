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

#include "site_def.h"

#include <gdk/gdkx.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>

#define ZCONF_DOMAIN "/zapping/options/main/"
#include "zmisc.h"
#include "zconf.h"
#include "tveng.h"
#include "interface.h"
#include "callbacks.h"
#include "x11stuff.h"
#include "overlay.h"
#include "capture.h"
#include "fullscreen.h"
#include "v4linterface.h"
#include "ttxview.h"
#include "zvbi.h"
#include "osd.h"
#include "remote.h"
#include "keyboard.h"
#include "globals.h"
#include "audio.h"
#include "mixer.h"
#include "zvideo.h"

extern tveng_device_info * main_info;
extern volatile gboolean flag_exit_program;
extern gint disable_preview; /* TRUE if preview won't work */
extern gboolean xv_present;

gchar*
Prompt (GtkWidget *main_window, const gchar *title,
	const gchar *prompt,  const gchar *default_text)
{
  GtkWidget * dialog;
  GtkBox *vbox;
  GtkWidget *label, *entry;
  gchar *buffer = NULL;

  dialog = gtk_dialog_new_with_buttons
    (title, GTK_WINDOW (main_window),
     GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
     GTK_STOCK_OK, GTK_RESPONSE_OK,
     NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  vbox = GTK_BOX (GTK_DIALOG (dialog) -> vbox);

  if (prompt)
    {
      label = gtk_label_new (prompt);
      gtk_box_pack_start_defaults (vbox, label);
      gtk_widget_show(label);
    }
  entry = gtk_entry_new();
  gtk_box_pack_start_defaults(GTK_BOX(vbox), entry);
  gtk_widget_show(entry);
  gtk_widget_grab_focus(entry);
  if (default_text)
    {
      gtk_entry_set_text (GTK_ENTRY(entry), default_text);
      gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
    }

  z_entry_emits_response (entry, GTK_DIALOG (dialog),
			  GTK_RESPONSE_OK);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    buffer = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));

  gtk_widget_destroy(dialog);
  
  return buffer;
}

GtkWidget * z_gtk_pixmap_menu_item_new(const gchar * mnemonic,
				       const gchar * icon)
{
  GtkWidget * imi;
  GtkWidget * image;

  imi = gtk_image_menu_item_new_with_mnemonic (mnemonic);

  if (icon)
    {
      image = gtk_image_new_from_stock (icon, GTK_ICON_SIZE_MENU);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (imi),
				     image);
      /* not sure whether this is necessary, but won't harm */
      gtk_widget_show (image);
    }

  return (imi);
}

/*
 *  Zapping Global Tooltips
 */

static GList *			tooltips_list = NULL;
static GtkTooltips *		tooltips_default = NULL;
static gboolean			tooltips_enabled = TRUE;

static void
tooltips_destroy_notify		(gpointer	data,
				 GObject	*where_the_object_was)
{
  g_list_remove (tooltips_list, data);
}

GtkTooltips *
z_tooltips_add			(GtkTooltips *		tips)
{
  if (!tips)
    tips = gtk_tooltips_new (); /* XXX destroy at exit */

  tooltips_list = g_list_append (tooltips_list, (gpointer) tips);

  g_object_weak_ref (G_OBJECT (tips),
		     tooltips_destroy_notify,
		     (gpointer) tips);

  if (tooltips_enabled)
    gtk_tooltips_enable (tips);
  else
    gtk_tooltips_disable (tips);

  return tips;
}

void
z_tooltips_active		(gboolean		enable)
{
  GList *list;

  tooltips_enabled = enable;

  for (list = tooltips_list; list; list = list->next)
    {
      if (enable)
	gtk_tooltips_enable (GTK_TOOLTIPS (list->data));
      else
	gtk_tooltips_disable (GTK_TOOLTIPS (list->data));
    }
}

void
z_tooltip_set			(GtkWidget *		widget,
				 const gchar *		tip_text)
{
  if (!tooltips_default)
    tooltips_default = z_tooltips_add (NULL);

#ifndef ZMISC_TOOLTIP_WARNING
#define ZMISC_TOOLTIP_WARNING 0
#endif

  if (ZMISC_TOOLTIP_WARNING && GTK_WIDGET_NO_WINDOW(widget))
    fprintf(stderr, "Warning: tooltip <%s> for "
            "widget without window\n", tip_text);

  gtk_tooltips_set_tip (tooltips_default, widget, tip_text, "private tip");
}

GtkWidget *
z_tooltip_set_wrap		(GtkWidget *		widget,
				 const gchar *		tip_text)
{
  if (!tooltips_default)
    tooltips_default = z_tooltips_add (NULL);

  if (GTK_WIDGET_NO_WINDOW(widget))
    {
      GtkWidget *event_box = gtk_event_box_new ();

      gtk_widget_show (widget);
      gtk_container_add (GTK_CONTAINER (event_box), widget);
      widget = event_box;
    }

  gtk_tooltips_set_tip (tooltips_default, widget, tip_text, "private tip");

  return widget;
}

void
z_set_sensitive_with_tooltip	(GtkWidget *		widget,
				 gboolean		sensitive,
				 const gchar *		on_tip,
				 const gchar *		off_tip)
{
  const gchar *new_tip;

  if (!tooltips_default)
    tooltips_default = z_tooltips_add (NULL);

  gtk_widget_set_sensitive (widget, sensitive);

  new_tip = sensitive ? on_tip : off_tip; /* can be NULL */

  gtk_tooltips_set_tip (tooltips_default, widget, new_tip, NULL);
}

/**************************************************************************/
int
zmisc_restore_previous_mode(tveng_device_info * info)
{
  return zmisc_switch_mode(zcg_int(NULL, "previous_mode"), info);
}

void
zmisc_stop (tveng_device_info *info)
{
  /* Stop current capture mode */
  switch (info->current_mode)
    {
    case TVENG_CAPTURE_PREVIEW:
      /* fullscreen overlay */
      tveng_stop_previewing(info);
      fullscreen_stop(info);
      break;
    case TVENG_CAPTURE_READ:
      /* capture windowed */
      capture_stop();
      video_uninit ();
      tveng_stop_capturing(info);
      break;
    case TVENG_CAPTURE_WINDOW:
      /* overlay windowed */
      stop_overlay ();
      break;
    case TVENG_NO_CAPTURE:
      break;
    default:
      g_assert_not_reached ();
      break;
    }
}

#define BLANK_CURSOR_TIMEOUT 1500 /* ms */

/*
  does the mode switching. Since this requires more than just using
  tveng, a new routine is needed.
  Returns whatever tveng returns, but we print the message ourselves
  too, so no need to aknowledge it to the user.
*/
int
zmisc_switch_mode(enum tveng_capture_mode new_mode,
		  tveng_device_info * info)
{
  GtkWidget * tv_screen;
  int return_value = 0;
  gint x, y, w, h;
  enum tveng_frame_pixformat format;
  gchar * old_input = NULL;
  gchar * old_standard = NULL;
  enum tveng_capture_mode mode;
  extern int disable_overlay;
  gint muted;

  g_assert(info != NULL);
  g_assert(main_window != NULL);
  tv_screen = lookup_widget(main_window, "tv-screen");
  g_assert(tv_screen != NULL);

  if ((info->current_mode == new_mode) &&
      (new_mode != TVENG_NO_CAPTURE))
    {
      x11_screensaver_set (X11_SCREENSAVER_DISPLAY_ACTIVE);
      z_video_blank_cursor (Z_VIDEO (tv_screen),
			    (mode == TVENG_NO_CAPTURE) ?
			    0 : BLANK_CURSOR_TIMEOUT);
      return 0; /* success */
    }

  /* save this input name for later retrieval */
  if (info->cur_video_input)
    old_input = g_strdup (info->cur_video_input->label);
  if (info->cur_video_standard)
    old_standard = g_strdup(info->cur_video_standard->label);

  gdk_window_get_geometry(tv_screen->window, NULL, NULL, &w, &h, NULL);
  gdk_window_get_origin(tv_screen->window, &x, &y);

#if 0 /* XXX should use quiet_set, but that can't handle yet
         how the controls are rebuilt when switching btw
         v4l <-> xv. */
  if ((avoid_noise = zcg_bool (NULL, "avoid_noise")))

    tv_quiet_set (main_info, TRUE);
#else
  muted = tv_mute_get (main_info, FALSE);
#endif

  mode = info->current_mode;

  zmisc_stop (info);

#ifdef HAVE_LIBZVBI
  if (!flag_exit_program)
    {
      GtkWidget *button = lookup_widget (main_window, "videotext3");
      GtkWidget *pixmap;

      if (new_mode != TVENG_NO_CAPTURE)
	{
	  gtk_widget_hide (lookup_widget (main_window, "appbar2"));

	  ttxview_detach (main_window);
#warning
	  //	  pixmap = gtk_image_new_from_stock ("zapping-teletext",
	  //					     GTK_ICON_SIZE_BUTTON);
	  pixmap=0;

	  if (pixmap)
	    {
	      gtk_widget_show (pixmap);
	      gtk_container_remove (GTK_CONTAINER (button),
	                            gtk_bin_get_child (GTK_BIN (button)));
	      gtk_container_add (GTK_CONTAINER (button), pixmap);
	    }
	  else
	    {
	      set_stock_pixmap (button, GTK_STOCK_JUSTIFY_FILL);
	    }

	  z_tooltip_set (button, _("Use Zapping as a Teletext navigator"));
	}
      else
	{
	  set_stock_pixmap (button, GTK_STOCK_REDO);
	  z_tooltip_set (button, _("Return to windowed mode and use the current "
			   "page as subtitles"));
	}
    }
#endif /* HAVE_LIBZVBI */

  if (new_mode != TVENG_CAPTURE_PREVIEW &&
      new_mode != TVENG_NO_CAPTURE)
    osd_set_window(tv_screen);
  else if (new_mode == TVENG_NO_CAPTURE)
    {
      osd_clear();
      osd_unset_window();
    }

  switch (new_mode)
    {
    case TVENG_CAPTURE_READ:
      if (info->current_controller == TVENG_CONTROLLER_XV ||
	  info->current_controller == TVENG_CONTROLLER_NONE)
	{
	  if (info->current_controller != TVENG_CONTROLLER_NONE)
	    tveng_close_device(info);
	  if (tveng_attach_device(zcg_char(NULL, "video_device"),
				  TVENG_ATTACH_READ, info) == -1)
	    {
	      /* Try restoring as XVideo as a last resort */
	      if (tveng_attach_device(zcg_char(NULL, "video_device"),
				      TVENG_ATTACH_XV, info) == -1)
		{
		  RunBox("%s couldn't be opened\n:%s,\naborting",
			 GTK_MESSAGE_ERROR,
			 zcg_char(NULL, "video_device"), info->error);
		  exit(1);
		}
	      else
		ShowBox("Capture mode not available:\n%s",
			GTK_MESSAGE_ERROR, info->error);
	    }
	}
      tveng_set_capture_size(w, h, info);
      return_value = capture_start(info);
      video_init (tv_screen, tv_screen->style->black_gc);
      video_suggest_format ();
      x11_screensaver_set (X11_SCREENSAVER_DISPLAY_ACTIVE
			   | X11_SCREENSAVER_CPU_ACTIVE);
      z_video_blank_cursor (Z_VIDEO (tv_screen), BLANK_CURSOR_TIMEOUT);
      break;
    case TVENG_CAPTURE_WINDOW:
      if (disable_preview || disable_overlay) {
	ShowBox("preview has been disabled", GTK_MESSAGE_WARNING);
	g_free(old_input);
	g_free(old_standard);
	x11_screensaver_set (X11_SCREENSAVER_ON);
        z_video_blank_cursor (Z_VIDEO (tv_screen), 0);
	return -1;
      }

      if (info->current_controller != TVENG_CONTROLLER_XV &&
	  xv_present)
	{
	  tveng_close_device(info);
	  if (tveng_attach_device(zcg_char(NULL, "video_device"),
				  TVENG_ATTACH_XV, info)==-1)
	    {
	      RunBox("%s couldn't be opened\n:%s, aborting",
		     GTK_MESSAGE_ERROR,
		     zcg_char(NULL, "video_device"), info->error);
	      exit(1);
	    }
	}

      if (info->current_controller != TVENG_CONTROLLER_XV)
	{
	  tv_overlay_buffer dma;

	  if (!tv_get_overlay_buffer (info, &dma))
	    {
	      ShowBox(_("Preview will not work: %s"),
		      GTK_MESSAGE_ERROR, info->error);
	      x11_screensaver_set (X11_SCREENSAVER_ON);
	      z_video_blank_cursor (Z_VIDEO (tv_screen), 0);
	      return -1;
	    }
	}

      format = zmisc_resolve_pixformat(tveng_get_display_depth(info),
				       x11_get_byte_order());

      if ((format != -1) &&
	  (info->current_controller != TVENG_CONTROLLER_XV))
	{
	  info->format.pixformat = format;
XX();
	  if ((tveng_set_capture_format(info) == -1) ||
	      (info->format.pixformat != format))
	    g_warning("Preview format invalid: %s (%d, %d)", info->error,
		      info->format.pixformat, format);
	  printv("prev: setting %d, got %d\n", format,
		 info->format.pixformat);
	}

      if ((x + w) <= 0 || (y + h) <= 0)
	goto oops;

      if (start_overlay (main_window, tv_screen, info))
	{
	  x11_screensaver_set (X11_SCREENSAVER_DISPLAY_ACTIVE);
          z_video_blank_cursor (Z_VIDEO (tv_screen), BLANK_CURSOR_TIMEOUT);
	}
      else
	{
	  g_warning(info->error);
	oops:
	  x11_screensaver_set (X11_SCREENSAVER_ON);
          z_video_blank_cursor (Z_VIDEO (tv_screen), 0);
	}
      break;

    case TVENG_CAPTURE_PREVIEW:
      if (disable_preview || disable_overlay) {
	ShowBox("preview has been disabled", GTK_MESSAGE_WARNING);
	g_free(old_input);
	g_free(old_standard);
	x11_screensaver_set (X11_SCREENSAVER_ON);
	z_video_blank_cursor (Z_VIDEO (tv_screen), 0);
	return -1;
      }

      if (info->current_controller != TVENG_CONTROLLER_XV &&
	  xv_present)
	{
	  tveng_close_device(info);
	  if (tveng_attach_device(zcg_char(NULL, "video_device"),
				  TVENG_ATTACH_XV, info)==-1)
	    {
	      RunBox("%s couldn't be opened\n:%s, aborting",
		     GTK_MESSAGE_ERROR,
		     zcg_char(NULL, "video_device"), info->error);
	      exit(1);
	    }
	}

      format = zmisc_resolve_pixformat(tveng_get_display_depth(info),
				       x11_get_byte_order());

      if ((format != -1) &&
	  (info->current_controller != TVENG_CONTROLLER_XV))
	{
	  info->format.pixformat = format;
XX();
	  if ((tveng_set_capture_format(info) == -1) ||
	      (info->format.pixformat != format))
	    g_warning("Fullscreen format invalid: %s (%d, %d)", info->error,
		      info->format.pixformat, format);
	  printv("fulls: setting %d, got %d\n", format,
		 info->format.pixformat);
	}

      if (-1 == (return_value = fullscreen_start (info)))
	{
	  g_warning("couldn't start fullscreen mode");
	  x11_screensaver_set (X11_SCREENSAVER_ON);
          z_video_blank_cursor (Z_VIDEO (tv_screen), 0);
	}
      else
	{
	  x11_screensaver_set (X11_SCREENSAVER_DISPLAY_ACTIVE);
          z_video_blank_cursor (Z_VIDEO (tv_screen), BLANK_CURSOR_TIMEOUT);
	}
      break;
    default:
      x11_screensaver_set (X11_SCREENSAVER_ON);
      z_video_blank_cursor (Z_VIDEO (tv_screen), 0);

      if (!flag_exit_program) /* Just closing */
	{
#ifdef HAVE_LIBZVBI
	  if (zvbi_get_object())
	    {
	      /* start vbi code */
	      gtk_widget_show(lookup_widget(main_window, "appbar2"));
	      ttxview_attach(main_window, lookup_widget(main_window, "tv-screen"),
			     lookup_widget(main_window, "toolbar1"),
			     lookup_widget(main_window, "appbar2"));
	    }
	  else
#endif
	    {
	      ShowBox(_("VBI has been disabled, or it doesn't work."),
		      GTK_MESSAGE_INFO);
	      break;
	    }
	}
      break; /* TVENG_NO_CAPTURE */
    }

  /* Restore old input if we found it earlier */
  if (old_input != NULL)
    if (-1 == tveng_set_input_by_name(old_input, info))
      g_warning("couldn't restore old input");
  if (old_standard != NULL)
    if (-1 == tveng_set_standard_by_name(old_standard, info))
      g_warning("couldn't restore old standard");

  g_free (old_input);
  g_free (old_standard);

  if (mode != new_mode)
    zcs_int(mode, "previous_mode");

  /* Update the standards, channels, etc */
  zmodel_changed(z_input_model);
  /* Updating the properties is not so useful, and it isn't so easy,
     since there might be multiple properties dialogs open */

#if 0
  /* XXX don't reset when we're in shutdown, see cmd.c/py_quit(). */
  if (avoid_noise && !flag_exit_program)
    reset_quiet (main_info, /* delay ms */ 300);
#else
  if (muted != -1)
    tv_mute_set (main_info, muted);
#endif

  /* Update the controls window if it's open */
  update_control_box(info);

  /* Find optimum size for widgets */
  gtk_widget_queue_resize(main_window);

  return return_value;
}

void set_stock_pixmap	(GtkWidget	*button,
			 const gchar	*new_pix)
{
  gtk_button_set_use_stock (GTK_BUTTON (button), TRUE);
  gtk_button_set_label (GTK_BUTTON (button), new_pix);
}

/**
 * Just like gdk_pixbuf_copy_area but does clipping.
 */
void
z_pixbuf_copy_area		(GdkPixbuf	*src_pixbuf,
				 gint		src_x,
				 gint		src_y,
				 gint		width,
				 gint		height,
				 GdkPixbuf	*dest_pixbuf,
				 gint		dest_x,
				 gint		dest_y)
{
  gint src_w = gdk_pixbuf_get_width(src_pixbuf);
  gint src_h = gdk_pixbuf_get_height(src_pixbuf);
  gint dest_w = gdk_pixbuf_get_width(dest_pixbuf);
  gint dest_h = gdk_pixbuf_get_height(dest_pixbuf);

  if (src_x < 0)
    {
      width += src_x;
      dest_x -= src_x;
      src_x = 0;
    }
  if (src_y < 0)
    {
      height += src_y;
      dest_y -= src_y;
      src_y = 0;
    }

  if (src_x + width > src_w)
    width = src_w - src_x;
  if (src_y + height > src_h)
    height = src_h - src_y;

  if (dest_x < 0)
    {
      src_x -= dest_x;
      width += dest_x;
      dest_x = 0;
    }
  if (dest_y < 0)
    {
      src_y -= dest_y;
      height += dest_y;
      dest_y = 0;
    }

  if (dest_x + width > dest_w)
    width = dest_w - dest_x;
  if (dest_y + height > dest_h)
    height = dest_h - dest_y;

  if ((width <= 0) || (height <= 0))
    return;

  gdk_pixbuf_copy_area(src_pixbuf, src_x, src_y, width, height,
		       dest_pixbuf, dest_x, dest_y);
}

void
z_pixbuf_render_to_drawable	(GdkPixbuf	*pixbuf,
				 GdkWindow	*window,
				 GdkGC		*gc,
				 gint		x,
				 gint		y,
				 gint		width,
				 gint		height)
{
  gint w, h;

  if (!pixbuf)
    return;

  w = gdk_pixbuf_get_width(pixbuf);
  h = gdk_pixbuf_get_height(pixbuf);

  if (x < 0)
    {
      width += x;
      x = 0;
    }
  if (y < 0)
    {
      height += 0;
      y = 0;
    }

  if (x + width > w)
    width = w - x;
  if (y + height > h)
    height = h - y;

  if (width < 0 || height < 0)
    return;

  gdk_pixbuf_render_to_drawable(pixbuf, window, gc, x, y, x, y, width,
				height, GDK_RGB_DITHER_NORMAL, x, y);
}

gint
z_menu_get_index		(GtkWidget	*menu,
				 GtkWidget	*item)
{
  gint return_value =
    g_list_index(GTK_MENU_SHELL(menu)->children, item);

  return return_value ? return_value : -1;
}

GtkWidget *
z_menu_shell_nth_item		(GtkMenuShell *		menu_shell,
				 guint			n)
{
  GList *list;

  list = g_list_nth (menu_shell->children, n);
  assert (list != NULL);

  return GTK_WIDGET (list->data);
}


gint
z_option_menu_get_active	(GtkWidget	*option_menu)
{
  return gtk_option_menu_get_history (GTK_OPTION_MENU (option_menu));
}

void
z_option_menu_set_active	(GtkWidget	*option_menu,
				 gint		index)
{
  gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), index);
}

static GtkAccelGroup *accel_group = NULL;

static void
change_pixmenuitem_label		(GtkWidget	*menuitem,
					 const gchar	*new_label)
{
  GtkWidget *widget = GTK_BIN(menuitem)->child;

  gtk_label_set_text(GTK_LABEL(widget), new_label);
}

void
z_change_menuitem			 (GtkWidget	*widget,
					  const gchar	*new_pixmap,
					  const gchar	*new_label,
					  const gchar	*new_tooltip)
{
  GtkWidget *image;

  if (new_label)
    change_pixmenuitem_label(widget, new_label);
  if (new_tooltip)
    z_tooltip_set(widget, new_tooltip);
  if (new_pixmap)
    {
      image = gtk_image_new_from_stock (new_pixmap, GTK_ICON_SIZE_MENU);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (widget),
				     image);
      /* not sure whether this is necessary, but won't harm */
      gtk_widget_show (image);
     }
}

static void
appbar_hide(GtkWidget *appbar)
{
  gtk_widget_hide(appbar);
  gtk_widget_queue_resize(main_window);
}

static void
add_hide(GtkWidget *appbar)
{
  GtkWidget *old =
    g_object_get_data(G_OBJECT(appbar), "hide_button");
  GtkWidget *widget;

  if (old)
    return;

  widget = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
  z_tooltip_set(widget, _("Hide the status bar"));

  if (widget)
    gtk_box_pack_end(GTK_BOX(appbar), widget, FALSE, FALSE, 0);

  gtk_widget_show(widget);
  g_signal_connect_swapped(G_OBJECT(widget), "clicked",
			   G_CALLBACK(appbar_hide),
			   appbar);

  g_object_set_data(G_OBJECT(appbar), "hide_button", widget);
}

static gint status_hide_timeout_id = -1;

static gint
status_hide_timeout	(void		*ignored)
{
  GtkWidget *appbar2 =
    lookup_widget(main_window, "appbar2");

  appbar_hide (appbar2);

  status_hide_timeout_id = -1;
  return FALSE; /* Do not call me again */
}

void z_status_print(const gchar *message, gint timeout)
{
  GtkWidget *appbar2 =
    lookup_widget(main_window, "appbar2");
  GtkWidget *status = gnome_appbar_get_status (GNOME_APPBAR (appbar2));

  add_hide(appbar2);

  gtk_label_set_text (GTK_LABEL (status), message);
  gtk_widget_show(appbar2);

  if (status_hide_timeout_id > -1)
    gtk_timeout_remove(status_hide_timeout_id);

  if (timeout > 0)
    status_hide_timeout_id =
      gtk_timeout_add(timeout, status_hide_timeout, NULL);
  else
    status_hide_timeout_id = -1;
}

void z_status_print_markup (const gchar *markup, gint timeout)
{
  GtkWidget *appbar2 =
    lookup_widget(main_window, "appbar2");
  GtkWidget *status = gnome_appbar_get_status (GNOME_APPBAR (appbar2));

  add_hide(appbar2);

  gtk_label_set_markup (GTK_LABEL (status), markup);
  gtk_widget_show(appbar2);

  if (status_hide_timeout_id > -1)
    gtk_timeout_remove(status_hide_timeout_id);

  if (timeout > 0)
    status_hide_timeout_id =
      gtk_timeout_add(timeout, status_hide_timeout, NULL);
  else
    status_hide_timeout_id = -1;
}

/* FIXME: [Hide] button */
void z_status_set_widget(GtkWidget * widget)
{
  GtkWidget *appbar2 =
    lookup_widget(main_window, "appbar2");
  GtkWidget *old =
    g_object_get_data(G_OBJECT(appbar2), "old_widget");

  if (old)
    gtk_container_remove(GTK_CONTAINER(appbar2), old);

  if (widget)
    gtk_box_pack_end(GTK_BOX(appbar2), widget, FALSE, FALSE, 0);

  add_hide(appbar2);

  g_object_set_data(G_OBJECT(appbar2), "old_widget", widget);

  gtk_widget_show(appbar2);
}

gboolean
z_build_path(const gchar *path, gchar **error_description)
{
  struct stat sb;
  gchar *b;
  gint i;

  if (!path || *path != '/')
    {
      /* FIXME */
      if (error_description)
	*error_description =
	  g_strdup(_("The path must start with /"));
      return FALSE;
    }
    
  for (i=1; path[i]; i++)
    if (path[i] == '/' || !path[i+1])
      {
	b = g_strndup(path, i+1);

	if (stat(b, &sb) < 0)
	  {
	    if (mkdir(b, S_IRUSR | S_IWUSR | S_IXUSR) < 0)
	      {
		if (error_description)
		  *error_description =
		    g_strdup_printf(_("Cannot create %s: %s"), b,
				    strerror(errno));
		g_free(b);
		return FALSE;
	      }
	    else
	      g_assert(stat(b, &sb) >= 0);
	  }

	if (!S_ISDIR(sb.st_mode))
	  {
	    if (error_description)
	      *error_description =
		g_strdup_printf(_("%s is not a directory"), b);
	    g_free(b);
	    return FALSE;
	  }

	g_free(b);
      }

  return TRUE;
}

static gchar *
strnmerge			(const gchar *		s1,
				 guint			len1,
				 const gchar *		s2,
				 guint			len2)
{
  gchar *d;

  d = g_malloc (len1 + len2 + 1);

  memcpy (d, s1, len1);
  memcpy (d + len1, s2, len2);

  d[len1 + len2] = 0;

  return d;
}

gchar *
z_replace_filename_extension	(const gchar *		filename,
				 const gchar *		new_ext)
{
  const gchar *ext;
  gint len;

  if (!filename)
    return NULL;

  len = strlen (filename);
  /* Last '.' in basename. UTF-8 safe because we scan for ASCII only. */
  for (ext = filename + len - 1;
       ext > filename && *ext != '.' && *ext != '/'; ext--);

  if (len == 0 || *ext != '.')
    {
      if (!new_ext)
	return g_strdup (filename);
      else
	return g_strconcat (filename, ".", new_ext, NULL);
    }

  len = ext - filename;

  if (new_ext)
    return strnmerge (filename, len + 1, new_ext, strlen (new_ext));
  else
    return g_strndup (filename, len);
}

static void
append_text			(GtkEditable *		e,
				 const gchar *		text)
{
  const gint end_pos = -1;
  gint old_pos, new_pos;

  gtk_editable_set_position (e, end_pos);
  old_pos = gtk_editable_get_position (e);
  new_pos = old_pos;

  gtk_editable_insert_text (e, text, /* bytes */ strlen (text), &new_pos);

  /* Move cursor before appended text */
  gtk_editable_set_position (e, old_pos);
}

/* See ttx export or screenshot for a demo */
void
z_on_electric_filename		(GtkWidget *		w,
				 gpointer		user_data)
{
  const gchar *name;	/* editable: "/foo/bar.baz" */
  const gchar *ext;	/* editable: "baz" */
  gchar *basename;	/* proposed: "far.faz" */
  gchar *baseext;	/* proposed: "faz" */
  gchar **bpp;		/* copy entered name here */
  gint len;
  gint baselen;
  gint baseextlen;

  name = gtk_entry_get_text (GTK_ENTRY (w));

  len = strlen (name);
  /* Last '/' in name. */
  for (ext = name + len - 1; ext > name && *ext != '/'; ext--);
  /* First '.' in last part of name. */
  for (; *ext && *ext != '.'; ext++);

  basename = (gchar *) g_object_get_data (G_OBJECT (w), "basename");
  g_assert (basename != NULL);

  baselen = strlen (basename);
  /* Last '.' in basename. UTF-8 safe because we scan for ASCII only. */
  for (baseext = basename + baselen - 1;
       baseext > basename && *baseext != '.'; baseext--);
  baseextlen = (*baseext == '.') ? baselen - (baseext - basename) : 0;

  bpp = (gchar **) user_data;

  /* This function is usually a callback handler for the "changed"
     signal in a GtkEditable. Since we will change the editable too,
     block the signal emission while we are editing */
  g_signal_handlers_block_by_func (G_OBJECT (w), 
				   z_on_electric_filename,
				   user_data);

  /* Tack basename on if no name or ends with '/' */
  if (len == 0 || name[len - 1] == '/')
    {
      append_text (GTK_EDITABLE (w), basename);
    }
  /* Cut off basename if not prepended by '/' */
  else if (len > baselen
	   && 0 == strcmp (&name[len - baselen], basename)
	   && name[len - baselen - 1] != '/')
    {
      const gint end_pos = -1;
      gchar *buf = g_strndup (name, len - baselen);

      gtk_entry_set_text (GTK_ENTRY (w), buf);

      /* Attach baseext if none already */
      if (baseextlen > 0 && ext < (name + len - baselen))
	append_text (GTK_EDITABLE (w), baseext);
      else
	gtk_editable_set_position (GTK_EDITABLE (w), end_pos);

      g_free (buf);
    }
#if 0
  /* Tack baseext on if name ends with '.' */
  else if (baseextlen > 0 && name[len - 1] == '.')
    {
      append_text (GTK_EDITABLE (w), baseext + 1);
    }
#endif
  /* Cut off baseext when duplicate */
  else if (baseextlen > 0 && len > baseextlen
	   && 0 == strcmp (&name[len - baseextlen], baseext)
	   && ext < (name + len - baseextlen))
    {
      gchar *buf = g_strndup (name, len - baseextlen);

      gtk_entry_set_text (GTK_ENTRY (w), buf);

      g_free (buf);
    }

  if (bpp)
    {
      g_free (*bpp);

      *bpp = g_strdup (gtk_entry_get_text (GTK_ENTRY (w)));
    }

  g_signal_handlers_unblock_by_func (G_OBJECT (w), 
				     z_on_electric_filename,
				     user_data);
}

void
z_electric_replace_extension	(GtkWidget *		w,
				 const gchar *		ext)
{
  const gchar *old_name;
  gchar *old_base;
  gchar *new_name;

  old_base = (gchar *) g_object_get_data (G_OBJECT (w), "basename");
  new_name = z_replace_filename_extension (old_base, ext);
  g_object_set_data (G_OBJECT (w), "basename", (gpointer) new_name);

  g_free (old_base);

  old_name = gtk_entry_get_text (GTK_ENTRY (w));
  new_name = z_replace_filename_extension (old_name, ext);
  
  g_signal_handlers_block_matched (G_OBJECT (w), G_SIGNAL_MATCH_FUNC,
				   0, 0, 0, z_on_electric_filename, 0);

  gtk_entry_set_text (GTK_ENTRY (w), new_name);

  g_signal_handlers_unblock_matched (G_OBJECT (w), G_SIGNAL_MATCH_FUNC,
				     0, 0, 0, z_on_electric_filename, 0);
  g_free (new_name);
}

static void
set_orientation_recursive	(GtkToolbar	*toolbar,
				 GtkOrientation orientation)
{
  GList *p = toolbar->children;
  GtkToolbarChild *child;

  while (p)
    {
      child = (GtkToolbarChild*)p->data;
      
      if (child->type == GTK_TOOLBAR_CHILD_WIDGET &&
	  GTK_IS_TOOLBAR(child->widget))
	set_orientation_recursive(GTK_TOOLBAR(child->widget), orientation);
      p = p->next;
    }

  gtk_toolbar_set_orientation(toolbar, orientation);
}

static void
on_orientation_changed		(GtkToolbar	*toolbar,
				 GtkOrientation	orientation,
				 gpointer	data)
{
  GList *p;
  GtkToolbarChild *child;

  if (!toolbar)
    return;

  p = toolbar->children;

  while (p)
    {
      child = (GtkToolbarChild*)p->data;

      if (child->type == GTK_TOOLBAR_CHILD_WIDGET &&
	  GTK_IS_TOOLBAR(child->widget))
	set_orientation_recursive(GTK_TOOLBAR(child->widget), orientation);
      p = p->next;
    }  
}

static void
set_style_recursive		(GtkToolbar	*toolbar,
				 GtkToolbarStyle style)
{
  GList *p = toolbar->children;
  GtkToolbarChild *child;

  while (p)
    {
      child = (GtkToolbarChild*)p->data;
      
      if (child->type == GTK_TOOLBAR_CHILD_WIDGET &&
	  GTK_IS_TOOLBAR(child->widget))
	set_style_recursive(GTK_TOOLBAR(child->widget), style);
      p = p->next;
    }

  gtk_toolbar_set_style(toolbar, style);
}

static void
on_style_changed		(GtkToolbar	*toolbar,
				 GtkToolbarStyle style,
				 gpointer	data)
{
  GList *p;
  GtkToolbarChild *child;

  if (!toolbar)
    return;

  p = toolbar->children;

  while (p)
    {
      child = (GtkToolbarChild*)p->data;

      if (child->type == GTK_TOOLBAR_CHILD_WIDGET &&
	  GTK_IS_TOOLBAR(child->widget))
	set_style_recursive(GTK_TOOLBAR(child->widget), style);
      p = p->next;
    }
}

void
propagate_toolbar_changes	(GtkWidget	*toolbar)
{
  g_return_if_fail (GTK_IS_TOOLBAR(toolbar));

  g_signal_connect(G_OBJECT(toolbar), "style-changed",
		   GTK_SIGNAL_FUNC(on_style_changed),
		   NULL);
  g_signal_connect(G_OBJECT(toolbar), "orientation-changed",
		   GTK_SIGNAL_FUNC(on_orientation_changed),
		   NULL);
}

void zmisc_overlay_subtitles	(gint page)
{
#ifdef HAVE_LIBZVBI
  GtkWidget *closed_caption1;

  zvbi_page = page;
  
  zconf_set_integer(zvbi_page,
		    "/zapping/internal/callbacks/zvbi_page");
  zconf_set_boolean(TRUE, "/zapping/internal/callbacks/closed_caption");

  if (main_info->current_mode == TVENG_NO_CAPTURE)
    zmisc_restore_previous_mode(main_info);

  osd_clear();
  
  closed_caption1 =
    lookup_widget(main_window, "closed_caption1");
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(closed_caption1),
				 TRUE);
#endif /* HAVE_LIBZVBI */
}


GtkWidget *
z_load_pixmap			(const gchar *		name)
{
  GtkWidget *image;
  gchar *path;

  path = g_strconcat (PACKAGE_PIXMAPS_DIR "/", name, NULL);
  image = gtk_image_new_from_file (path);
  g_free (path);

  gtk_widget_show (image);

  return image;
}

GtkWindow *
z_main_window		(void)
{
  return GTK_WINDOW(main_window);
}

gchar *
find_unused_name		(const gchar *		dir,
				 const gchar *		file,
				 const gchar *		ext)
{
  gchar *buf = NULL;
  gchar *name;
  const gchar *slash = "";
  const gchar *dot = "";
  gint index = 0;
  const gchar *s;
  gint n;
  struct stat sb;

  if (!dir)
    dir = "";
  else if (dir[0] && dir[strlen (dir) - 1] != '/')
    slash = "/";

  if (!file || !file[0])
    return g_strconcat (dir, slash, NULL);

  n = strlen (file);

  /* cut off existing extension from @file */
  for (s = file + n; s > file;)
    if (*--s == '.')
      {
	if (s == file || s[-1] == '/')
	  return g_strconcat (dir, slash, NULL);
	else
	  break;
      }

  if (s == file) /* has no extension */
    s = file + n;
  else if (!ext) /* no new ext, keep old */
    ext = s;

  /* parse off existing numeral suffix */
  for (n = 1; s > file && n < 10000000; n *= 10)
    if (isdigit(s[-1]))
      index += (*--s & 15) * n;
    else
      break;

  name = g_strndup (file, s - file);

  if (!ext)
    ext = "";
  else if (ext[0] && ext[0] != '.')
    dot = ".";

  if (index == 0 && n == 1) /* had no suffix */
    {
      /* Try first without numeral suffix */
      buf = g_strdup_printf ("%s%s%s%s%s",
			     dir, slash, name, dot, ext);
      index = 2; /* foo, foo2, foo3, ... */
    }
  /* else fooN, fooN+1, fooN+2 */

  for (n = 10000; n > 0; n--) /* eventually abort */
    {
      if (!buf)
	buf = g_strdup_printf("%s%s%s%d%s%s",
			      dir, slash, name, index++, dot, ext);

      /* Try to query file availability */
      /*
       * Note: This is easy to break, but since there's no good(tm)
       * way to predict an available file name, just do the simple thing.
       */
      if (stat (buf, &sb) == -1)
	{
	  switch (errno)
	    {
	    case ENOENT:
	    case ENOTDIR:
	      /* take this */
	      break;

	    default:
	      /* give up */
	      g_free (buf);
	      buf = NULL;
	      break;
	    }

	  break;
	}
      else
	{
	  /* exists, try other */
	  g_free (buf);
	  buf = NULL;
	}
    }

  g_free (name);

  return buf ? buf : g_strconcat (dir, slash, NULL);
}

/*
 *  "Spinslider"
 *
 *  [12345___|<>] unit [-<>--------][Reset]
 */

typedef struct _z_spinslider z_spinslider;

struct _z_spinslider
{
  GtkWidget *		hbox;
  GtkAdjustment *	spin_adj;
  GtkAdjustment *	hscale_adj;

  gfloat		history[3];
  guint			reset_state;
  gboolean		in_reset;
};

static inline z_spinslider *
get_spinslider			(GtkWidget *		hbox)
{
  z_spinslider *sp;

  sp = g_object_get_data (G_OBJECT (hbox), "z_spinslider");
  g_assert (sp != NULL);

  return sp;
}

GtkAdjustment *
z_spinslider_get_spin_adj	(GtkWidget *		hbox)
{
  return get_spinslider (hbox)->spin_adj;
}

GtkAdjustment *
z_spinslider_get_hscale_adj	(GtkWidget *		hbox)
{
  return get_spinslider (hbox)->hscale_adj;
}

gfloat
z_spinslider_get_value		(GtkWidget *		hbox)
{
  return get_spinslider (hbox)->spin_adj->value;
}

void
z_spinslider_set_value		(GtkWidget *		hbox,
				 gfloat			value)
{
  z_spinslider *sp = get_spinslider (hbox);

  gtk_adjustment_set_value (sp->spin_adj, value);
  gtk_adjustment_set_value (sp->hscale_adj, value);
}

void
z_spinslider_set_reset_value	(GtkWidget *		hbox,
				 gfloat			value)
{
  z_spinslider *sp = get_spinslider (hbox);

  sp->history[sp->reset_state] = value;

  gtk_adjustment_set_value (sp->spin_adj, value);
  gtk_adjustment_set_value (sp->hscale_adj, value);
}

void
z_spinslider_adjustment_changed	(GtkWidget *		hbox)
{
  z_spinslider *sp = get_spinslider (hbox);

  sp->hscale_adj->value = sp->spin_adj->value;
  sp->hscale_adj->lower = sp->spin_adj->lower;
  sp->hscale_adj->upper = sp->spin_adj->upper + sp->spin_adj->page_size;
  sp->hscale_adj->step_increment = sp->spin_adj->step_increment;
  sp->hscale_adj->page_increment = sp->spin_adj->page_increment;
  sp->hscale_adj->page_size = sp->spin_adj->page_size;
  gtk_adjustment_changed (sp->spin_adj);
  gtk_adjustment_changed (sp->hscale_adj);
}

static void
on_z_spinslider_hscale_changed	(GtkWidget *		widget,
				 z_spinslider *		sp)
{
  if (sp->spin_adj->value != sp->hscale_adj->value)
    gtk_adjustment_set_value (sp->spin_adj, sp->hscale_adj->value);
}

static void
on_z_spinslider_spinbutton_changed (GtkWidget *		widget,
				    z_spinslider *	sp)
{
  if (!sp->in_reset)
    {
      if (sp->reset_state != 0)
	{
	  sp->history[0] = sp->history[1];
	  sp->history[1] = sp->history[2];
	  sp->reset_state--;
	}
      sp->history[2] = sp->spin_adj->value;
    }

  if (sp->spin_adj->value != sp->hscale_adj->value)
    gtk_adjustment_set_value (sp->hscale_adj, sp->spin_adj->value);
}

static void
on_z_spinslider_reset		(GtkWidget *		widget,
				 z_spinslider *		sp)
{
  gfloat current_value;

  current_value = sp->history[2];
  sp->history[2] = sp->history[1];
  sp->history[1] = sp->history[0];
  sp->history[0] = current_value;

  sp->in_reset = TRUE;

  gtk_adjustment_set_value (sp->spin_adj, sp->history[2]);
  gtk_adjustment_set_value (sp->hscale_adj, sp->history[2]);

  sp->in_reset = FALSE;

  if (sp->reset_state == 0
      && fabs (sp->history[0] - sp->history[1]) < 1e-6)
    sp->reset_state = 2;
  else
    sp->reset_state = (sp->reset_state + 1) % 3;
}

#include "../pixmaps/reset.h"

GtkWidget *
z_spinslider_new		(GtkAdjustment *	spin_adj,
				 GtkAdjustment *	hscale_adj,
				 const gchar *		unit,
				 gfloat			reset,
				 gint			digits)
{
  z_spinslider *sp;

  g_assert (spin_adj != NULL);

  sp = g_malloc (sizeof (*sp));

  sp->spin_adj = spin_adj;
  sp->hscale_adj = hscale_adj;

  sp->hbox = gtk_hbox_new (FALSE, 0);
  g_object_set_data_full (G_OBJECT (sp->hbox), "z_spinslider", sp,
			  (GDestroyNotify) g_free);

  /*
    fprintf(stderr, "zss_new %f %f...%f  %f %f  %f  %d\n",
    spin_adj->value,
    spin_adj->lower, spin_adj->upper,
    spin_adj->step_increment, spin_adj->page_increment,
    spin_adj->page_size, digits);
  */

  /* Spin button */

  {
    GtkWidget *spinbutton;

    spinbutton = gtk_spin_button_new (sp->spin_adj,
				      sp->spin_adj->step_increment, digits);
    gtk_widget_show (spinbutton);
    /* I don't see how to set "as much as needed", so hacking this up */
    gtk_widget_set_size_request (spinbutton, 80, -1);
    gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (spinbutton),
				       GTK_UPDATE_IF_VALID);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbutton), TRUE);
    gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinbutton), TRUE);
    gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (spinbutton), TRUE);
    gtk_box_pack_start (GTK_BOX (sp->hbox), spinbutton, FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (sp->spin_adj), "value-changed",
		      G_CALLBACK (on_z_spinslider_spinbutton_changed), sp);
  }

  /* Unit name */

  if (unit)
    {
      GtkWidget *label;

      label = gtk_label_new (unit);
      gtk_widget_show (label);
      gtk_box_pack_start (GTK_BOX (sp->hbox), label, FALSE, FALSE, 3);
    }

  /* Slider */

  /* Necessary to reach spin_adj->upper with slider */
  if (!hscale_adj)
    sp->hscale_adj = GTK_ADJUSTMENT (gtk_adjustment_new
	(sp->spin_adj->value,
	 sp->spin_adj->lower,
	 sp->spin_adj->upper + sp->spin_adj->page_size,
	 sp->spin_adj->step_increment,
	 sp->spin_adj->page_increment,
	 sp->spin_adj->page_size));

  {
    GtkWidget *hscale;

    hscale = gtk_hscale_new (sp->hscale_adj);
    /* Another hack */
    gtk_widget_set_size_request (hscale, 80, -1);
    gtk_widget_show (hscale);
    gtk_scale_set_draw_value (GTK_SCALE (hscale), FALSE);
    gtk_scale_set_digits (GTK_SCALE (hscale), -digits);
    gtk_box_pack_start (GTK_BOX (sp->hbox), hscale, TRUE, TRUE, 3);
    g_signal_connect (G_OBJECT (sp->hscale_adj), "value-changed",
		      G_CALLBACK (on_z_spinslider_hscale_changed), sp);
  }

  /* Reset button */

  {
    static GdkPixbuf *pixbuf = NULL;
    GtkWidget *button;
    GtkWidget *image;

    sp->history[0] = reset;
    sp->history[1] = reset;
    sp->history[2] = reset;
    sp->reset_state = 0;
    sp->in_reset = FALSE;

    if (!pixbuf)
      pixbuf = gdk_pixbuf_from_pixdata (&reset_png, FALSE, NULL);

    if (pixbuf && (image = gtk_image_new_from_pixbuf (pixbuf)))
      {
	gtk_widget_show (image);
	button = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER (button), image);
	z_tooltip_set (button, _("Reset"));
      }
    else
      {
	button = gtk_button_new_with_label (_("Reset"));
      }

    gtk_widget_show (button);
    gtk_box_pack_start (GTK_BOX (sp->hbox), button, FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (button), "pressed",
		      G_CALLBACK (on_z_spinslider_reset), sp);
  }

  return sp->hbox;
}

/*
 *  "Device entry"
 *
 *  Device:	 Foo Inc. Mk II
 *  Driver:	 Foo 0.0.1
 *  Device file: [/dev/foo__________][v]
 */

typedef struct _z_device_entry z_device_entry;

struct _z_device_entry
{
  GtkWidget *		table;
  GtkWidget *		device;
  GtkWidget *		driver;
  GtkWidget *		combo;

  guint			timeout;

  tv_device_node *	list;
  tv_device_node *	selected;

  z_device_entry_open_fn *open_fn;
  z_device_entry_select_fn *select_fn;

  void *		user_data;
};

static void
z_device_entry_destroy		(gpointer		data)
{
  z_device_entry *de = data;

  if (de->timeout != ~0)
    gtk_timeout_remove (de->timeout);

  tv_device_node_delete_list (&de->list);

  g_free (de);
}

tv_device_node *
z_device_entry_selected		(GtkWidget *		table)
{
  z_device_entry *de = g_object_get_data (G_OBJECT (table), "z_device_entry");

  g_assert (de != NULL);
  return de->selected;
}

static void
z_device_entry_select		(z_device_entry *	de,
				 tv_device_node *	n)
{
  const gchar *device = "";
  const gchar *driver = "";
  gchar *s = NULL;

  de->selected = n;

  if (de->select_fn)
    de->select_fn (de->table, de->user_data, n);

  if (n)
    {
      if (n->label)
	device = n->label;

      if (n->driver && n->version)
	driver = s = g_strdup_printf ("%s %s", n->driver, n->version);
      else if (n->driver)
	driver = n->driver;
    }

  gtk_label_set_text (GTK_LABEL (de->device), device);
  gtk_label_set_text (GTK_LABEL (de->driver), driver);

  g_free (s);
}

static GtkWidget *
z_device_entry_label_new	(const char *		text,
				 guint			padding)
{
  GtkWidget *label;

  label = gtk_label_new (text);
  gtk_widget_show (label);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC (label), padding, padding);

  return label;
}

static void
on_z_device_entry_changed	(GtkEntry *		entry,
				 gpointer		user_data);

static void
z_device_entry_relist		(z_device_entry *	de)
{
  tv_device_node *n;

  if (de->combo)
    gtk_widget_destroy (de->combo);
  de->combo = gtk_combo_new ();
  gtk_widget_show (de->combo);

  for (n = de->list; n; n = n->next)
    {
      GtkWidget *item;
      GtkWidget *label;
      gchar *s;

      /* XXX deprecated, although the GtkCombo description elaborates:
	 "The drop-down list is a GtkList widget and can be accessed
	 using the list member of the GtkCombo. List elements can contain
	 arbitrary widgets, [by appending GtkListItems to the list]" */
      extern GtkWidget *gtk_list_item_new (void);

      item = gtk_list_item_new ();
      gtk_widget_show (item);

      /* XXX Perhaps add an icon indicating if the device is
         present but busy (v4l...), or the user has no access permission. */

      if (n->driver && n->label)
	s = g_strdup_printf ("%s (%s %s)", n->device, n->driver, n->label);
      else if (n->driver || n->label)
	s = g_strdup_printf ("%s (%s)", n->device,
			     n->driver ? n->driver : n->label);
      else
	s = g_strdup (n->device);

      label = z_device_entry_label_new (s, 0);
      g_free (s);

      gtk_container_add (GTK_CONTAINER (item), label);
      gtk_combo_set_item_string (GTK_COMBO (de->combo), GTK_ITEM (item), n->device);
      gtk_container_add (GTK_CONTAINER (GTK_COMBO (de->combo)->list), item);
    }

    gtk_table_attach (GTK_TABLE (de->table), de->combo, 1, 2, 2, 2 + 1,
  	    GTK_FILL | GTK_EXPAND, 0, 0, 0);

    g_signal_connect (G_OBJECT (GTK_COMBO (de->combo)->entry),
  	    "changed", G_CALLBACK (on_z_device_entry_changed), de);
}

static gboolean
on_z_device_entry_timeout	(gpointer		user_data)
{
  z_device_entry *de = user_data;
  tv_device_node *n;
  const gchar *s;

  s = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (de->combo)->entry));

  if (s && s[0])
    if ((n = de->open_fn (de->table, de->user_data, de->list, s)))
      {
	tv_device_node_add (&de->list, n);
	z_device_entry_relist (de);
	z_device_entry_select (de, n);
	return FALSE;
      }

  z_device_entry_select (de, NULL);

  return FALSE; /* don't call again */
}

static void
on_z_device_entry_changed	(GtkEntry *		entry,
				 gpointer		user_data)
{
  z_device_entry *de = user_data;
  tv_device_node *n;
  const gchar *s;

  if (de->timeout != ~0)
    {
      gtk_timeout_remove (de->timeout);
      de->timeout = ~0;
    }

  s = gtk_entry_get_text (entry);

  if (s && s[0])
    {
      for (n = de->list; n; n = n->next)
	if (0 == strcmp (s, n->device))
	  {
	    z_device_entry_select (de, n);
	    return;
	  }

      z_device_entry_select (de, NULL);
      
      de->timeout = gtk_timeout_add (1000 /* ms */,
				     on_z_device_entry_timeout, de);
    }
  else
    {
      z_device_entry_select (de, NULL);
    }
}

static void
z_device_entry_table_pair	(z_device_entry *	de,
				 gint			row,
				 const char *		label,
				 GtkWidget *		crank)
{
  gtk_table_attach (GTK_TABLE (de->table), z_device_entry_label_new (label, 3),
		    0, 1, row, row + 1, GTK_FILL, 0, 0, 0);
  if (crank)
    {
      gtk_widget_show (crank);
      gtk_table_attach (GTK_TABLE (de->table), crank, 1, 2, row, row + 1,
			GTK_FILL | GTK_EXPAND, 0, 0, 0);
    }
}

GtkWidget *
z_device_entry_new		(const gchar *		prompt,
				 tv_device_node *	list,
				 const gchar *		current_device,
				 z_device_entry_open_fn *open_fn,
				 z_device_entry_select_fn *select_fn,
				 void *			user_data)
{
  z_device_entry *de;

  g_assert (open_fn != NULL);

  de = g_malloc0 (sizeof (*de));

  de->open_fn = open_fn;
  de->select_fn = select_fn;
  de->user_data = user_data;

  de->timeout = ~0;

  de->table = gtk_table_new (3, 2, FALSE);
  gtk_widget_show (de->table);
  g_object_set_data_full (G_OBJECT (de->table), "z_device_entry", de,
			  (GDestroyNotify) z_device_entry_destroy);

  de->device = z_device_entry_label_new ("", 3);
  de->driver = z_device_entry_label_new ("", 3);

  z_device_entry_table_pair (de, 0, _("Device:"), de->device);
  z_device_entry_table_pair (de, 1, _("Driver:"), de->driver);
  z_device_entry_table_pair (de, 2, prompt ? prompt : _("Device file:"), NULL);

  de->list = list;

  z_device_entry_relist (de);

  gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (de->combo)->entry), "");

  if (current_device && current_device[0])
    gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (de->combo)->entry),
			current_device);
  else if (list)
    gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (de->combo)->entry),
			list->device);

  return de->table;
}

/**
 * z_widget_add_accelerator:
 * @widget: 
 * @accel_signal: 
 * @accel_key: 
 * @accel_mods: 
 * 
 * Like gtk_widget_add_accelerator() but takes care of creating the
 * accel group.
 **/
void
z_widget_add_accelerator	(GtkWidget	*widget,
				 const gchar	*accel_signal,
				 guint		accel_key,
				 guint		accel_mods)
{
  if (!accel_group)
    {
      accel_group = gtk_accel_group_new();
      gtk_window_add_accel_group(GTK_WINDOW(main_window),
				 accel_group);
    }

  gtk_widget_add_accelerator(widget, accel_signal, accel_group,
			     accel_key, accel_mods, GTK_ACCEL_VISIBLE);
}

static void
on_entry_activated (GObject *entry, GtkDialog *dialog)
{
  gtk_dialog_response (dialog, GPOINTER_TO_INT
		       (g_object_get_data (entry, "zmisc-response")));
}

void
z_entry_emits_response		(GtkWidget	*entry,
				 GtkDialog	*dialog,
				 GtkResponseType response)
{
  g_signal_connect (G_OBJECT (entry), "activate",
		    G_CALLBACK (on_entry_activated),
		    dialog);

  g_object_set_data (G_OBJECT (entry), "zmisc-response",
		     GINT_TO_POINTER (response));
}

/*
 *  Application stock icons
 */

GtkWidget *
z_gtk_image_new_from_pixdata	(const GdkPixdata *	pixdata)
{
  GdkPixbuf *pixbuf;
  GtkWidget *image;

  pixbuf = gdk_pixbuf_from_pixdata (pixdata, FALSE, NULL);
  g_assert (pixbuf != NULL);

  image = gtk_image_new_from_pixbuf (pixbuf);
  g_object_unref (G_OBJECT (pixbuf));

  return image;
}

static GtkIconFactory *
icon_factory			(void)
{
  static GtkIconFactory *factory = NULL;

  if (!factory)
    {
      factory = gtk_icon_factory_new ();
      gtk_icon_factory_add_default (factory);
      /* g_object_unref (factory); */
    }

  return factory;
}

gboolean
z_icon_factory_add_file		(const gchar *		stock_id,
				 const gchar *		filename)
{
  GtkIconSet *icon_set;
  GdkPixbuf *pixbuf;
  GError *err;
  gchar *path;
 
  path = g_strconcat (PACKAGE_PIXMAPS_DIR "/", filename, NULL);

  err = NULL;

  pixbuf = gdk_pixbuf_new_from_file (path, &err);

  g_free (path);

  if (!pixbuf)
    {
      if (err)
	{
#ifdef ZMISC_DEBUG_STOCK /* FIXME */
	  fprintf (stderr, "Cannot read image file '%s':\n%s\n",
		   err->message);
#endif
	  g_error_free (err);
	}

      return FALSE;
    }

  icon_set = gtk_icon_set_new_from_pixbuf (pixbuf);
  g_object_unref (G_OBJECT (pixbuf));

  gtk_icon_factory_add (icon_factory (), stock_id, icon_set);
  gtk_icon_set_unref (icon_set);

  return TRUE;
}

gboolean
z_icon_factory_add_pixdata	(const gchar *		stock_id,
				 const GdkPixdata *	pixdata)
{
  GtkIconSet *icon_set;
  GdkPixbuf *pixbuf;
  GError *err;

  err = NULL;

  pixbuf = gdk_pixbuf_from_pixdata (pixdata, /* copy_pixels */ FALSE, &err);

  if (!pixbuf)
    {
      if (err)
	{
#ifdef ZMISC_DEBUG_STOCK /* FIXME */
	  fprintf (stderr, "Cannot read pixdata:\n%s\n", err->message);
#endif
	  g_error_free (err);
	}

      return FALSE;
    }

  icon_set = gtk_icon_set_new_from_pixbuf (pixbuf);
  g_object_unref (G_OBJECT (pixbuf));

  gtk_icon_factory_add (icon_factory (), stock_id, icon_set);
  gtk_icon_set_unref (icon_set);

  return TRUE;
}

size_t
z_strlcpy			(char *			dst1,
				 const char *		src,
				 size_t			size)
{
	char c, *dst, *end;

	assert (size > 0);

	dst = dst1;
	end = dst1 + size - 1;

	while (dst < end && (c = *src++))
		*dst++ = c;

	*dst = 0;

	return dst - dst1;
}

/* Debugging. */
const gchar *
z_gdk_event_name		(GdkEvent *		event)
{
  static const gchar *event_name[] =
    {
      "NOTHING", "DELETE", "DESTROY", "EXPOSE", "MOTION_NOTIFY",
      "BUTTON_PRESS", "2BUTTON_PRESS", "3BUTTON_PRESS", "BUTTON_RELEASE",
      "KEY_PRESS", "KEY_RELEASE", "ENTER_NOTIFY", "LEAVE_NOTIFY",
      "FOCUS_CHANGE", "CONFIGURE", "MAP", "UNMAP", "PROPERTY_NOTIFY",
      "SELECTION_CLEAR", "SELECTION_REQUEST", "SELECTION_NOTIFY",
      "PROXIMITY_IN", "PROXIMITY_OUT", "DRAG_ENTER", "DRAG_LEAVE",
      "DRAG_MOTION", "DRAG_STATUS", "DROP_START", "DROP_FINISHED",
      "CLIENT_EVENT", "VISIBILITY_NOTIFY", "NO_EXPOSE"
    };
  
  if (event->type >= GDK_NOTHING
      && event->type <= GDK_NO_EXPOSE)
    return event_name[event->type - GDK_NOTHING];
  else
    return "unknown";
}
