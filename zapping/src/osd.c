/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2001 I�aki Garc�a Etxebarria
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
 * OSD routines.
 */

/* XXX gtk+ 2.3 GtkOptionMenu, Gnome font picker, color picker */
/* gdk_input_add/remove */
#undef GTK_DISABLE_DEPRECATED
#undef GNOME_DISABLE_DEPRECATED
#undef GDK_DISABLE_DEPRECATED

#include "site_def.h"

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gnome.h>

#define ZCONF_DOMAIN "/zapping/options/osd/"
#include "zconf.h"
#include "zmisc.h"
#include "osd.h"
#include "remote.h"
#include "common/math.h"
#include "properties.h"
#include "interface.h"
#include "globals.h"

#ifndef OSD_TEST
#define OSD_TEST 0
#endif

#define MAX_COLUMNS 48 /* TTX */
#define MAX_ROWS 26 /* 25 for TTX plus one for OSD */

typedef struct _piece {
  /* The drawing area */
  GtkWidget	*da;

  /* The rendered text */
  GdkPixbuf	*unscaled;
  GdkPixbuf	*scaled;

  /* geometry in the osd matrix, interesting for TTX and CC location */
  int		column, row, width, max_rows, max_columns;

  /* relative geometry in the OSD window (0...1) */
  float		x, y, w, h;

  /* sw is the width we scale p->unscaled to. Then the columns in
     "double_columns" are duplicated to build scaled */
  float		sw;

  /* Columns to duplicate, ignored if sw = w */
  int		*double_columns;
  int		num_double_columns;

  /* called before the piece geometry is set to allow changing of
     x, y, w, h */
  void		(*position)(struct _piece *p);
} piece;

typedef struct {
  piece		pieces[MAX_COLUMNS]; /* Set of pieces to show */
  int		n_pieces; /* Pieces in this row */
} row;

static row *matrix[MAX_ROWS];

static GtkWidget *osd_window = NULL;
/* Subrectangle in the osd_window we are drawing to */
static gint cx, cy, cw, ch;

/**
 * Handling of OSD.
 */
static void
paint_piece			(piece		*p,
				 gint x,	gint y,
				 gint w,	gint h)
{
  z_pixbuf_render_to_drawable(p->scaled, p->da->window,
			      p->da->style->white_gc, x, y, w, h);
}

static gboolean
expose				(GtkWidget	*da _unused_,
				 GdkEventExpose	*event,
				 piece		*p)
{
  paint_piece(p, event->area.x, event->area.y,
	      event->area.width, event->area.height);

  return TRUE;
}

/**
 * A temporary place to keep child windows without a parent, created
 * by the non-profit routine startup_osd.
 * cf. Futurama
 */
static GtkWidget *orphanarium = NULL;

