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

/*
 * This code is used to communicate with the VBI device (usually
 * /dev/vbi), so multiple plugins can access it simultaneously.
 * The code uses libvbi, written by Michael Schimek.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libvbi.h>
#include <pthread.h>
#include <ctype.h>
#include <time.h>
#include <pthread.h>

#include "tveng.h"
/* Manages config values for zconf (it saves me some typing) */
#define ZCONF_DOMAIN "/zapping/options/vbi/"
#include "zconf.h"
#include "zvbi.h"
#include "zmisc.h"
#include "interface.h"

#undef TRUE
#undef FALSE
#include "../common/fifo.h"

extern GtkWidget *main_window;
extern tveng_device_info *main_info;

/*
  Quality-speed tradeoff when scaling+antialiasing the page:
  - GDK_INTERP_NEAREST: Very fast scaling but visually pure rubbish
  - GDK_INTERP_TILES: Slower, and doesn't look very good at high (>2x)
			scalings
  - GDK_INTERP_BILINEAR: Very good quality, but slower than nearest
			and tiles.
  - GDK_INTERP_HYPER: Slower than Bilinear, slightly better at very
			high (>3x) scalings
*/
#define INTERP_MODE GDK_INTERP_BILINEAR

static struct vbi *vbi=NULL; /* holds the vbi object */
static pthread_t zvbi_thread_id; /* VBI thread in libvbi */

/**
 * The blink of items in the page is done by applying the patch once
 * every second (whenever the client wishes) to the appropiate places
 * in the screen.
 */
struct ttx_patch {
  int		col, row, width, height; /* geometry of the patch */
  GdkPixbuf	*unscaled_on;
  GdkPixbuf	*unscaled_off;
  GdkPixbuf	*scaled_on;
  GdkPixbuf	*scaled_off;
  gboolean	phase; /* flash phase 0 ... 3, 0 == off */
};

struct ttx_client {
  fifo		mqueue;
  int		page, subpage; /* monitored page, subpage */
  int		id; /* of the client */
  pthread_mutex_t mutex;
  struct fmt_page fp; /* formatted page */
  GdkPixbuf	*unscaled_on; /* unscaled version of the page (on) */
  GdkPixbuf	*unscaled_off;
  GdkPixbuf	*scaled; /* scaled version of the page */
  int		w, h;
  int		freezed; /* do not refresh the current page */
  int		num_patches;
  int		reveal; /* whether to show hidden chars */
  struct ttx_patch *patches; /* patches to be applied */
};

static GList *ttx_clients = NULL;
static pthread_mutex_t clients_mutex; /* FIXME: A rwlock is better for
				       this */

/* handler for a vbi event */
static void event(vbi_event *ev, void *unused);

/* Some info about the last processed header, protected by a mutex */
static struct {
  pthread_mutex_t mutex;

  /* Generic info */
  /* xpacket condition, signalled when the station name is parsed */
  pthread_cond_t xpacket_cond;
  char xpacket[32];
  char header[64];
  char *name; /* usually, something like station-Teletext or similar
		 (Antena3-Teletexto, for example) */
  /* Pre-processed info from the header, for convenience */
  int hour, min, sec;
} last_info;

