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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gdk/gdkx.h>

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

extern tveng_device_info * main_info;
extern GtkWidget * main_window;
extern gboolean disable_preview; /* TRUE if preview won't work */
gboolean debug_msg=FALSE; /* Debugging messages on or off */

/*
  Prints a message box showing an error, with the location of the code
  that called the function. If run is TRUE, gnome_dialog_run will be
  called instead of just showing the dialog. Keep in mind that this
  will block the capture.
*/
GtkWidget * ShowBoxReal(const gchar * sourcefile,
			const gint line,
			const gchar * func,
			const gchar * message,
			const gchar * message_box_type,
			gboolean blocking, gboolean modal)
{
  GtkWidget * dialog;
  gchar * str;
  gchar buffer[256];

  buffer[255] = 0;

  g_snprintf(buffer, 255, " (%d)", line);

  str = g_strconcat(sourcefile, buffer, ": [", func, "]", NULL);

  if (!str)
    return NULL;

  dialog = gnome_message_box_new(message, message_box_type,
				 GNOME_STOCK_BUTTON_OK,
				 NULL);


  gtk_window_set_title(GTK_WINDOW (dialog), str);

  /*
    I know this isn't usual for a dialog box, but this way we can see
    all the title bar if we want to
  */
  gtk_window_set_policy(GTK_WINDOW (dialog), FALSE, TRUE, TRUE);

  g_free(str);

  if (blocking)
    {
      gnome_dialog_run_and_close(GNOME_DIALOG(dialog));
      return NULL;
    }

  gtk_window_set_modal(GTK_WINDOW (dialog), modal);
  gtk_widget_show(dialog);

  return dialog;
}

gchar*
Prompt (GtkWidget *main_window, const gchar *title,
	const gchar *prompt,  const gchar *default_text)
{
  GtkWidget * dialog;
  GtkVBox * vbox;
  GtkWidget *label, *entry;
  gchar *buffer = NULL;

  dialog = gnome_dialog_new(title,
			    GNOME_STOCK_BUTTON_OK,
			    GNOME_STOCK_BUTTON_CANCEL,
			    NULL);
  if (main_window)
    gnome_dialog_set_parent(GNOME_DIALOG (dialog), GTK_WINDOW(main_window));
  gnome_dialog_close_hides(GNOME_DIALOG (dialog), TRUE);
  gnome_dialog_set_default(GNOME_DIALOG (dialog), 0);

  gtk_window_set_title(GTK_WINDOW (dialog), title);
  gtk_window_set_modal(GTK_WINDOW (dialog), TRUE);
  vbox = GTK_VBOX(GNOME_DIALOG(dialog)->vbox);
  if (prompt)
    {
      label = gtk_label_new(prompt);
      gtk_box_pack_start_defaults(GTK_BOX(vbox), label);
      gtk_widget_show(label);
    }
  entry = gtk_entry_new();
  gtk_box_pack_start_defaults(GTK_BOX(vbox), entry);
  gtk_widget_show(entry);
  gnome_dialog_editable_enters(GNOME_DIALOG(dialog), GTK_EDITABLE (entry));
  gtk_widget_grab_focus(entry);
  if (default_text)
    {
      gtk_entry_set_text(GTK_ENTRY(entry), default_text);
      gtk_entry_select_region(GTK_ENTRY(entry), 0, -1);
    }

  if (!gnome_dialog_run_and_close(GNOME_DIALOG(dialog)))
    buffer = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));

  gtk_widget_destroy(dialog);
  
  return buffer;
}

/*
  Creates a GtkPixmapMenuEntry with the desired pixmap and the
  desired label.
*/
GtkWidget * z_gtk_pixmap_menu_item_new(const gchar * label,
				       const gchar * icon)
{
  GtkWidget * pixmap_menu_item;
  GtkWidget * accel_label;
  GtkWidget * pixmap;

  g_assert(label != NULL);

  pixmap_menu_item = gtk_pixmap_menu_item_new();
  accel_label = gtk_accel_label_new (label);
  gtk_misc_set_alignment(GTK_MISC(accel_label), 0.0, 0.5);
  gtk_container_add(GTK_CONTAINER (pixmap_menu_item), accel_label);
  gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (accel_label),
				    pixmap_menu_item);
  gtk_widget_show (accel_label);

  if (icon)
    {
      /* if i don't specify the size, the pixmap is too big, but the
	 one libgnomeui creates isn't... why? */
      pixmap = gnome_stock_pixmap_widget_at_size (pixmap_menu_item,
						  icon, 16, 16);
      
      gtk_pixmap_menu_item_set_pixmap(GTK_PIXMAP_MENU_ITEM(pixmap_menu_item),
				      pixmap);
      gtk_widget_show(pixmap);
    }

  return (pixmap_menu_item);
}