static void
set_piece_geometry		(piece		*p)
{
  gint dest_x, dest_y, dest_w, dest_h, dest_sw;

  if (p->double_columns)
    g_free(p->double_columns);
  p->double_columns = NULL;
  p->num_double_columns = 0;

  if (p->position)
    {
      p->position(p);
      dest_x = (gint) p->x;
      dest_y = (gint) p->y;
      dest_w = (gint) p->w;
      dest_h = (gint) p->h;
      dest_sw = (gint) p->sw;
    }
  else
    {
      dest_x = (gint)(cx + p->x * cw);
      dest_y = (gint)(cy + p->y * ch);
      dest_w = (gint)(p->w * cw);
      dest_h = (gint)(p->h * ch);
      dest_sw = dest_w;
    }

  if (osd_window && ((!p->scaled)  ||
      (gdk_pixbuf_get_width(p->scaled) != dest_w)  ||
      (gdk_pixbuf_get_height(p->scaled) != dest_h)))
    {
      if (p->scaled)
	{
	  g_object_unref (G_OBJECT (p->scaled));
	  p->scaled = NULL;
	}
      if (dest_h > 0)
	{
	  if (dest_w == dest_sw)
	    p->scaled = z_pixbuf_scale_simple(p->unscaled,
					      dest_w, dest_h,
					      GDK_INTERP_BILINEAR);
	  else
	    {
	      GdkPixbuf * canvas = z_pixbuf_scale_simple(p->unscaled,
							 dest_sw, dest_h,
							 GDK_INTERP_BILINEAR);
	      /* Scaling with line duplication */
	      if (canvas)
		{
		  int i, last_column = 0, scaled_x=0;
		  p->scaled = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
					     dest_w, dest_h);

		  /* do the line duplication */
		  for (i=0; i<p->num_double_columns; i++)
		    {
		      int width;

		      width = p->double_columns[i] - last_column;
		      if (width)
			{
			  z_pixbuf_copy_area(canvas, last_column,
					     0, width, dest_h,
					     p->scaled,
					     scaled_x, 0);
			  scaled_x += width;
			}
		      z_pixbuf_copy_area(canvas, p->double_columns[i], 0,
					 1, dest_h,
					 p->scaled, scaled_x, 0);
		      scaled_x ++;
		      last_column = p->double_columns[i];
		    }
		  /* Copy the remaining */
		  if (p->num_double_columns)
		    {
		      int width = dest_sw - last_column;
		      if (width > 0)
			{
			  z_pixbuf_copy_area(canvas, last_column, 0,
					     width, dest_h,
					     p->scaled, scaled_x, 0);
			  scaled_x += width; /* for checking */
			}
		    }

		  /* We should always draw the whole p->scaled, no
		     more, no less. Otherwise sth is severely b0rken. */
		  g_assert(scaled_x == dest_w);
		  g_object_unref (G_OBJECT (canvas));
		}
	    }
	}
    }

  if (!osd_window)
    {
      if (gdk_window_get_parent(p->da->window) != orphanarium->window)
	gdk_window_reparent(p->da->window, orphanarium->window, 0, 0);
      return;
    }
  else if (osd_window->window != gdk_window_get_parent(p->da->window))
    {
      gdk_window_reparent(p->da->window, osd_window->window, dest_x,
			  dest_y);
      gdk_window_show(p->da->window);
    }

  gdk_window_move_resize(p->da->window,
			 dest_x, dest_y, dest_w, dest_h);

  if (p->scaled)
    paint_piece(p, 0, 0, dest_w, dest_h);
}

/**
 * Call this when:
 *	a) The osd_window parent is changed
 *	b) There's a resize of the osd_window
 */
static void
geometry_update			(void)
{
  int i, j;

  for (i = 0; i < MAX_ROWS; i++)
    if (matrix[i])
      for (j = 0; j < matrix[i]->n_pieces; j++)
	set_piece_geometry(&(matrix[i]->pieces[j]));
}





/**
 * Widget creation/destruction with caching (for reducing flicker)
 */
/* List of destroyed windows for reuse */
static GList *window_stack = NULL;



static GtkWidget *
get_window(void)
{
  GtkWidget *da;

  da = gtk_drawing_area_new();

  gtk_widget_add_events(da, GDK_EXPOSURE_MASK);

  gtk_fixed_put(GTK_FIXED(orphanarium), da, 0, 0);

  gtk_widget_realize(da);
  gdk_window_set_back_pixmap(da->window, NULL, FALSE);

  return da;
}

static void
unget_window(GtkWidget *window)
{
  if (gdk_window_get_parent(window->window) != orphanarium->window)
    gdk_window_reparent(window->window, orphanarium->window, 0, 0);

  gtk_widget_destroy(window);
}

static void
push_window(GtkWidget *window)
{
  window_stack = g_list_append(window_stack, window);
}

/* Unget all windows in the stack */
static void clear_stack (void) __attribute__ ((unused));
static void
clear_stack(void)
{
  GtkWidget *window;

  while (window_stack)
    {
      window = GTK_WIDGET(g_list_last(window_stack)->data);
      unget_window(window);
      window_stack = g_list_remove(window_stack, window);
    }
}

static GtkWidget *
pop_window(void)
{
  GtkWidget *window = NULL;

  if (window_stack)
    {
      window = GTK_WIDGET(g_list_last(window_stack)->data);
      window_stack = g_list_remove(window_stack, window);
    }

  if (!window)
    window = get_window();

  return window;
}

/**
 * Add/delete pieces.
 */