static void
on_vbi_prefs_changed		(const gchar *key,
				 gpointer data)
{
  /* Try to open the device */
  if (!vbi && zcg_bool(NULL, "enable_vbi"))
    {
      g_message("Opening the device");
      if (!zvbi_open_device())
	{
	  ShowBox(_("Sorry, but %s couldn't be opened:\n%s (%d)"),
		  GNOME_MESSAGE_BOX_ERROR, "/dev/vbi",
		  strerror(errno), errno);
	}
    }
  if (vbi && !zcg_bool(NULL, "enable_vbi"))
    {
      g_message("closing the device");
      /* TTX mode */
      if (main_info->current_mode == TVENG_NO_CAPTURE)
	zmisc_switch_mode(TVENG_CAPTURE_WINDOW, main_info);
      zvbi_close_device();
    }

  /* disable VBI if needed */
  if (!zvbi_get_object())
    {
      printv("VBI disabled, removing GUI items\n");
      gtk_widget_set_sensitive(lookup_widget(main_window, "separador5"),
			       FALSE);
      gtk_widget_hide(lookup_widget(main_window, "separador5"));
      gtk_widget_set_sensitive(lookup_widget(main_window, "videotext1"),
			       FALSE);
      gtk_widget_hide(lookup_widget(main_window, "videotext1"));
      gtk_widget_set_sensitive(lookup_widget(main_window, "videotext3"),
			       FALSE);
      gtk_widget_hide(lookup_widget(main_window, "videotext3"));
      gtk_widget_set_sensitive(lookup_widget(main_window, "new_ttxview"),
			       FALSE);
      gtk_widget_hide(lookup_widget(main_window, "new_ttxview"));
      gtk_widget_set_sensitive(lookup_widget(main_window,
					     "closed_caption1"),
			       FALSE);
      gtk_widget_hide(lookup_widget(main_window, "closed_caption1"));
      gtk_widget_hide(lookup_widget(main_window, "separator8"));
      /* Set the capture mode to a default value and disable VBI */
      if (zcg_int(NULL, "capture_mode") == TVENG_NO_CAPTURE)
	zcs_int(TVENG_CAPTURE_READ, "capture_mode");
    }
  else
    {
      printv("VBI enabled, removing GUI items\n");
      gtk_widget_set_sensitive(lookup_widget(main_window, "separador5"),
			       TRUE);
      gtk_widget_show(lookup_widget(main_window, "separador5"));
      gtk_widget_set_sensitive(lookup_widget(main_window, "videotext1"),
			       TRUE);
      gtk_widget_show(lookup_widget(main_window, "videotext1"));
      gtk_widget_set_sensitive(lookup_widget(main_window, "videotext3"),
			       TRUE);
      gtk_widget_show(lookup_widget(main_window, "videotext3"));
      gtk_widget_set_sensitive(lookup_widget(main_window, "new_ttxview"),
			       TRUE);
      gtk_widget_show(lookup_widget(main_window, "new_ttxview"));
      gtk_widget_set_sensitive(lookup_widget(main_window,
					     "closed_caption1"),
			       TRUE);
      gtk_widget_show(lookup_widget(main_window, "closed_caption1"));
      gtk_widget_show(lookup_widget(main_window, "separator8"));
    }
}

void
startup_zvbi(void)
{
  zcc_bool(FALSE, "Enable VBI decoding", "enable_vbi");
  zcc_bool(TRUE, "Use VBI for getting station names", "use_vbi");
  zcc_char("/dev/vbi", "VBI device", "vbi_device");
  zcc_int(0, "Default TTX region", "default_region");

  zconf_add_hook("/zapping/options/vbi/enable_vbi",
		 (ZConfHook)on_vbi_prefs_changed, (gpointer)0xdeadbeef);
}

void shutdown_zvbi(void)
{
  if (vbi)
    zvbi_close_device();
}

int test_pipe[2];

/* Open the configured VBI device, FALSE on error */
gboolean
zvbi_open_device(void)
{
  gchar *device;
  gint index;
  static int region_mapping[8] = {
    0, /* WCE */
    8, /* EE */
    16, /* WET */
    24, /* CSE */
    32, /* C */
    48, /* GC */
    64, /* A */
    80 /* I */
  };

  if ((vbi) || (!zcg_bool(NULL, "enable_vbi")))
    return FALSE; /* this code isn't reentrant */

  device = zcg_char(NULL, "vbi_device");

  if (pipe(test_pipe)) {
    fprintf(stderr, "pipe! pipe! duh.\n");
    exit(EXIT_FAILURE);
  }

  if (!(vbi = vbi_open(device, cache_open())))
    {
      g_warning("cannot open %s, vbi services will be disabled",
		device);
      goto error;
    }

  if (vbi->cache)
    vbi->cache->op->mode(vbi->cache, CACHE_MODE_ERC, 1);

  vbi_event_handler(vbi, ~0, event, NULL);

  last_info.name = NULL;
  last_info.hour = last_info.min = last_info.sec = -1;

  pthread_mutex_init(&(last_info.mutex), NULL);
  pthread_cond_init(&(last_info.xpacket_cond), NULL);

  if (pthread_create(&zvbi_thread_id, NULL, vbi_mainloop, vbi))
    {
      vbi_event_handler(vbi, 0, event, NULL);
      vbi_close(vbi);
      vbi = NULL;
      goto error;
    }

  index = zcg_int(NULL, "default_region");
  if (index < 0)
    index = 0;
  if (index > 7)
    index = 7;
  vbi_set_default_region(vbi, region_mapping[index]);
  pthread_mutex_init(&clients_mutex, NULL);

  return TRUE;

 error:

  ShowBox(_("VBI: %s couldn't be opened: %d, %s"),
	  GNOME_MESSAGE_BOX_ERROR, device, errno, strerror(errno));
  return FALSE;
}

