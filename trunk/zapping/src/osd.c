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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <pthread.h>
#define ZCONF_DOMAIN "/zapping/options/osd/"
#include "zconf.h"
#include "osd.h"
#include "zmisc.h"
#include "x11stuff.h"
#include "common/fifo.h"
#include "zvbi.h"

#include "libvbi/export.h"

#define CELL_WIDTH 16
#define CELL_HEIGHT 26

#define MAX_COLUMNS 48
#define MAX_ROWS 26 /* 25 for TTX plus one for OSD */

#include "libvbi/libvbi.h"
extern struct vbi *zvbi_get_object(void);

ZModel *osd_model = NULL;

struct osd_piece
{
  GtkWidget	*window; /* Attached to this piece */
  GdkPixbuf	*unscaled; /* unscaled version of the rendered text */
  GdkPixbuf	*scaled; /* scaled version of the text */
  int		start; /* starting col in the row */
  int		width; /* number of characters in this piece */
};

struct osd_row {
  struct osd_piece	pieces[MAX_COLUMNS]; /* Set of pieces to show */
  int			n_pieces; /* Number of pieces in this row */
};
static struct osd_row * osd_matrix[MAX_ROWS];
static struct fmt_page osd_page;

/* For window mode */
static GtkWidget *osd_window = NULL;
static GtkWidget *osd_parent_window = NULL;
/* For coords mode */
static gint cx, cy, cw=-1, ch=-1;

static gboolean osd_started = FALSE; /* shared between threads */
static gboolean osd_status = FALSE;

static gint input_id = 0;

extern int osd_pipe[2];

static void osd_event		(gpointer	   data,
                                 gint              source, 
				 GdkInputCondition condition);

void
startup_osd(void)
{
  int i;

  if (osd_started)
    return;

  for (i = 0; i<MAX_ROWS; i++)
    {
      osd_matrix[i] = g_malloc(sizeof(struct osd_row));
      memset(osd_matrix[i], 0, sizeof(struct osd_row));
    }

  osd_started = TRUE;

  input_id = gdk_input_add(osd_pipe[0], GDK_INPUT_READ,
			   osd_event, NULL);

  osd_model = ZMODEL(zmodel_new());

  memset(&osd_page, 0, sizeof(struct fmt_page));

  zcc_int(0, "Which kind of OSD should be used", "osd_type");
  zcc_char("-adobe-times-bold-r-normal-*-14-*-*-*-p-*-iso8859-1",
	   "Default font", "font");

  zcc_float(1.0, "Default fg r component", "fg_r");
  zcc_float(1.0, "Default fg g component", "fg_g");
  zcc_float(1.0, "Default fg b component", "fg_b");

  zcc_float(0.0, "Default bg r component", "bg_r");
  zcc_float(0.0, "Default bg g component", "bg_g");
  zcc_float(0.0, "Default bg b component", "bg_b");
}

void
shutdown_osd(void)
{
  int i;

  g_assert(osd_started == TRUE);

  osd_clear();

  for (i = 0; i<MAX_ROWS; i++)
    g_free(osd_matrix[i]);

  gdk_input_remove(input_id);

  osd_started = FALSE;

  gtk_object_destroy(GTK_OBJECT(osd_model));
  osd_model = NULL;
}

static void
paint_piece (GtkWidget *widget, struct osd_piece *piece,
	     gint x, gint y, gint width, gint height)
{
  gint w, h;

  g_assert(osd_started == TRUE);
  g_assert(widget != NULL);

  gdk_window_get_size(widget->window, &w, &h);

  if (width == -1)
    {
      width = w;
      height = h;
    }

  if (piece->scaled)
    z_pixbuf_render_to_drawable(piece->scaled, widget->window,
				widget->style->white_gc,
				x, y, width, height);
}