static void
add_piece			(GdkPixbuf	*unscaled,
				 GtkWidget	*da,
				 unsigned int col,
				 unsigned int row,
				 unsigned int width,
				 unsigned int max_rows,
				 unsigned int max_columns,
				 float x,	float y,
				 float w,	float h,
				 void		(*position)(piece *p))
{
  piece *p;

  g_assert(col < MAX_COLUMNS);
  g_assert(row < MAX_ROWS);

  p = matrix[row]->pieces + matrix[row]->n_pieces;
  matrix[row]->n_pieces++;

  CLEAR (*p);

  p->da = da;

  g_signal_connect(G_OBJECT(p->da), "expose-event",
		   G_CALLBACK(expose), p);

  p->x = x;
  p->y = y;
  p->w = w;
  p->h = h;

  p->row = row;
  p->column = col;
  p->width = width;
  p->max_rows = max_rows;
  p->max_columns = max_columns;
  p->position = position;

  p->unscaled = unscaled;

  set_piece_geometry(p);
}

static void
remove_piece			(guint		row,
				 int		p_index,
				 gboolean	just_push)
{
  piece *p = &(matrix[row]->pieces[p_index]);

  if (p->scaled)
    g_object_unref (G_OBJECT (p->scaled));
  g_object_unref (G_OBJECT (p->unscaled));

  g_signal_handlers_disconnect_matched (G_OBJECT(p->da),
					G_SIGNAL_MATCH_FUNC |
					G_SIGNAL_MATCH_DATA,
					0, 0, NULL,
					G_CALLBACK (expose), p);
  
  if (!just_push)
    unget_window(p->da);
  else
    push_window(p->da);

  if (p->double_columns)
    g_free(p->double_columns);

  if (p_index != (matrix[row]->n_pieces - 1))
    memcpy(p, p+1,
	   sizeof(piece)*((matrix[row]->n_pieces-p_index)-1));

  matrix[row]->n_pieces--;
}

static void
clear_row			(guint		row,
				 gboolean	just_push)
{
  while (matrix[row]->n_pieces)
    remove_piece(row, matrix[row]->n_pieces-1, just_push);
}

void
osd_clear			(void)
{
  guint i;

  for (i = 0; i < (MAX_ROWS - 1); ++i)
    if (NULL != matrix[i])
      clear_row (i, FALSE);

  zmodel_changed (osd_model);
}

/**
 * OSD sources.
 */

#include <libxml/parser.h>

static guint osd_clear_timeout_id = NO_SOURCE_ID;
static void (* osd_clear_timeout_cb)(gboolean);

#define OSD_ROW (MAX_ROWS - 1)

static gint
osd_clear_timeout	(void		*ignored _unused_)
{
  clear_row(OSD_ROW, FALSE);

  osd_clear_timeout_id = NO_SOURCE_ID;

  zmodel_changed(osd_model);

  if (osd_clear_timeout_cb)
    osd_clear_timeout_cb(TRUE);

  return FALSE;
}

/**
 * Given the rgb and the colormap creates a suitable color that should
 * be unref by unref_color
 */
static
GdkColor *create_color	  (float r, float g, float b,
			   GdkColormap *cmap)
{
  GdkColor *ret = g_malloc0(sizeof(GdkColor));

  ret->red = r*65535;
  ret->green = g*65535;
  ret->blue = b*65535;

  gdk_colormap_alloc_color(cmap, ret, FALSE, TRUE);

  return ret;
}

/**
 * Decreases the reference count of the given color, allocated with
 * create_color.
 */
static void
unref_color		(GdkColor *color, GdkColormap *cmap)
{
  gdk_colormap_free_colors(cmap, color, 1);
  g_free(color);
}

static gchar *
remove_markup			(const gchar *		s)
{
  GError *error = NULL;
  gchar *d;

  pango_parse_markup (s, -1, 0, NULL, &d, NULL, &error);

  if (!d || error)
    {
      g_assert (!d);

      if (error)
	{
	  if (OSD_TEST)
	    fprintf (stderr, "%s:%u: Bad markup in \"%s\": %s\n",
		     __FILE__, __LINE__, s, error->message);

	  g_error_free (error);
	  error = NULL;
	}

      return NULL;
    }

  return d;
}