/* down the road */
static void remove_client(struct ttx_client* client);

/* Closes the VBI device */
void
zvbi_close_device(void)
{
  GList * destruction;

  if (!vbi) /* disabled */
    return;

  vbi->quit = 1;
  pthread_join(zvbi_thread_id, NULL);

  pthread_mutex_destroy(&(last_info.mutex));
  pthread_cond_destroy(&(last_info.xpacket_cond));

  vbi_event_handler(vbi, 0, event, NULL);
  pthread_mutex_lock(&clients_mutex);
  destruction = g_list_first(ttx_clients);
  while (destruction)
    {
      remove_client((struct ttx_client*) destruction->data);
      destruction = destruction->next;
    }
  g_list_free(ttx_clients);
  ttx_clients = NULL;
  pthread_mutex_unlock(&clients_mutex);
  pthread_mutex_destroy(&clients_mutex);
  vbi_close(vbi);
  vbi = NULL;
  close(test_pipe[0]);
  close(test_pipe[1]);
}

static void
send_ttx_message(struct ttx_client *client,
		 enum ttx_message message)
{
  buffer *b = wait_empty_buffer(&client->mqueue);
  b->data = GINT_TO_POINTER(message);
  send_full_buffer(&client->mqueue, b);
}

static void
remove_client(struct ttx_client *client)
{
  gint i;

  uninit_fifo(&client->mqueue);
  pthread_mutex_destroy(&client->mutex);
  gdk_pixbuf_unref(client->unscaled_on);
  gdk_pixbuf_unref(client->unscaled_off);
  if (client->scaled)
    gdk_pixbuf_unref(client->scaled);
  for (i = 0; i<client->num_patches; i++)
    {
      gdk_pixbuf_unref(client->patches[i].unscaled_on);
      gdk_pixbuf_unref(client->patches[i].unscaled_off);
      if (client->patches[i].scaled_on)
	gdk_pixbuf_unref(client->patches[i].scaled_on);
      if (client->patches[i].scaled_off)
	gdk_pixbuf_unref(client->patches[i].scaled_off);
    }
  if (client->num_patches)
    g_free(client->patches);
  g_free(client);
}

/* returns the id */
int
register_ttx_client(void)
{
  static int id;
  struct ttx_client *client;
  gchar *filename;
  int w, h; /* of the unscaled image */
  GdkPixbuf *simple;

  client = g_malloc(sizeof(struct ttx_client));
  memset(client, 0, sizeof(struct ttx_client));
  client->id = id++;
  client->reveal = 0;
  pthread_mutex_init(&client->mutex, NULL);
  filename = g_strdup_printf("%s/%s%d.jpeg", PACKAGE_DATA_DIR,
			     "../pixmaps/zapping/vt_loading",
			     (rand()%2)+1);
  vbi_get_rendered_size(&w, &h);
  client->unscaled_on = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, w,
				    h);
  client->unscaled_off = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, w,
				    h);
  
  g_assert(client->unscaled_on != NULL);
  g_assert(client->unscaled_off != NULL);
  
  simple = gdk_pixbuf_new_from_file(filename);
  g_free(filename);
  if (simple)
    {
      gdk_pixbuf_scale(simple,
		       client->unscaled_on, 0, 0, w, h,
		       0, 0,
		       (double) w / gdk_pixbuf_get_width(simple),
		       (double) h / gdk_pixbuf_get_height(simple),
		       INTERP_MODE);
      z_pixbuf_copy_area(client->unscaled_on, 0, 0, w, h,
			   client->unscaled_off, 0, 0);
      gdk_pixbuf_unref(simple);
    }

  g_assert(init_buffered_fifo(&client->mqueue, NULL, 16, 0) > 0);
  pthread_mutex_lock(&clients_mutex);
  ttx_clients = g_list_append(ttx_clients, client);
  pthread_mutex_unlock(&clients_mutex);
  return client->id;
}

static inline struct ttx_client*
find_client(int id)
{
  GList *find;

  find= g_list_first(ttx_clients);
  while (find)
    {
      if (((struct ttx_client*)find->data)->id == id)
	return ((struct ttx_client*)find->data);

      find = find->next;
    }

  return NULL; /* not found */
}