static void
set_piece_geometry(int row, int piece)
{
  gint x, y, w, h, rows, columns, start;
  struct osd_piece * p;
  gint dest_w, dest_h;

  g_assert(osd_started == TRUE);
  g_assert(row >= 0);
  g_assert(row < MAX_ROWS);
  g_assert(piece >= 0);
  g_assert(piece < osd_matrix[row]->n_pieces);

  if (!osd_window && (cw < 0 || ch < 0))
    return; /* nop */

  if (osd_window)
    {
      gdk_window_get_size(osd_window->window, &w, &h);
      gdk_window_get_origin(osd_window->window, &x, &y);
    }
  else
    {
      x = cx; y = cy; w = cw; h = ch;
    }

  if (osd_page.columns >= 40) /* naive cc test */
    {
      /* Text area 40x25 is (64, 38) - (703, 537) in a 768x576 screen */
      x += (64*w)/768;
      y += (38*h)/576;
      w -= (128*w)/768;
      h -= (76*h)/576;
    }
  else
    {
      /* Text area 34x15 is (48, 45) - (591, 434) in a 640x480 screen */
      x += (48*w)/640;
      y += (45*h)/480;
      w -= (96*w)/640;
      h -= (90*h)/480;
    }

  if (!osd_page.rows)
    rows = 15;
  else
    rows = osd_page.rows;

  if (!osd_page.columns)
    columns = 34;
  else
    columns = osd_page.columns;

  p = &(osd_matrix[row]->pieces[piece]);

  if (row == MAX_ROWS - 1) /* OSD text */
    row = rows;

  if (p->start < 0)
    start = (columns-p->width)/2;
  else
    start = p->start;

  gtk_widget_realize(p->window);

  dest_w = ((start+p->width)*w)/columns
           - (start*w)/columns;
  dest_h = ((row+1)*h)/rows-(row*h)/rows;

  gdk_window_move_resize(p->window->window,
			 x+(start*w)/columns,
			 y+(row*h)/rows,
			 dest_w, dest_h);

  if ((!p->scaled)  ||
      (gdk_pixbuf_get_width(p->scaled) != dest_w)  ||
      (gdk_pixbuf_get_height(p->scaled) != dest_h))
    {
      if (p->scaled)
	gdk_pixbuf_unref(p->scaled);
      if (dest_h > 0)
	p->scaled = z_pixbuf_scale_simple(p->unscaled, dest_w,
					  dest_h,
					  GDK_INTERP_BILINEAR);
      else
	p->scaled = NULL;
    }

  if (p->scaled)
    paint_piece(GTK_BIN(p->window)->child, p, 0, 0, dest_w, dest_h);
}

static void
osd_geometry_update(gboolean raise_if_visible)
{
  int i, j;
  gboolean visible;

  g_assert(osd_started == TRUE);

  if (osd_window)
    visible = x11_window_viewable(osd_window->window);
  else
    visible = FALSE;

  if (cw > 0 && ch > 0)
    visible = TRUE;

  for (i=0; i<MAX_ROWS; i++)
    for (j=0; j<osd_matrix[i]->n_pieces; j++)
      {
	set_piece_geometry(i, j);
	if (visible)
	  {
	    gtk_widget_show(osd_matrix[i]->pieces[j].window);
	    if (raise_if_visible)
	      gdk_window_raise(osd_matrix[i]->pieces[j].window->window);
	  }
	else
	  gtk_widget_hide(osd_matrix[i]->pieces[j].window);
      }
}

static
gboolean on_osd_screen_configure	(GtkWidget	*widget,
					 GdkEventConfigure *event,
					 gpointer	user_data)
{
  osd_geometry_update(FALSE);

  return TRUE;
}

static
void on_osd_screen_size_allocate	(GtkWidget	*widget,
					 GtkAllocation	*allocation,
					 gpointer	ignored)
{
  osd_geometry_update(FALSE);
}

/* Handle the map/unmap logic */
static
gboolean on_osd_event			(GtkWidget	*widget,
					 GdkEvent	*event,
					 gpointer	ignored)
{
  switch (event->type)
    {
    case GDK_UNMAP:
    case GDK_MAP:
      osd_geometry_update(FALSE);
      break;
    default:
      break;
    }

  return FALSE;
}