static void
render_console			(const gchar *		s,
				 gboolean		markup)
{
  GError *error = NULL;
  gchar *plain_buf;
  gchar *locale_buf;
  gsize length;
  guint i;

  if (markup)
    {
      plain_buf = remove_markup (s);

      if (!plain_buf)
	return;
    }
  else
    {
      plain_buf = g_strdup (s);
    }

  for (i = 0; 0 != plain_buf[i]; ++i)
    if ('\n' == plain_buf[i] ||
	'\r' == plain_buf[i])
      plain_buf[i] = ' ';

  locale_buf = g_locale_from_utf8 (plain_buf, (gint) i, NULL, &length, &error);

  if (!locale_buf || error)
    {
      g_assert (!locale_buf);

      if (error)
	{
	  if (OSD_TEST)
	    fprintf (stderr, "%s:%u: Bad UTF-8 in \"%s\": %s\n",
		     __FILE__, __LINE__, plain_buf, error->message);

	  g_error_free (error);
	  error = NULL;
	}
    }
  else
    {
      if (length > 0)
	{
	  fputs (locale_buf, stdout);
	  
	  if (locale_buf[length - 1] != '\n')
	    fputc ('\n', stdout);
	}

      g_free (locale_buf);
    }

  g_free (plain_buf);
}

/* Render in OSD mode. Put markup to TRUE if the text contains pango
   markup to be interpreted. */
static void
osd_render_osd		(void (*timeout_cb)(gboolean),
			 const gchar *src, gboolean markup)
{
  GdkDrawable *canvas;
  GtkWidget *patch = pop_window ();
  PangoContext *context = gtk_widget_get_pango_context (patch);
  PangoLayout *layout = pango_layout_new (context);
  PangoFontDescription *pfd = NULL;
  GdkGC *gc = gdk_gc_new (patch->window);
  GdkColormap *cmap = gdk_drawable_get_colormap (patch->window);
  const gchar *fname = zcg_char (NULL, "font");
  PangoRectangle logical, ink;
  GdkColor *bg;
  gint w, h;
  gfloat rx, ry, rw, rh;
  gchar *buf = g_strdup_printf ("<span foreground=\"#%02x%02x%02x\">%s</span>",
				(int)(zcg_float(NULL, "fg_r")*255),
				(int)(zcg_float(NULL, "fg_g")*255),
				(int)(zcg_float(NULL, "fg_b")*255), src);

  /* First check that the selected font is valid */
  if (!fname || !*fname ||
      !(pfd = pango_font_description_from_string (fname)))
    {
      if (!fname || !*fname)
	ShowBox(_("Please choose a font for OSD in preferences."),
		GTK_MESSAGE_ERROR);
      else
	ShowBox(_("The configured font \"%s\" cannot be loaded, please"
		  " select another one in preferences\n"),
		GTK_MESSAGE_ERROR, fname);

      /* Some common fonts */
      pfd = pango_font_description_from_string ("Arial 36");
      if (!pfd)
	pfd = pango_font_description_from_string ("Sans 36");
      if (!pfd)
	{
	  ShowBox(_("No font could be loaded, please make sure\n"
		    "your system is properly configured."),
		  GTK_MESSAGE_ERROR);
	  unget_window (patch);
	  g_object_unref (G_OBJECT (layout));
	  g_object_unref (G_OBJECT (gc));
	  g_free (buf);
	  return;
	}
    }

  pango_layout_set_font_description (layout, pfd);

  if (markup)
    pango_layout_set_markup (layout, buf, -1);
  else
    pango_layout_set_text (layout, buf, -1);

  /* Get the text extents, compute the geometry and build the canvas */
  pango_layout_get_pixel_extents (layout, &ink, &logical);

  w = logical.width;
  h = logical.height;

  canvas = gdk_pixmap_new (patch->window, w, h, -1);

  /* Draw the canvas contents */
  bg = create_color(zcg_float(NULL, "bg_r"),
		    zcg_float(NULL, "bg_g"),
		    zcg_float(NULL, "bg_b"), cmap);

  gdk_gc_set_foreground(gc, bg);
  gdk_draw_rectangle(canvas, gc, TRUE, 0, 0, w, h);
  unref_color(bg, cmap);

  gdk_draw_layout (canvas, gc, 0, 0, layout);

  /* Compute the resulting patch geometry */
  rh = 0.1;
  rw = (rh*w)/h;

  if (rw >= 0.9)
    {
      rw = 0.9;
      rh = (rw*h)/w;
    }

  rx = 1 - rw;
  ry = 1 - rh;

  /* Create the patch */
  add_piece(gdk_pixbuf_get_from_drawable
	    (NULL, canvas, cmap, 0, 0, 0, 0, w, h),
	    patch, 0, OSD_ROW, 0, 0, 0, rx, ry, rw, rh, NULL);

  zmodel_changed(osd_model);

  /* Schedule the destruction of the patch */
  if (osd_clear_timeout_id > 0)
    {
      if (osd_clear_timeout_cb)
	osd_clear_timeout_cb(FALSE);

      g_source_remove (osd_clear_timeout_id);
    }

  osd_clear_timeout_id =
    g_timeout_add ((guint)(zcg_float (NULL, "timeout") * 1000),
		   (GSourceFunc) osd_clear_timeout, NULL);

  osd_clear_timeout_cb = timeout_cb;

  /* Cleanup */
  pango_font_description_free (pfd);
  g_object_unref (G_OBJECT (layout));
  g_object_unref (G_OBJECT (canvas));
  g_object_unref (G_OBJECT (gc));
  g_free (buf);
}