void
set_ttx_parameters(int id, int reveal)
{
  struct ttx_client *client;

  pthread_mutex_lock(&clients_mutex);
  if ((client = find_client(id)))
    {
      client->reveal = reveal;
    }
  pthread_mutex_unlock(&clients_mutex);
}

struct fmt_page*
get_ttx_fmt_page(int id)
{
  struct ttx_client *client;
  struct fmt_page *result=NULL;

  pthread_mutex_lock(&clients_mutex);
  if ((client = find_client(id)))
    result = &client->fp;
  pthread_mutex_unlock(&clients_mutex);

  return result;
}

enum ttx_message
peek_ttx_message(int id)
{
  struct ttx_client *client;
  buffer *b;
  enum ttx_message message;

  pthread_mutex_lock(&clients_mutex);

  if ((client = find_client(id)))
    {
      b = recv_full_buffer(&client->mqueue);
      if (b)
	{
	  message = GPOINTER_TO_INT(b->data);
	  send_empty_buffer(&client->mqueue, b);
	}
      else
	message = TTX_NONE;
    }
  else
    message = TTX_BROKEN_PIPE;

  pthread_mutex_unlock(&clients_mutex);

  return message;
}

enum ttx_message
get_ttx_message(int id)
{
  struct ttx_client *client;
  buffer *b;
  enum ttx_message message;

  pthread_mutex_lock(&clients_mutex);

  if ((client = find_client(id)))
    {
      b = wait_full_buffer(&client->mqueue);
      g_assert(b != NULL);
      message = GPOINTER_TO_INT(b->data);
      send_empty_buffer(&client->mqueue, b);
    }
  else
    message = TTX_BROKEN_PIPE;

  pthread_mutex_unlock(&clients_mutex);

  return message;
}

static void
clear_message_queue(struct ttx_client *client)
{
  buffer *b;

  while ((b=recv_full_buffer(&client->mqueue)))
    send_empty_buffer(&client->mqueue, b);
}

void
unregister_ttx_client(int id)
{
  GList *search;

  pthread_mutex_lock(&clients_mutex);
  search = g_list_first(ttx_clients);
  while (search)
    {
      if (((struct ttx_client*)search->data)->id == id)
	{
	  remove_client((struct ttx_client*)search->data);
	  ttx_clients = g_list_remove(ttx_clients, search->data);
	  break;
	}
      search = search->next;
    }
  pthread_mutex_unlock(&clients_mutex);
}

/* Won't change */
#define CW		12		
#define CH		10

static void
add_patch(struct ttx_client *client, int col, int row, attr_char *ac)
{
  struct ttx_patch patch;
  gint sw, sh; /* scaled dimensions */

  memset(&patch, 0, sizeof(struct ttx_patch));
  patch.width = patch.height = 1;
  patch.col = col;
  patch.row = row;

  switch (ac->size)
    {
    case DOUBLE_WIDTH:
      patch.width = 2;
      break;
    case DOUBLE_HEIGHT:
      patch.height = 2;
      break;
    case DOUBLE_SIZE:
      patch.width = patch.height = 2;
      break;
    default:
      break;
    }

  sw = (client->w*(patch.width*CW+10))/
    gdk_pixbuf_get_width(client->unscaled_on);
  sh = (client->h*(patch.height*CH+10))/
    gdk_pixbuf_get_height(client->unscaled_on);

  patch.unscaled_on = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
				     patch.width*CW+10, patch.height*CH+10);
  patch.unscaled_off = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
				      patch.width*CW+10, patch.height*CH+10);
  g_assert(patch.unscaled_on != NULL);
  g_assert(patch.unscaled_off != NULL);

  z_pixbuf_copy_area(client->unscaled_on, patch.col*CW-5,
		       patch.row*CH-5, patch.width*CW+10,
		       patch.height*CH+10, patch.unscaled_on, 0, 0);
  z_pixbuf_copy_area(client->unscaled_off, patch.col*CW-5,
		     patch.row*CH-5, patch.width*CW+10,
		     patch.height*CH+10, patch.unscaled_off, 0, 0);

  if (client->w > 0  &&  client->h > 0)
    {
      patch.scaled_on =
	gdk_pixbuf_scale_simple(patch.unscaled_on, sw, sh, INTERP_MODE);
      patch.scaled_off =
	gdk_pixbuf_scale_simple(patch.unscaled_off, sw, sh, INTERP_MODE);
    }

  client->patches = g_realloc(client->patches, sizeof(struct ttx_patch)*
			      (client->num_patches+1));
  memcpy(client->patches+client->num_patches, &patch,
	 sizeof(struct ttx_patch));
  client->num_patches++;
}