void
osd_unset_window(void)
{
  if (!osd_window && !osd_parent_window)
    return;

  if (osd_window)
    gtk_signal_disconnect_by_func(GTK_OBJECT(osd_window),
				  GTK_SIGNAL_FUNC(on_osd_screen_size_allocate),
				  NULL);

  if (osd_parent_window) {
    gtk_signal_disconnect_by_func(GTK_OBJECT(osd_parent_window),
				  GTK_SIGNAL_FUNC(on_osd_screen_configure),
				  NULL);
    gtk_signal_disconnect_by_func(GTK_OBJECT(osd_parent_window),
				  GTK_SIGNAL_FUNC(on_osd_event),
				  NULL);
  }

  osd_window = osd_parent_window = NULL;
  osd_geometry_update(FALSE);
}

void
osd_set_window(GtkWidget *dest_window, GtkWidget *parent)
{
  g_assert(osd_started == TRUE);

  if (osd_window || osd_parent_window)
    osd_unset_window();

  g_assert(osd_window == NULL);
  g_assert(osd_parent_window == NULL);

  ch = cw = -1;

  osd_window = dest_window;
  osd_parent_window = parent;

  gtk_signal_connect(GTK_OBJECT(dest_window), "size-allocate",
		     GTK_SIGNAL_FUNC(on_osd_screen_size_allocate),
		     NULL);
  gtk_signal_connect(GTK_OBJECT(parent), "configure-event",
		     GTK_SIGNAL_FUNC(on_osd_screen_configure),
		     NULL);
  gtk_signal_connect(GTK_OBJECT(parent), "event",
		     GTK_SIGNAL_FUNC(on_osd_event), NULL);

  osd_geometry_update(TRUE);
}

void
osd_set_coords(gint x, gint y, gint w, gint h)
{
  if (osd_window || osd_parent_window)
    osd_unset_window();

  g_assert(osd_window == NULL);
  g_assert(osd_parent_window == NULL);

  cx = x;
  cy = y;
  cw = w;
  ch = h;

  osd_geometry_update(TRUE);
}

void
osd_on(GtkWidget * dest_window, GtkWidget *parent)
{
  g_assert(osd_started == TRUE);

  if (osd_status)
    return;

  osd_set_window(dest_window, parent);

  osd_status = TRUE;
}

void
osd_off(void)
{
  g_assert(osd_started == TRUE);

  osd_status = FALSE;
  osd_clear();

  if (osd_window)
    osd_unset_window();
  cw = ch = -1;
}

static
gboolean on_osd_expose_event		(GtkWidget	*widget,
					 GdkEventExpose	*event,
					 struct osd_piece *piece)
{
  g_assert(osd_started == TRUE);

  g_assert(piece != NULL);
  g_assert(widget == GTK_BIN(piece->window)->child);

  paint_piece(widget, piece, event->area.x, event->area.y,
	      event->area.width, event->area.height);

  return TRUE;
}

/* List of destroyed windows for reuse */
static GList *window_pool = NULL;
static GList *window_stack = NULL;

static GtkWidget *
get_window(void)
{
  GtkWidget *window;
  GtkWidget *da;

  g_assert(osd_started == TRUE);

  if (window_pool)
    {
      window = GTK_WIDGET(g_list_last(window_pool)->data);
      window_pool = g_list_remove(window_pool, window);
      return window;
    }

  window = gtk_window_new(GTK_WINDOW_POPUP);
  da = gtk_drawing_area_new();

  gtk_widget_realize(window);
  while (!window->window)
    z_update_gui();
  gtk_container_add(GTK_CONTAINER(window), da);

  gdk_window_set_back_pixmap(da->window, NULL, FALSE);

  gdk_window_set_decorations(window->window, 0);

  if (osd_parent_window)
    {
      gdk_window_set_transient_for(window->window,
				   osd_parent_window->window);
      gdk_window_set_group(window->window, osd_parent_window->window);
    }

  gtk_widget_add_events(da, GDK_EXPOSURE_MASK);

  gtk_widget_show(da);

  return window;
}

static void
unget_window(GtkWidget *window)
{
  g_assert(osd_started == TRUE);

  gtk_widget_hide(window);

  window_pool = g_list_append(window_pool, window);
}

static void
push_window(GtkWidget *window)
{
  window_stack = g_list_append(window_stack, window);
}