/* If given, timeout_cb(TRUE) is called when osd timed out,
   timeout_cb(FALSE) when error, replaced.
*/
void
osd_render_markup		(osd_timeout_fn *	timeout_cb,
				 osd_type		type,
				 const char *		buf)
{
  if (!buf || !buf[0])
    goto failed;

  if (OSD_TYPE_CONFIG == type)
    type = zcg_int (NULL, "osd_type");

  /* The different ways of drawing */
  switch (type)
    {
    case 0: /* OSD */
      clear_row (OSD_ROW, TRUE);
      osd_render_osd (timeout_cb, buf, TRUE);
      break;

    case 1: /* Statusbar */
      {
	gchar *plain_buf;

	/* Color looks funny in status bar. */
	if (!(plain_buf = remove_markup (buf)))
	  break;

	z_status_print (plain_buf, /* markup */ FALSE,
			(guint)(zcg_float (NULL, "timeout") * 1000),
			/* hide */ FALSE);

	g_free (plain_buf);

	break;
      }

    case 2: /* Console */
      render_console (buf, /* markup */ TRUE);
      break;

    case 3:
      break; /* Ignore */

    default:
      g_assert_not_reached ();
      break;
    }

  return;

 failed:
  if (timeout_cb)
    timeout_cb (FALSE);
}

/* If given, timeout_cb(TRUE) is called when osd timed out,
   timeout_cb(FALSE) when error, replaced.
*/
void
osd_render_markup_printf	(osd_timeout_fn *	timeout_cb,
				 const char *		template,
				 ...)
{
  gchar *buf;
  va_list args;

  buf = NULL;

  if (!template || !template[0])
    return;

  va_start (args, template);

  buf = g_strdup_vprintf (template, args);

  va_end (args);

  osd_render_markup (timeout_cb, OSD_TYPE_CONFIG, buf);

  g_free (buf);
}

void
osd_render			(osd_timeout_fn *	timeout_cb,
				 const char *		template,
				 ...)
{
  gchar *buf;
  va_list args;

  buf = NULL;

  if (!template || !template[0])
    goto failed;

  va_start (args, template);

  buf = g_strdup_vprintf (template, args);

  va_end (args);

  if (!buf || !buf[0])
    goto failed;

  /* The different ways of drawing */
  switch (zcg_int (NULL, "osd_type"))
    {
    case 0: /* OSD */ 
      clear_row (OSD_ROW, TRUE);
      osd_render_osd (timeout_cb, buf, FALSE);
      break;

    case 1: /* Statusbar */
      z_status_print (buf, /* markup */ FALSE,
		      (guint)(zcg_float (NULL, "timeout") * 1000),
		      /* hide */ FALSE);
      break;

    case 2: /* Console */
      render_console (buf, /* markup */ FALSE);
      break;

    case 3:
      break; /* Ignore */

    default:
      g_assert_not_reached ();
      break;
    }

  g_free (buf);

  return;

 failed:
  g_free (buf);

  if (timeout_cb)
    timeout_cb (FALSE);
}

/**
 * Interaction with the OSD window.
 */
gboolean coords_mode = FALSE;

static
void on_osd_screen_size_allocate	(GtkWidget	*widget _unused_,
					 GtkAllocation	*allocation,
					 gpointer	ignored _unused_)
{
  if (cw == allocation->width &&
      ch == allocation->height)
    return;

  cw = allocation->width;
  ch = allocation->height;

  geometry_update();
}