/**
 * Resizes the set of patches to fit in the new geometry
 */
static void
resize_patches(struct ttx_client *client)
{
  gint i;
  gint sw, sh;

  for (i=0; i<client->num_patches; i++)
    {
      sw = (client->w*(client->patches[i].width*CW+10))/
	gdk_pixbuf_get_width(client->unscaled_on);
      sh = (client->h*(client->patches[i].height*CH+10))/
	gdk_pixbuf_get_height(client->unscaled_on);

      if (client->patches[i].scaled_on)
	gdk_pixbuf_unref(client->patches[i].scaled_on);
      client->patches[i].scaled_on =
	gdk_pixbuf_scale_simple(client->patches[i].unscaled_on,
				sw, sh, INTERP_MODE);
      if (client->patches[i].scaled_off)
	gdk_pixbuf_unref(client->patches[i].scaled_off);
      client->patches[i].scaled_off =
	gdk_pixbuf_scale_simple(client->patches[i].unscaled_off,
				sw, sh, INTERP_MODE);
    }
}

/**
 * Scans the current page of the client and builds the appropiate set
 * of patches.
 */
static void
build_patches(struct ttx_client *client)
{
  gint i;
  gint col, row;
  attr_char *ac;

  for (i = 0; i<client->num_patches; i++)
    {
      if (client->patches[i].scaled_on)
	gdk_pixbuf_unref(client->patches[i].scaled_on);
      if (client->patches[i].scaled_off)
	gdk_pixbuf_unref(client->patches[i].scaled_off);
    }
  g_free(client->patches);
  client->patches = NULL;
  client->num_patches = 0;

  /* FIXME: This is too cumbersome, something more smart is needed */
  for (col = 0; col < client->fp.columns; col++)
    for (row = 0; row < client->fp.rows; row++)
      {
	ac = &client->fp.text[row * client->fp.columns + col];
	if ((ac->flash) && (ac->size <= DOUBLE_SIZE))
	  add_patch(client, col, row, ac);
      }
}

/**
 * Applies the patches into the page.
 */
static void
refresh_ttx_page_intern(struct ttx_client *client, GtkWidget *drawable)
{
  int i;
  struct ttx_patch *p;
  GdkPixbuf *scaled;
  gint x, y;
  gint sx, sy;

  for (i=0; i<client->num_patches; i++)
    {
      p = &(client->patches[i]);
      p->phase = (p->phase + 1) & 3;
      if (p->phase > 1)
	continue;
      else if (p->phase)
	scaled = p->scaled_on;
	  else
	    scaled = p->scaled_off;
      
      /* Update the scaled version of the page */
      if ((scaled) && (client->scaled) && (client->w > 0)
	  && (client->h > 0))
	{
	  /* fixme: roundoff */
	  x = (client->w*p->col)/client->fp.columns;
	  y = (client->h*p->row)/client->fp.rows;
	  sx = (gdk_pixbuf_get_width(scaled)*5)/
	    gdk_pixbuf_get_width(p->unscaled_on);
	  sy = (gdk_pixbuf_get_height(scaled)*5)/
	    gdk_pixbuf_get_height(p->unscaled_on);
	  
	  z_pixbuf_copy_area(scaled, sx, sy,
			     gdk_pixbuf_get_width(scaled)-sx*2,
			     gdk_pixbuf_get_height(scaled)-sy*2,
			     client->scaled,
			     x, y);
	  
	  if (drawable)
	    gdk_window_clear_area_e(drawable->window, x, y,
				    gdk_pixbuf_get_width(scaled)-sx*2,
				    gdk_pixbuf_get_height(scaled)-sy*2);
	}
    }
}

void
refresh_ttx_page(int id, GtkWidget *drawable)
{
  struct ttx_client *client;

  pthread_mutex_lock(&clients_mutex);
  if ((client = find_client(id)))
    {
      pthread_mutex_lock(&client->mutex);
      refresh_ttx_page_intern(client, drawable);
      pthread_mutex_unlock(&client->mutex);
    }
  pthread_mutex_unlock(&clients_mutex);
}