/* Unget all windows in the stack */
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

static void
remove_piece(int row, int p_index, int just_push)
{
  struct osd_piece * p;
  GtkWidget *da;

  g_assert(osd_started == TRUE);

  if (row < 0 || row >= MAX_ROWS || p_index < 0 ||
      p_index >= osd_matrix[row]->n_pieces)
    return;

  p = &(osd_matrix[row]->pieces[p_index]);

  if (p->scaled)
    gdk_pixbuf_unref(p->scaled);
  gdk_pixbuf_unref(p->unscaled);

  if (p->window)
    {
      da = GTK_BIN(p->window)->child;
      gtk_signal_disconnect_by_func(GTK_OBJECT(da),
				    GTK_SIGNAL_FUNC(on_osd_expose_event), p);

      if (!just_push)
	unget_window(p->window);
      else
	push_window(p->window);
    }

  if (p_index != (osd_matrix[row]->n_pieces - 1))
    memcpy(p, p+1,
	   sizeof(struct osd_piece)*((osd_matrix[row]->n_pieces-p_index)-1));

  osd_matrix[row]->n_pieces--;
}

/* Returns the index of the piece. c has at least width chars */
static int
add_piece(int col, int row, int width, attr_char *c)
{
  struct osd_piece p;
  GtkWidget *da;
  struct osd_piece *pp;

  g_assert(osd_started == TRUE);
  g_assert(row >= 0);
  g_assert(row < MAX_COLUMNS);
  g_assert(col >= 0);
  g_assert(col+width <= MAX_COLUMNS);

  memset(&p, 0, sizeof(p));

  p.width = width;
  p.start = col;
  p.window = pop_window();
  da = GTK_BIN(p.window)->child;

  if (osd_page.columns < 40) /* naive cc test */
    {
      p.unscaled = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
			          CELL_WIDTH*p.width, CELL_HEIGHT);
      vbi_draw_cc_page_region(&osd_page,
          (uint32_t *) gdk_pixbuf_get_pixels(p.unscaled),
	  col, row, width, 1 /* height */,
          gdk_pixbuf_get_rowstride(p.unscaled));
    }
  else
    {
      p.unscaled = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
			          12*p.width, 10);
      vbi_draw_vt_page_region(&osd_page,
          (uint32_t *) gdk_pixbuf_get_pixels(p.unscaled),
          col, row, width, 1 /* height */,
	  gdk_pixbuf_get_rowstride(p.unscaled),
	  1 /* reveal */, 1 /* flash_on */);
    }

  pp = &(osd_matrix[row]->pieces[osd_matrix[row]->n_pieces]);
  memcpy(pp, &p, sizeof(struct osd_piece));

  osd_matrix[row]->n_pieces++;

  gtk_signal_connect(GTK_OBJECT(da), "expose-event",
		     GTK_SIGNAL_FUNC(on_osd_expose_event), pp);

  set_piece_geometry(row, osd_matrix[row]->n_pieces-1);

  if ((osd_window && x11_window_viewable(osd_window->window)) ||
      (cw > 0 && ch > 0))
    gtk_widget_show(pp->window);

  return osd_matrix[row]->n_pieces-1;
}