static void
set_window(GtkWidget *dest_window, gboolean _coords_mode)
{
  if (osd_window && !coords_mode)
    g_signal_handlers_disconnect_matched (G_OBJECT(osd_window),
					  G_SIGNAL_MATCH_FUNC |
					  G_SIGNAL_MATCH_DATA,
					  0, 0, NULL,
					  G_CALLBACK
					  (on_osd_screen_size_allocate),
					  NULL);

  osd_window = dest_window;

  coords_mode = _coords_mode;

  if (!coords_mode)
    g_signal_connect(G_OBJECT(dest_window), "size-allocate",
		     G_CALLBACK(on_osd_screen_size_allocate),
		     NULL);

  geometry_update();
}

void
osd_set_window(GtkWidget *dest_window)
{
  gtk_widget_realize(dest_window);

  cx = cy = 0;
  gdk_drawable_get_size(dest_window->window, &cw, &ch);
  
  set_window(dest_window, FALSE);
}

void
osd_set_coords(GtkWidget *dest_window,
	       gint x, gint y, gint w, gint h)
{
  cx = x;
  cy = y;
  cw = w;
  ch = h;

  set_window(dest_window, TRUE);
}

void
osd_unset_window(void)
{
  if (!osd_window)
    return;

  if (!coords_mode)
    g_signal_handlers_disconnect_matched (G_OBJECT(osd_window),
					  G_SIGNAL_MATCH_FUNC |
					  G_SIGNAL_MATCH_DATA,
					  0, 0, NULL,
					  G_CALLBACK (on_osd_screen_size_allocate),
					  NULL);

  osd_window = NULL;
  geometry_update(); /* Reparent to the orphanarium */
}

/* Python wrappers for the OSD renderer */
static PyObject* py_osd_render (PyObject *self _unused_, PyObject *args)
{
  char *string;
  int ok = ParseTuple (args, "s", &string);

  if (!ok)
    g_error ("zapping.osd_render(s)");

  osd_render (NULL, "%s", string);

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* py_osd_render_markup (PyObject *self _unused_, PyObject *args)
{
  char *string;
  int ok = ParseTuple (args, "s", &string);

  if (!ok)
    g_error ("zapping.osd_render_markup(s)");

  osd_render_markup (NULL, OSD_TYPE_CONFIG, string);

  Py_INCREF(Py_None);
  return Py_None;
}

static void
on_osd_type_changed	(GtkWidget	*widget,
			 GtkWidget	*page _unused_)
{
  GtkWidget *w;

  gboolean sensitive1 = FALSE;
  gboolean sensitive2 = FALSE;

  widget = lookup_widget(widget, "optionmenu22");

  switch (z_option_menu_get_active (widget))
    {
    case 0:
      sensitive1 = TRUE;
    case 1:
      sensitive2 = TRUE;
      break;
    default:
      break;
    }

  /* XXX Ugh. */

  w = lookup_widget (widget, "general-osd-font-label");
  gtk_widget_set_sensitive (w, sensitive1);
  w = lookup_widget (widget, "general-osd-font-selector");
  gtk_widget_set_sensitive (w, sensitive1);
  w = lookup_widget (widget, "general-osd-foreground-label");
  gtk_widget_set_sensitive (w, sensitive1);
  w = lookup_widget (widget, "general-osd-foreground-selector");
  gtk_widget_set_sensitive (w, sensitive1);
  w = lookup_widget (widget, "general-osd-background-label");
  gtk_widget_set_sensitive (w, sensitive1);
  w = lookup_widget (widget, "general-osd-background-selector");
  gtk_widget_set_sensitive (w, sensitive1);

  w = lookup_widget (widget, "general-osd-timeout-label");
  gtk_widget_set_sensitive (w, sensitive2);
  w = lookup_widget (widget, "general-osd-timeout-selector");
  gtk_widget_set_sensitive (w, sensitive2);
}

/* OSD properties */
static void
osd_setup		(GtkWidget	*page)
{
  GtkWidget *widget;

  /* OSD type */
  widget = lookup_widget(page, "optionmenu22");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
			      (guint) zcg_int(NULL, "osd_type"));
  on_osd_type_changed (page, page);

  g_signal_connect(G_OBJECT (widget), "changed",
		   G_CALLBACK(on_osd_type_changed),
		   page);

  /* OSD font */
  widget = lookup_widget(page, "general-osd-font-selector");
  if (zcg_char(NULL, "font"))
    gnome_font_picker_set_font_name(GNOME_FONT_PICKER(widget),
				    zcg_char(NULL, "font"));

  /* OSD foreground color */
  widget = lookup_widget(page, "general-osd-foreground-selector");
  gnome_color_picker_set_d(GNOME_COLOR_PICKER(widget),
		   zcg_float(NULL, "fg_r"),
		   zcg_float(NULL, "fg_g"),
		   zcg_float(NULL, "fg_b"),
		   0.0);

  /* OSD background color */
  widget = lookup_widget(page, "general-osd-background-selector");
  gnome_color_picker_set_d(GNOME_COLOR_PICKER(widget),
		   zcg_float(NULL, "bg_r"),
		   zcg_float(NULL, "bg_g"),
		   zcg_float(NULL, "bg_b"),
		   0.0);

  /* OSD timeout in seconds */
  widget = lookup_widget(page, "spinbutton2");
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget),
			    zcg_float(NULL, "timeout"));
}