static int
build_client_page(struct ttx_client *client, int page, int subpage)
{
  GdkPixbuf *simple;
  gchar *filename;

  g_assert(client != NULL);

  pthread_mutex_lock(&client->mutex);
  if (page > 0)
    {
      if (!vbi_fetch_vt_page(vbi, &client->fp, page, subpage, 25, 1))
        {
	  pthread_mutex_unlock(&client->mutex);
	  return 0;
	}
      vbi_draw_page(&client->fp,
		    gdk_pixbuf_get_pixels(client->unscaled_on),
		    client->reveal);
      vbi_draw_page_region(&client->fp,
			   gdk_pixbuf_get_pixels(client->unscaled_off),
			   client->reveal, 0, 0, 40, 25, -1, 0);
    }
  else if (page == 0)
    {
      memset(&client->fp, 0, sizeof(struct fmt_page));
      filename = g_strdup_printf("%s/%s%d.jpeg", PACKAGE_DATA_DIR,
				 "../pixmaps/zapping/vt_loading",
				 (rand()%2)+1);
  
      simple = gdk_pixbuf_new_from_file(filename);
      g_free(filename);
      if (simple)
	{
	  gdk_pixbuf_scale(simple,
			   client->unscaled_on, 0, 0,
			   gdk_pixbuf_get_width(client->unscaled_on),
			   gdk_pixbuf_get_height(client->unscaled_on),
			   0, 0,
			   (double)
			   gdk_pixbuf_get_width(client->unscaled_on) /
			   gdk_pixbuf_get_width(simple),
			   (double)
			   gdk_pixbuf_get_height(client->unscaled_on) /
			   gdk_pixbuf_get_height(simple),
			   INTERP_MODE);
	  z_pixbuf_copy_area(client->unscaled_on, 0, 0,
			       gdk_pixbuf_get_width(client->unscaled_on),
			       gdk_pixbuf_get_height(client->unscaled_off),
			       client->unscaled_off, 0, 0);
	  gdk_pixbuf_unref(simple);
	}
    }

  if ((!client->scaled) &&
      (client->w > 0) &&
      (client->h > 0))
    client->scaled = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
				    client->w, client->h);
  if (client->scaled)
    gdk_pixbuf_scale(client->unscaled_on,
		     client->scaled, 0, 0, client->w, client->h,
		     0, 0,
		     (double) client->w /
		     gdk_pixbuf_get_width(client->unscaled_on),
		     (double) client->h /
		      gdk_pixbuf_get_height(client->unscaled_on),
		     INTERP_MODE);

  build_patches(client);
  refresh_ttx_page_intern(client, NULL);

  pthread_mutex_unlock(&client->mutex);
  return 1; /* success */
}

void
monitor_ttx_page(int id/*client*/, int page, int subpage)
{
  struct ttx_client *client;

  pthread_mutex_lock(&clients_mutex);
  client = find_client(id);
  if (client)
    {
      client->freezed = FALSE;
      client->page = page;
      client->subpage = subpage;
      if ((page >= 0x100) && (page <= 0x899)) {
        if (build_client_page(client, page, subpage))
	  {
	    clear_message_queue(client);
	    send_ttx_message(client, TTX_PAGE_RECEIVED);
	  }
      } else {
	build_client_page(client, 0, 0);
	clear_message_queue(client);
	send_ttx_message(client, TTX_PAGE_RECEIVED);
      }
    }
  pthread_mutex_unlock(&clients_mutex);
}

void monitor_ttx_this(int id, struct fmt_page *pg)
{
  struct ttx_client *client;

  if (!pg)
    return;

  pthread_mutex_lock(&clients_mutex);
  if ((client = find_client(id)))
    {
      client->page = pg->pgno;
      client->subpage = pg->subno;
      client->freezed = TRUE;
      memcpy(&client->fp, pg, sizeof(struct fmt_page));
      vbi_draw_page(&client->fp,
		    gdk_pixbuf_get_pixels(client->unscaled_on),
		    client->reveal);
      vbi_draw_page_region(&client->fp,
			   gdk_pixbuf_get_pixels(client->unscaled_off),
			   client->reveal, 0, 0, 40, 25, -1, 0);
      build_client_page(client, -1, -1);
      clear_message_queue(client);
      send_ttx_message(client, TTX_PAGE_RECEIVED);
    }
  pthread_mutex_unlock(&clients_mutex);
}