/* Draws the text */
static void
add_piece_osd(const gchar *buf)
{
  struct osd_piece p;
  GtkWidget *da;
  struct osd_piece *pp;
  gint row = MAX_ROWS-1;
  GdkPixmap *canvas;
  GdkFont *font; /* FIXME: font, gc, fg and bg should be cached */
  GdkColor fg, bg;
  GdkGC *gc;
  gint w, s_w, h, s_h;
  gint lbearing, rbearing, width, ascent, descent;

  g_assert(osd_started == TRUE);

  memset(&p, 0, sizeof(p));

  p.width = strlen(buf)*2;
  p.start = -1;
  p.window = pop_window();
  da = GTK_BIN(p.window)->child;

  /* Draw the text, scale and antialias */
  gc = gdk_gc_new(da->window);
  g_assert(gc != NULL);
  fg.red = zcg_float(NULL, "fg_r") * 65535;
  fg.green = zcg_float(NULL, "fg_g") * 65535;
  fg.blue = zcg_float(NULL, "fg_b") * 65535;
  if (!gdk_colormap_alloc_color(gdk_colormap_get_system(),
				&fg, FALSE, TRUE))
    g_error("Couldn't allocate fg");
  bg.red = zcg_float(NULL, "bg_r") * 65535;
  bg.green = zcg_float(NULL, "bg_g") * 65535;
  bg.blue = zcg_float(NULL, "bg_b") * 65535;
  if (!gdk_colormap_alloc_color(gdk_colormap_get_system(),
				&bg, FALSE, TRUE))
    g_error("Couldn't allocate bg");
  gdk_gc_set_background(gc, &bg);
  font = gdk_font_load(zcg_char(NULL, "font"));
  if (!font)
    {
      g_warning("Cannot create fontset");
      return;
    }
  gdk_font_ref(font);
  gdk_string_extents(font, buf, &lbearing, &rbearing, &width, &ascent,
		     &descent);
  s_w = width;
  s_h = ascent+descent;
  w = s_w+width;
  h = s_h*2;
  gtk_widget_realize(da);
  canvas = gdk_pixmap_new(da->window, w, h, -1);
  g_assert(canvas);
  gdk_gc_set_foreground(gc, &bg);
  gdk_draw_rectangle(canvas, gc, TRUE, 0, 0, w, h);
  gdk_gc_set_foreground(gc, &fg);
  gdk_draw_string(canvas, font, gc, (w-s_w)/2, ascent+(h-s_h)/2, buf);
  /* draw text to canvas, dump to pixbuf */
  p.unscaled =
    gdk_pixbuf_get_from_drawable(NULL, canvas,
				 gdk_window_get_colormap(da->window),
				 0, 0, 0, 0, w, h);
  gdk_pixmap_unref(canvas);
  gdk_colormap_free_colors(gdk_colormap_get_system(), &fg, 1);
  gdk_colormap_free_colors(gdk_colormap_get_system(), &bg, 1);
  gdk_font_unref(font);

  pp = &(osd_matrix[row]->pieces[osd_matrix[row]->n_pieces]);
  memcpy(pp, &p, sizeof(struct osd_piece));

  osd_matrix[row]->n_pieces++;

  gtk_signal_connect(GTK_OBJECT(da), "expose-event",
		     GTK_SIGNAL_FUNC(on_osd_expose_event), pp);

  set_piece_geometry(row, osd_matrix[row]->n_pieces-1);

  if ((osd_window && x11_window_viewable(osd_window->window)) ||
      (cw > 0 && ch > 0))
    gtk_widget_show(pp->window);
}

static void osd_clear_row(int row, int just_push)
{
  g_assert(osd_started == TRUE);
  g_assert(row >= 0);
  g_assert(row < MAX_ROWS);

  while (osd_matrix[row]->n_pieces)
    remove_piece(row, osd_matrix[row]->n_pieces-1, just_push);
}

/* These routines (clear, render, roll_up) are the caption.c
   counterparts, they will communicate with the CC engine using an
   event fifo. This is needed since all GDK/GTK
   calls should be done from the main thread. */
/* {mhs} Now subroutines of osd_event. */

void osd_clear(void)
{
  int i;

  g_assert(osd_started == TRUE);

  for (i=0; i<MAX_ROWS; i++)
    osd_clear_row(i, 0);

  zmodel_changed(osd_model);
}

void osd_render(void)
{
  attr_char *ac_row;
  int row, i, j;

  g_assert(osd_started == TRUE);

  row = osd_page.dirty.y0;
  ac_row = osd_page.text + row * osd_page.columns;

  for (; row <= osd_page.dirty.y1; ac_row += osd_page.columns, row++)
    {
      osd_clear_row(row, 1);
      for (i = j = 0; i < osd_page.columns; i++)
        {
	  if (ac_row[i].opacity != TRANSPARENT_SPACE)
	    j++;
	  else if (j > 0)
	    {
	      add_piece(i - j, row, j, ac_row + i - j);
	      j = 0;
	    }
	}

      if (j)
	add_piece(osd_page.columns - j, row, j,
		  ac_row + osd_page.columns - j);

      clear_stack();
    }

  zmodel_changed(osd_model);
}