void set_tooltip	(GtkWidget	*widget,
			 const gchar	*new_tip)
{
  GtkTooltipsData *td = gtk_tooltips_data_get(widget);
  GtkTooltips *tips;

  if ((!td) || (!td->tooltips))
    tips = gtk_tooltips_new();
  else
    tips = td->tooltips;

  gtk_tooltips_set_tip(tips, widget, new_tip,
		       "private tip, or, er, just babbling, you know");
}

/*
  does the mode switching. Since this requires more than just using
  tveng, a new routine is needed.
  Returns whatever tveng returns, but we print the message ourselves
  too, so no need to aknowledge it to the user.
  Side efects: Stops whatever mode was being used before.
*/
int
zmisc_switch_mode(enum tveng_capture_mode new_mode,
		  tveng_device_info * info)
{
  GtkWidget * tv_screen;
  int return_value = 0;
  gint x, y, w, h;
  enum tveng_frame_pixformat format;
  gboolean muted;
  gchar * old_name = NULL;
  enum tveng_capture_mode mode;

  g_assert(info != NULL);
  g_assert(main_window != NULL);
  tv_screen = lookup_widget(main_window, "tv_screen");
  g_assert(tv_screen != NULL);

  if ((info->current_mode == new_mode) &&
      (new_mode != TVENG_NO_CAPTURE))
    return 0; /* success */

  /* save this input name for later retrieval */
  if (info->num_inputs > 0)
    old_name = g_strdup(info->inputs[info->cur_input].name);

  gdk_window_get_size(tv_screen->window, &w, &h);
  gdk_window_get_origin(tv_screen->window, &x, &y);

  muted = tveng_get_mute(info);
  mode = info->current_mode;
  tveng_stop_everything(info);

  /* Stop current capture mode */
  switch (mode)
    {
    case TVENG_CAPTURE_PREVIEW:
      fullscreen_stop(info);
      break;
    case TVENG_CAPTURE_READ:
      capture_stop(info);
      break;
    case TVENG_CAPTURE_WINDOW:
      overlay_stop(info);
      break;
    default:
      break;
    }
  if (new_mode != TVENG_NO_CAPTURE)
    {
      gtk_widget_hide(lookup_widget(main_window, "appbar2"));
      ttxview_detach(main_window);
      tveng_close_device(info);
    }

  if (new_mode != TVENG_CAPTURE_PREVIEW)
    osd_set_window(tv_screen, main_window);

  switch (new_mode)
    {
    case TVENG_CAPTURE_READ:
      if (tveng_attach_device(zcg_char(NULL, "video_device"),
			      TVENG_ATTACH_READ, info) == -1)
	{
	  /* Try opening as XVideo as a last resort */
	  if (tveng_attach_device(zcg_char(NULL, "video_device"),
				  TVENG_ATTACH_XV, info) == -1)
	    {
	      RunBox("%s couldn't be opened\n:%s,\naborting",
		     GNOME_MESSAGE_BOX_ERROR,
		     zcg_char(NULL, "video_device"), info->error);
	      exit(1);
	    }
	}
      tveng_set_capture_size(w, h, info);
      return_value = capture_start(tv_screen, info);
      break;
    case TVENG_CAPTURE_WINDOW:
      if (disable_preview) {
	g_warning("preview has been disabled");
	tveng_attach_device(zcg_char(NULL, "video_device"),
			    TVENG_ATTACH_XV, info);
	g_free(old_name);
	return -1;
      }

      if (tveng_attach_device(zcg_char(NULL, "video_device"),
			      TVENG_ATTACH_XV, info)==-1)
	{
	  RunBox("%s couldn't be opened\n:%s, aborting",
		 GNOME_MESSAGE_BOX_ERROR,
		 zcg_char(NULL, "video_device"), info->error);
	  exit(1);
	}

      format = zmisc_resolve_pixformat(tveng_get_display_depth(info),
				       x11_get_byte_order());

      if ((format != -1) &&
	  (info->current_controller != TVENG_CONTROLLER_XV))
	{
	  info->format.pixformat = format;
	  if ((tveng_set_capture_format(info) == -1) ||
	      (info->format.pixformat != format))
	    g_warning("Preview format invalid: %s (%d, %d)", info->error,
		      info->format.pixformat, format);
	  printv("prev: setting %d, got %d\n", format,
		 info->format.pixformat);
	}

      info->window.x = x;
      info->window.y = y;
      info->window.width = w;
      info->window.height = h;
      info->window.clipcount = 0;
      info->window.win = GDK_WINDOW_XWINDOW(tv_screen->window);
      info->window.gc = GDK_GC_XGC(tv_screen->style->white_gc);
      tveng_set_preview_window(info);
      return_value = tveng_start_window(info);
      if (return_value != -1)
	{
	  startup_overlay(tv_screen, main_window, info);
	  overlay_sync(TRUE);
	}
      else
	g_warning(info->error);
      break;
    case TVENG_CAPTURE_PREVIEW:
      if (tveng_attach_device(zcg_char(NULL, "video_device"),
			      TVENG_ATTACH_XV, info)==-1)
      {
	RunBox("%s couldn't be opened\n:%s,\naborting",
	       GNOME_MESSAGE_BOX_ERROR,
	       zcg_char(NULL, "video_device"), info->error);
	exit(1);
      }

      if (disable_preview) {
	g_warning("preview has been disabled");
	g_free (old_name);
	return -1;
      }

      format = zmisc_resolve_pixformat(tveng_get_display_depth(info),
				       x11_get_byte_order());

      if ((format != -1) &&
	  (info->current_controller != TVENG_CONTROLLER_XV))
	{
	  info->format.pixformat = format;
	  if ((tveng_set_capture_format(info) == -1) ||
	      (info->format.pixformat != format))
	    g_warning("Fullscreen format invalid: %s (%d, %d)", info->error,
		      info->format.pixformat, format);
	  printv("fulls: setting %d, got %d\n", format,
		 info->format.pixformat);
	}

      return_value = fullscreen_start(info);
      if (return_value == -1)
	g_warning("couldn't start fullscreen mode");
      break;
    default:
#ifndef HAVE_GDKPIXBUF
      ShowBox(_("The teletext decoder needs GdkPixbuf, and\n"
		"configure didn't find it."), GNOME_MESSAGE_BOX_INFO);
      break;
#endif /* HAVE_GDKPIXBUF */

      if (!zvbi_get_object())
	{
	  ShowBox(_("VBI has been disabled, or it doesn't work."),
		  GNOME_MESSAGE_BOX_INFO);
	  break;
	}

      /* start vbi code */
      gtk_widget_show(lookup_widget(main_window, "appbar2"));
      ttxview_attach(main_window, lookup_widget(main_window, "tv_screen"),
		     lookup_widget(main_window, "toolbar1"),
		     lookup_widget(main_window, "appbar2"));

      break; /* TVENG_NO_CAPTURE */
    }

  /* Restore old input if we found it earlier */
  if (old_name != NULL)
    if (-1 == tveng_set_input_by_name(old_name, info))
      g_warning("couldn't restore old input");

  g_free (old_name);

  /* Update the controls window if it's open */
  update_control_box(info);
  /* Update the standards, channels, etc */
  update_standards_menu(tv_screen, info);
  /* Updating the properties is not so useful, and it isn't so easy,
     since there might be multiple properties dialog open */
  tveng_set_mute(muted, info);

  /* Find optimum size for widgets */
  gtk_widget_queue_resize(main_window);

  return return_value;
}

#if 0
/* API flaw */
static
void set_stock_pixmap	(GtkWidget	*button,
			 const gchar	*new_pix)
{
  GtkWidget *widget = GTK_BIN(button)->child;
  GList *node = g_list_first(GTK_BOX(widget)->children)->next;

  widget = GTK_WIDGET(((GtkBoxChild*)(node->data))->widget);

  gnome_stock_set_icon(GNOME_STOCK(widget), new_pix);
}
#endif

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
  gint w = gdk_pixbuf_get_width(pixbuf), h=gdk_pixbuf_get_height(pixbuf);

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