void
ttx_freeze (int id)
{
  struct ttx_client *client;

  pthread_mutex_lock(&clients_mutex);
  if ((client = find_client(id)))
    client->freezed = TRUE;
  pthread_mutex_unlock(&clients_mutex);  
}

void
ttx_unfreeze (int id)
{
  struct ttx_client *client;

  pthread_mutex_lock(&clients_mutex);
  if ((client = find_client(id)))
    client->freezed = FALSE;
  pthread_mutex_unlock(&clients_mutex);  
}

static void
notify_clients(int page, int subpage)
{
  GList *p;
  struct ttx_client *client;

  pthread_mutex_lock(&clients_mutex);
  p = g_list_first(ttx_clients);
  while (p)
    {
      client = (struct ttx_client*)p->data;
      if ((client->page == page) && (!client->freezed) &&
	  ((client->subpage == subpage) || (client->subpage == ANY_SUB)))
	{
	  build_client_page(client, page, subpage);
	  send_ttx_message(client, TTX_PAGE_RECEIVED);
	}
      p = p->next;
    }
  pthread_mutex_unlock(&clients_mutex);
}

void resize_ttx_page(int id, int w, int h)
{
  struct ttx_client *client;

  if ((w<=0) || (h<=0))
    return;

  pthread_mutex_lock(&clients_mutex);
  if ((client = find_client(id)))
    {
      pthread_mutex_lock(&client->mutex);
      if ((client->w != w) ||
	  (client->h != h))
	{
	  if (client->scaled)
	    gdk_pixbuf_unref(client->scaled);
	  client->scaled = NULL;
	  client->scaled = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
					  w, h);
	  if (client->scaled)
	    gdk_pixbuf_scale(client->unscaled_on,
			     client->scaled, 0, 0, w, h,
			     0, 0,
			     (double) w /
			     gdk_pixbuf_get_width(client->unscaled_on),
			     (double) h /
			     gdk_pixbuf_get_height(client->unscaled_on),
			     INTERP_MODE);
	  
	  client->w = w;
	  client->h = h;

	  resize_patches(client);

	  /* fixme: apply patches? */
	}
      pthread_mutex_unlock(&client->mutex);
    }
  pthread_mutex_unlock(&clients_mutex);
}

void render_ttx_page(int id, GdkDrawable *drawable,
		     GdkGC *gc,
		     gint src_x, gint src_y,
		     gint dest_x, gint dest_y,
		     gint w, gint h)
{
  struct ttx_client *client;

  pthread_mutex_lock(&clients_mutex);
  if ((client = find_client(id)))
    {
      pthread_mutex_lock(&client->mutex);

      if (client->scaled)
	gdk_pixbuf_render_to_drawable(client->scaled,
				      drawable,
				      gc,
				      src_x, src_y,
				      dest_x, dest_y,
				      w, h,
				      GDK_RGB_DITHER_NORMAL,
				      src_x, src_y);

      pthread_mutex_unlock(&client->mutex);
    }
  pthread_mutex_unlock(&clients_mutex);
}

void
render_ttx_mask(int id, GdkBitmap *mask)
{
  struct ttx_client *client;

  pthread_mutex_lock(&clients_mutex);
  if ((client = find_client(id)))
    {
      pthread_mutex_lock(&client->mutex);
      if (client->scaled)
	gdk_pixbuf_render_threshold_alpha(client->scaled, mask,
					  0, 0, 0, 0,
					  client->w, client->h, 127);
      pthread_mutex_unlock(&client->mutex);
    }
  pthread_mutex_unlock(&clients_mutex);
}

/*
  Returns the global vbi object, or NULL if vbi isn't enabled or
  doesn't work. You can safely use this function to test if VBI works
  (it will return NULL if it doesn't).
*/
struct vbi *
zvbi_get_object(void)
{
  return vbi;
}