void osd_roll_up(attr_char *buffer, int first_row, int last_row)
{
  gint i;
  struct osd_row *tmp;

  g_assert(osd_started == TRUE);

  for (; first_row < last_row; first_row++)
    {
      osd_clear_row(first_row, 0);
      tmp = osd_matrix[first_row];
      osd_matrix[first_row] = osd_matrix[first_row+1];
      osd_matrix[first_row+1] = tmp;
      for (i=0; i<osd_matrix[first_row]->n_pieces; i++)
	set_piece_geometry(first_row, i);
    }

  zmodel_changed(osd_model);
}

static void
osd_event			(gpointer	   data,
                                 gint              source, 
				 GdkInputCondition condition)
{
  struct vbi *vbi = zvbi_get_object();
  char dummy[16];
  extern int zvbi_page, zvbi_subpage;

  if (!vbi)
    return;

  if (read(osd_pipe[0], dummy, 16 /* flush */) <= 0
      || !osd_status)
    return;

  if (zvbi_page <= 8)
    {
      if (!vbi_fetch_cc_page(vbi, &osd_page, zvbi_page))
        return; /* trouble in outer space */
    }
  else
    {
      if (!vbi_fetch_vt_page(vbi, &osd_page, zvbi_page, zvbi_subpage,
          25 /* rows */, 1 /* nav */))
        return;
    }

  if (osd_page.dirty.y0 > osd_page.dirty.y1)
    return; /* not dirty (caption only) */

  if (abs(osd_page.dirty.roll) >= osd_page.rows)
    {
      osd_clear();
      return;
    }

  if (osd_page.dirty.roll == -1)
    {
      osd_roll_up(osd_page.text + osd_page.dirty.y0 * osd_page.columns,
                  osd_page.dirty.y0, osd_page.dirty.y1);
      return;
    }

  g_assert(osd_page.dirty.roll == 0);
    /* currently never down or more than one row */

  osd_render();
}

static void
osd_render_osd_sgml	(const attr_char *chars, int len)
{
  int i;
  gchar *buf = g_strdup("");
  ucs2_t tmp[2];
  GtkWidget *window;

  tmp[1] = 0;
  for (i=0; i<len; i++)
    {
      char *buf2, *buf3;
      tmp[0] = chars[i].glyph;
      buf2 = ucs22local(tmp);
      buf3 = g_strconcat(buf, buf2, NULL);
      free(buf2);
      g_free(buf);
      buf = buf3;
    }

  osd_clear_row(MAX_ROWS-1, FALSE);

  window = pop_window();

  add_piece_osd(buf);

  g_free(buf);
}

void
osd_render_sgml		(const char *string, ...)
{
  va_list args;
  gchar *buf;
  ucs2_t tmp[2];
  int len, i;
  attr_char *chars;
  
  if (!string)
    return;
  
  va_start(args, string);
  buf = g_strdup_vprintf(string, args);
  va_end(args);

  if (!(chars = sgml2attr_char(buf, &len)))
    return;
  g_free(buf);

  switch (zcg_int(NULL, "osd_type"))
    {
    case 0: /* OSD */
      osd_render_osd_sgml(chars, len);
      break;
    case 1: /* Status bar */
      buf = g_strdup("");
      tmp[1] = 0;
      for (i=0; i<len; i++)
	{
	  char *buf2, *buf3;
	  tmp[0] = chars[i].glyph;
	  buf2 = ucs22local(tmp);
	  buf3 = g_strconcat(buf, buf2, NULL);
	  free(buf2);
	  g_free(buf);
	  buf = buf3;
	}
      z_status_print(buf);
      g_free(buf);
      break;
    case 2: /* Console */
      printf("OSD: ");
      tmp[1] = 0;
      for (i=0; i<len; i++)
	{
	  tmp[0] = chars[i].glyph;
	  buf = ucs22local(tmp);
	  printf(buf);
	  free(buf);
	}
      printf("\n");
      break;
    default: /* ignore */
      break;
    }

  free(chars);
}