static void
osd_apply		(GtkWidget	*page)
{
  GtkWidget *widget;
  gdouble r, g, b, a;

  widget = lookup_widget(page, "optionmenu22"); /* osd type */
  zcs_int(z_option_menu_get_active(widget), "osd_type");

  widget = lookup_widget(page, "general-osd-font-selector");
  zcs_char(gnome_font_picker_get_font_name(GNOME_FONT_PICKER(widget)),
	   "font");

  widget = lookup_widget(page, "general-osd-foreground-selector");
  gnome_color_picker_get_d(GNOME_COLOR_PICKER(widget), &r, &g, &b,
			   &a);
  zcs_float(r, "fg_r");
  zcs_float(g, "fg_g");
  zcs_float(b, "fg_b");

  widget = lookup_widget(page, "general-osd-background-selector");
  gnome_color_picker_get_d(GNOME_COLOR_PICKER(widget), &r, &g, &b,
			   &a);
  zcs_float(r, "bg_r");
  zcs_float(g, "bg_g");
  zcs_float(b, "bg_b");

  widget = lookup_widget(page, "spinbutton2"); /* osd timeout */
  zcs_float(gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget)), "timeout");

}

static void
add				(GtkDialog *		dialog)
{
  SidebarEntry general_options[] = {
    { N_("OSD"), "gnome-oscilloscope.png",
      "general-osd-table", osd_setup, osd_apply,
      .help_link_id = "zapping-settings-osd" }
  };
  SidebarGroup groups[] = {
    { N_("General Options"), general_options, G_N_ELEMENTS (general_options) }
  };


  standard_properties_add (dialog, groups, G_N_ELEMENTS (groups),
			   "zapping.glade2");
}

/**
 * Shutdown/startup of the OSD engine
 */
ZModel			*osd_model = NULL;

void
startup_osd(void)
{
  int i;
  GtkWidget *toplevel;
  property_handler osd_handler = {
    add: add
  };

  for (i = 0; i<MAX_ROWS; i++)
    matrix[i] = g_malloc0(sizeof(row));

  osd_model = ZMODEL(zmodel_new());

  zcc_int(0, "Which kind of OSD should be used", "osd_type");
  zcc_char("times new roman Bold 36", "Default font", "font");

  zcc_float(1.0, "Default fg r component", "fg_r");
  zcc_float(1.0, "Default fg g component", "fg_g");
  zcc_float(1.0, "Default fg b component", "fg_b");

  zcc_float(0.0, "Default bg r component", "bg_r");
  zcc_float(0.0, "Default bg g component", "bg_g");
  zcc_float(0.0, "Default bg b component", "bg_b");

  zcc_float(1.5, "Seconds the OSD text stays on screen", "timeout");

  orphanarium = gtk_fixed_new();
  gtk_widget_set_size_request (orphanarium, 828, 271);
  toplevel = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_container_add(GTK_CONTAINER(toplevel), orphanarium);
  gtk_widget_show(orphanarium);
  gtk_widget_realize(orphanarium);
  /* toplevel will never be shown */

  /* Add Python interface to our routines */
  cmd_register ("osd_render_markup", py_osd_render_markup, METH_VARARGS);
  cmd_register ("osd_render", py_osd_render, METH_VARARGS);

  /* Register our properties handler */
  prepend_property_handler (&osd_handler);
}

void
shutdown_osd(void)
{
  int i;

  osd_clear();
  clear_row(OSD_ROW, FALSE);

  for (i = 0; i<MAX_ROWS; i++)
    g_free(matrix[i]);

  g_object_unref (G_OBJECT (osd_model));
  osd_model = NULL;
}

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