/* this is called when we receive a page, header, etc. */
static void
event(vbi_event *ev, void *unused)
{
    unsigned char *p;
    int hour=0, min=0, sec=0;
    char *name, *h;
    
    switch (ev->type) {
    case VBI_EVENT_HEADER:
	p = ev->p1;
	// printv("header %.32s\n", p+8);
	pthread_mutex_lock(&(last_info.mutex));
	memcpy(last_info.header,p+8,32);
	last_info.header[32] = 0;
	pthread_mutex_unlock(&(last_info.mutex));
	/* Parse the time from the header */
	/* first check that we are getting what we expect */
	if ((!(isdigit(last_info.header[25]))) ||
	    (!(isdigit(last_info.header[27]))) ||
	    (!(isdigit(last_info.header[28]))) ||
	    (!(isdigit(last_info.header[30]))) ||
	    (!(isdigit(last_info.header[31]))))
	  {
	    last_info.hour = last_info.min = last_info.sec = -1;
	    break;
	  }
	if (isdigit(last_info.header[24]))
	  hour = last_info.header[24]-'0';
	hour = hour * 10 + (last_info.header[25]-'0');
	min = (last_info.header[27]-'0')*10+(last_info.header[28]-'0');
	sec = (last_info.header[30]-'0')*10+(last_info.header[31]-'0');
	if ((hour >= 24) || (hour < 0) || (min >= 60) || (min < 0) ||
	    (sec >= 60) || (sec < 0))
	  {
	    last_info.hour = last_info.min = last_info.sec = -1;
	    break;
	  }
	pthread_mutex_lock(&(last_info.mutex));
	last_info.hour = hour;
	last_info.min = min;
	last_info.sec = sec;
	pthread_mutex_unlock(&(last_info.mutex));
	break;
    case VBI_EVENT_PAGE:
      {
	static gboolean receiving_pages = FALSE;
	if (!receiving_pages)
	  {
	    receiving_pages = TRUE;
	    printv("Received vtx page %x.%02x\n", ev->pgno,
		   ev->subno & 0xFF);
	  }
      }
	/* Set the dirty flag on the page */
	notify_clients(ev->pgno, ev->subno);
	break;
    case VBI_EVENT_XPACKET:
	p = ev->p1;
	//printv("xpacket %x %x %x %x - %.20s\n",
	// 			p[0],p[1],p[3],p[5],p+20);
	pthread_mutex_lock(&(last_info.mutex));
	memcpy(last_info.xpacket,p+20,20);
	last_info.xpacket[20] = 0;
	
	/* parse the VBI name of the broadcaster */
	last_info.name = NULL;
	if (last_info.xpacket[0]) {
	  for (h = last_info.xpacket+19; h >= last_info.xpacket; h--) {
	    if (' ' != *h)
	      break;
	    *h = 0;
	  }
	  for (name = last_info.xpacket; *name == ' '; name++)
	    ;
	  if (*name)
	    {
	      last_info.name = name;
	      /* signal the condition */
	      pthread_cond_broadcast(&(last_info.xpacket_cond));
	    }
	}
	pthread_mutex_unlock(&(last_info.mutex));
	break;

    case VBI_EVENT_NETWORK:
	printf("Station name: '%s'\n",
		((vbi_network *) ev->p1)->name);
	break;

    default:
    }
}

/*
  Returns a pointer to the name of the Teletext provider, or NULL if
  this name is unknown. You must g_free the returned value.
*/
gchar*
zvbi_get_name(void)
{
  gchar * p = NULL;
  struct timeval now;
  struct timespec timeout;
  int retcode;

  if (!vbi)
    return NULL;

  pthread_mutex_lock(&(last_info.mutex));
  gettimeofday(&now, NULL);
  timeout.tv_sec = now.tv_sec+1; /* Wait one second, then fail */
  timeout.tv_nsec = now.tv_usec * 1000;
  retcode = pthread_cond_timedwait(&(last_info.xpacket_cond),
				   &(last_info.mutex), &timeout);
  if ((retcode != ETIMEDOUT) && (last_info.name))
    p = g_strdup(last_info.name);
  pthread_mutex_unlock(&(last_info.mutex));

  return p;
}

/*
  Fills in the given pointers with the time as it appears in the
  header. The pointers can be NULL.
  If the time isn't known, -1 will be returned in all the fields
*/
void
zvbi_get_time(gint * hour, gint * min, gint * sec)
{
  if (!vbi)
    {
      if (hour)
	*hour = -1;
      if (min)
	*min = -1;
      if (sec)
	*sec = -1;

      return;
    }

  pthread_mutex_lock(&(last_info.mutex));

  if (hour)
    *hour = last_info.hour;
  if (min)
    *min = last_info.min;
  if (sec)
    *sec = last_info.sec;

  pthread_mutex_unlock(&(last_info.mutex));
}
