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
 * Capture management code. The logic is somewhat complex since we
 * have to support many combinations of request, available and
 * displayable formats, and try to find an optimum combination.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#define ZCONF_DOMAIN "/zapping/options/capture/"
#include "zconf.h"

#include <pthread.h>



#include <tveng.h>
#include "common/fifo.h"
#include "zmisc.h"
#include "plugins.h"
#include "capture.h"
#include "csconvert.h"
#include "globals.h"

#include <sched.h>

#define _pthread_rwlock_rdlock(l) pthread_rwlock_rdlock (l)
#define _pthread_rwlock_wrlock(l) pthread_rwlock_wrlock (l)
#define _pthread_rwlock_unlock(l) pthread_rwlock_unlock (l)

/* The capture fifo */
#define NUM_BUNDLES 6 /* in capture_fifo */
static zf_fifo				_capture_fifo;
zf_fifo					*capture_fifo = &_capture_fifo;
/* The frame producer */
static pthread_t			capture_thread_id;
static volatile gboolean		exit_capture_thread;
static volatile gboolean		capture_quit_ack;
/* List of requested capture formats */
static struct {
  gint			id;
  capture_fmt		fmt;
  gboolean		required;
}		*formats = NULL;
static int	num_formats = 0;
static pthread_rwlock_t fmt_rwlock;
static volatile gint	request_id = 0;
/* Capture handlers */
static struct {
  CaptureNotify		notify;
  gpointer		user_data;
  gint			id;
}					*handlers = NULL;
static int				num_handlers = 0;
/* The capture buffers */
typedef struct {
  /* What everyone else sees */
  capture_frame		frame;
  /* When this tag is different from request_id the buffer needs
     rebuilding */
  gint			tag;
  /* Images we can build for this buffer */

  guint			num_images;
  zimage		*images[TV_MAX_PIXFMTS];
  gboolean		converted[TV_MAX_PIXFMTS];
  /* The source image and its index in images */
  zimage		*src_image;
  gint			src_index;
} producer_buffer;

/* Available video formats in the video device */
static tv_pixfmt_set	available_pixfmts = 0;

static GtkWidget *	dest_window = NULL;

static void broadcast (capture_event event)
{
  gint i;

  for (i = 0; i < num_handlers; i++)
    handlers[i].notify (event, handlers[i].user_data);
}

static void
free_bundle (zf_buffer *b)
{
  producer_buffer *pb = (producer_buffer *) b;
  guint i;

  for (i = 0; i < pb->num_images; i++)
    zimage_unref (pb->images[i]);

  g_free (b);
}

static int
fill_bundle_tveng (producer_buffer *p, tveng_device_info *info)
{
  int r;

  if (p->src_image) {
    r = tveng_read_frame (&p->src_image->data, 50, info);
  } else { /* read & discard */
    r = tveng_read_frame (NULL, 50, info);
  }

  if (0 != r)
    return r;

  p->frame.timestamp = tveng_get_timestamp (info);

  CLEAR (p->converted);
  p->converted[p->src_index] = TRUE;

  return 0;
}

static tv_pixfmt_set
build_mask (gboolean allow_suggested)
{
  tv_pixfmt_set mask = 0;
  gint i;

  for (i = 0; i < num_formats; i++)
    if (allow_suggested || formats[i].required)
      mask |= TV_PIXFMT_SET (formats[i].fmt.pixfmt);

  return mask;
}

/* Check whether the given buffer can hold the current request */
static gboolean
compatible (producer_buffer *p, tveng_device_info *info)
{
  tv_pixfmt_set avail_mask = 0;
  const tv_image_format *fmt;
  gint retvalue;

  /* No images, it's always failure */
  if (!p->num_images)
    return FALSE;

  /* first check whether the size is right */
  fmt = tv_cur_capture_format (info);
  if (fmt->width != p->images[0]->fmt.width ||
      fmt->height != p->images[0]->fmt.height)
    {
      retvalue = FALSE;
    }
  else /* size is ok, check pixformat */
    {
      guint i;

      for (i = 0; i < p->num_images; i++)
	avail_mask |= TV_PIXFMT_SET (p->images[i]->fmt.pixfmt);

      retvalue = !!((avail_mask | build_mask (FALSE)) == avail_mask);
    }

  return retvalue;
}

static void *
capture_thread (void *data)
{
  tveng_device_info *info = (tveng_device_info*)data;
  zf_producer prod;

  zf_add_producer(capture_fifo, &prod);

  while (!exit_capture_thread)
    {
      producer_buffer *p =
	(producer_buffer*)zf_wait_empty_buffer(&prod);

      /*
	check whether this buffer needs rebuilding. We don't do it
	here but defer it to the main thread, since rebuilding buffers
	typically requires X calls, and those don't work across
	multiple threads (actually they do, but relaying on that is
	just asking for trouble, it's too complex to get right).
	Just seeing that the buffer is old doesn't mean that it's
	unusable, if compatible is TRUE then we fill it normally.
      */

      /* No size change now. */
      _pthread_rwlock_rdlock (&fmt_rwlock); 

retry:
      if (exit_capture_thread)
	{
	  _pthread_rwlock_unlock (&fmt_rwlock);
	  zf_unget_empty_buffer (&prod, &p->frame.b);
	  break;
	}

      if (p->tag != request_id && !compatible (p, info))
	{
	  /* schedule for rebuilding in the main thread */
	  p->frame.b.used = 1; /* used==0 indicates eof */
	  zf_send_full_buffer(&prod, &p->frame.b);
	  _pthread_rwlock_unlock (&fmt_rwlock);
	  continue;
	}

      /* We cannot handle timeouts or errors. Note timeouts
	 are frequent when capturing an empty */
      if (0 != fill_bundle_tveng(p, info))
	{
	  /* Avoid busy loop. FIXME there must be a better way. */
	  _pthread_rwlock_unlock (&fmt_rwlock);
	  usleep (10000);
	  _pthread_rwlock_rdlock (&fmt_rwlock);
	  goto retry;
	}

      _pthread_rwlock_unlock (&fmt_rwlock);

      /* FIXME something is wrong here with timestamps.
	 We start capturing, then get_timestamp() before
	 the first buffer dequeue. */
      if (p->src_image && p->frame.timestamp > 0)
	{
	  p->frame.b.time = p->frame.timestamp;

	  p->frame.b.data = p->src_image->data.linear.data;
	  p->frame.b.used = p->src_image->fmt.size;
	}
      else
	{
	  p->frame.b.data = NULL;
	  p->frame.b.used = 1;
	}

      zf_send_full_buffer(&prod, &p->frame.b);
    }

  zf_rem_producer(&prod);

  capture_quit_ack = TRUE;

  return NULL;
}

/*
  If the device has never been scanned for available modes before
  this routine checks for that. Otherwise the appropiate values are
  loaded from the configuration.
  Returns a bitmask indicating which modes are available.
*/
static tv_pixfmt_set
scan_device		(tveng_device_info	*info)
{
  /*  gchar *key;
  gchar *s;
  */
  tv_pixfmt pixfmt;
  tv_pixfmt_set supported;
  tv_image_format old_fmt;
  tv_image_format new_fmt;

  /*
  key = g_strdup_printf (ZCONF_DOMAIN "%x/scanned", info->signature);
  if (zconf_get_boolean (NULL, key))
    {
      g_free (key);
      key = g_strdup_printf (ZCONF_DOMAIN "%x/available_formats",
			     info->signature);
      zconf_get_int (&supported, key);
      // zconf_get_string (&s, key);
      g_free (key);
      // supported = atoll (s); !!
      // g_free (s);
      return supported;
    }
  */

  if (0 != tv_supported_pixfmts (info)) {
    /* Shortcut. */
    return tv_supported_pixfmts (info);
  }

  old_fmt = *tv_cur_capture_format (info);

  supported = 0;

  /* Things are simpler this way. */

  for (pixfmt = 0; pixfmt < TV_MAX_PIXFMTS; pixfmt++)
    if (TV_PIXFMT_SET_ALL & TV_PIXFMT_SET (pixfmt))
      {
	/* FIXME other code assumes info->capture_format is the
	   current format, not the one we request. */
	new_fmt = old_fmt;
	new_fmt.pixfmt = pixfmt;

	if (tv_set_capture_format (info, &new_fmt))
	  supported |= TV_PIXFMT_SET (pixfmt);
      }

  tv_set_capture_format (info, &old_fmt);

  /*
  zconf_create_boolean
    (TRUE, "Whether this device has been already scanned", key);

  g_free (key);
  key = g_strdup_printf (ZCONF_DOMAIN "%x/available_formats",
			 info->signature);
  zconf_create_int (supported, "Bitmaps of available pixformats", key);
  // s = g_strdup_printf ("%llu", supported);
  // zconf_create_string (s, "Bitmaps of available pixfmts", key);
  // g_free (s);
  g_free (key);
  */

  return supported;
}

/*
 * This is an special consumer. It could have been splitted into these
 * three different consumers, each one with a very specific job:
 * a) Rebuild:
 *	The producers and the main (GTK) loop are in different
 *	threads. Producers defer the job of rebuilding the bundles to
 *	this consumer.
 * b) Display:
 *	The consumer that blits the data into the tvscreen.
 * c) Plugins:
 *	Passes the data to the serial_read plugins.
 */

static zf_consumer		__ctc, *cf_timeout_consumer = &__ctc;
static guint			idle_id;
static gint idle_handler(gpointer _info)
{
  zf_buffer *b;
  struct timespec t;
  struct timeval now;
  producer_buffer *pb;
  tveng_device_info *info = (tveng_device_info*)_info;

  gettimeofday (&now, NULL);

  t.tv_sec = now.tv_sec;
  t.tv_nsec = (now.tv_usec + 50000)* 1000;

  if (t.tv_nsec >= 1e9)
    {
      t.tv_nsec -= 1e9;
      t.tv_sec ++;
    }

  b = zf_wait_full_buffer_timeout (cf_timeout_consumer, &t);
  if (!b)
    return TRUE; /* keep calling me */

  pb = (producer_buffer*)b;

  if (pb->tag == request_id)
    {
      capture_frame *cf = (capture_frame*)pb;
      GList *p = plugin_list;

      /* Draw the image */
      video_blit_frame (cf);
      
      /* Pass the data to the plugins */
      while (p)
	{
	  plugin_read_frame (cf, (struct plugin_info*)p->data);
	  p = p->next;
	}
    }
  else /* Rebuild time */
    {
      tv_pixfmt_set mask = build_mask (TRUE);
      unsigned int i;

      /* Remove items in the current array */
      for (i = 0; i < pb->num_images; i++)
	zimage_unref (pb->images[i]);

      pb->src_image = NULL;
      pb->num_images = 0;

      /* Get number of requested modes */
      for (i = 0; i < TV_MAX_PIXFMTS; i++)
	{
	  const tv_image_format *fmt;

	  fmt = tv_cur_capture_format (info);

	if (TV_PIXFMT_SET (i) & (mask | TV_PIXFMT_SET (fmt->pixfmt)))
	  {
	    pb->images[pb->num_images] =
	      zimage_new (i, fmt->width, fmt->height);

	    if (fmt->pixfmt == i)
	      {
		/* XXX is this correct? */
		pb->src_index = pb->num_images;
		pb->src_image = pb->images[pb->num_images];
	      }

	    pb->num_images++;
	  }
	}

      g_assert (pb->src_image != NULL);
      pb->tag = request_id; /* Done */
    }

  zf_send_empty_buffer (cf_timeout_consumer, b);

  return TRUE; /* keep calling me */
}

static void
on_capture_canvas_allocate             (GtkWidget       *widget _unused_,
                                        GtkAllocation   *allocation,
                                        tveng_device_info *info)
{
  capture_fmt fmt;

  CLEAR (fmt);

  fmt.pixfmt = tv_cur_capture_format (info)->pixfmt;
  fmt.width = allocation->width;
  fmt.height = allocation->height;

  request_capture_format (&fmt);
}

gint capture_start (tveng_device_info *info, GtkWidget *window)
{
  int i;
  zf_buffer *b;

  if (tveng_start_capturing (info) == -1)
    {
      ShowBox (_("Cannot start capturing: %s"),
	       GTK_MESSAGE_ERROR, tv_get_errstr (info));
      return FALSE;
    }

  /* XXX */
  dest_window = window;

  zf_init_buffered_fifo (capture_fifo, "zapping-capture", 0, 0);

  for (i=0; i<NUM_BUNDLES; i++)
    {
      g_assert ((b = g_malloc0(sizeof(producer_buffer))));
      b->destroy = free_bundle;
      zf_add_buffer (capture_fifo, b);
    }

  zf_add_consumer (capture_fifo, cf_timeout_consumer);

  exit_capture_thread = FALSE;
  capture_quit_ack = FALSE;
  g_assert (!pthread_create (&capture_thread_id, NULL, capture_thread,
			     zapping->info));

  available_pixfmts = scan_device (zapping->info);

  idle_id = g_idle_add ((GSourceFunc) idle_handler, info);

  /* XXX */
  g_signal_connect (G_OBJECT (window),
		    "size-allocate",
		    GTK_SIGNAL_FUNC (on_capture_canvas_allocate),
		    zapping->info);

  return TRUE;
}

static gint
join (const char *who, pthread_t id, volatile gboolean *ack, gint timeout)
{
  /* Dirty. Where is pthread_try_join()? */
  for (; (!*ack) && timeout > 0; timeout--) {
    usleep (100000);
  }

  /* Ok, you asked for it */
  if (timeout == 0) {
    int r;

    printv("Unfriendly video capture termination\n");
    r = pthread_cancel (id);
    if (r != 0)
      {
	printv("Cancellation of %s failed: %d\n", who, r);
	return 0;
      }
  }

  pthread_join (id, NULL);

  return timeout;
}

void capture_stop (void)
{
  GList *p;
  zf_buffer *b;

  /* XXX */
  g_signal_handlers_disconnect_by_func
    (G_OBJECT (dest_window),
     GTK_SIGNAL_FUNC (on_capture_canvas_allocate),
     zapping->info);

  dest_window = NULL;

  /* First tell all well-behaved consumers to stop */
  broadcast (CAPTURE_STOP);
  p = g_list_first(plugin_list);
  while (p)
    {
      plugin_capture_stop((struct plugin_info*)p->data);
      p = p->next;
    }  

  /* Stop our marvellous consumer */
  if (NO_SOURCE_ID != idle_id)
    g_source_remove (idle_id);
  idle_id = NO_SOURCE_ID;

  /* Let the capture thread go to a better place */
  exit_capture_thread = TRUE;

  /* empty full queue and remove timeout consumer */
  while ((b = zf_recv_full_buffer (cf_timeout_consumer)))
    zf_send_empty_buffer (cf_timeout_consumer, b);
  zf_rem_consumer (cf_timeout_consumer);

  join ("videocap", capture_thread_id, &capture_quit_ack, 15);

  /* Free handlers and formats */
  g_free (handlers);
  handlers = NULL;
  num_handlers = 0;

  g_free (formats);
  formats = NULL;
  num_formats = 0;

  zf_destroy_fifo (capture_fifo);
}

/*
  Finds the first request in (formats, fmt) that gives a size, and
  stores it in width, height. If nothing gives a size, then uses some
  defaults.
*/
static gboolean
find_request_size (capture_fmt *fmt, gint *width, gint *height)
{
  gint i;
  for (i=0; i<num_formats; i++)
    if (formats[i].fmt.locked)
      {
	/* We cannot change the capture size, caller
	   must take this or scale.

	   FIXME one should only scale down, and
	   don't forget about interlaced video. */

	*width = formats[i].fmt.width;
	*height = formats[i].fmt.height;
	return TRUE;
      }

  if (fmt && fmt->locked)
    {
      /* We can change, must use requested size
	 instead of current. */

      *width = fmt->width;
      *height = fmt->height;
      return TRUE;
    }

  /* Any size will do. */

  /* FIXME this should query tveng or use
     some user configured values. */
  if (dest_window)
    {
      *width = SATURATE (dest_window->allocation.width, 64, 768);
      *height = SATURATE (dest_window->allocation.height, 64 * 3/4, 576);
    }
  else
    {
      *width = 352;
      *height = 288;
    }

  return FALSE;
}

/*
 * The following routine tries to find the optimum tveng mode for the
 * current requested formats. Optionally, we can also pass an extra
 * capture_fmt, it will be treated just like it was a member of formats.
 * Upon success, the request is stored
 * and its id is returned. This routine isn't a complete algorithm, in
 * the sense that we don't try all possible combinations of
 * suggested/required modes when allow_suggested == FALSE. In fact,
 * since we will only have one suggested format it provides the
 * correct answer.
 * This routine could suffer some additional optimization, but
 * obfuscating it even more isn't a good thing.
 */
static gint
request_capture_format_real (capture_fmt *fmt, gboolean required,
			     gboolean allow_suggested,
			     tveng_device_info *info)
{
  tv_pixfmt_set prev_mask;
  tv_pixfmt_set req_mask;
  gint i;
  tv_pixfmt id=0;
  gint k;
  static gint counter = 0;
  gint conversions = 999;
  tv_image_format prev_fmt;
  const tv_image_format *ifmt;
  guint req_w, req_h;
  gboolean fixed_size;

  req_mask = fmt ? TV_PIXFMT_SET (fmt->pixfmt) : TV_PIXFMT_SET_EMPTY;

  if (fmt)
    _pthread_rwlock_wrlock (&fmt_rwlock);
  else
    _pthread_rwlock_rdlock (&fmt_rwlock);

  /* Previously requested formats */
  prev_mask = build_mask (allow_suggested);

  /* First do the easy check, whether the req size is available */
  /* Note that we assume that formats is valid [should be, since it's
     constructed by adding/removing] */
  if (fmt && fmt->locked)
    for (i=0; i<num_formats; i++)
      if (formats[i].fmt.locked &&
	  ((!formats[i].required) && allow_suggested) &&
	  (formats[i].fmt.width != fmt->width ||
	   formats[i].fmt.height != fmt->height))
	{
	  _pthread_rwlock_unlock (&fmt_rwlock);
	  return -1;
	}

  /* Check if we already convert to the requested pixfmt. */
  if (prev_mask & req_mask)
    {
      /* Keep the current capture pixfmt. */
      id = tv_cur_capture_format (info)->pixfmt;
      goto req_ok;
    }

  /* This searches for a capture pixfmt which can be converted to
     all requested pixfmts. If more than one qualifies, we pick
     that which needs the fewest conversions.

     FIXME this is too simple. See TODO. */

  for (i=0; i<TV_MAX_PIXFMTS; i++)
    if (available_pixfmts & TV_PIXFMT_SET (i))
      {
	gint num_conversions = 0;

	/* See what would happen if we set mode i, which is supported
	   by the video hw */

	for (k=0; k < TV_MAX_PIXFMTS; k++)
	  if ((req_mask | prev_mask) & TV_PIXFMT_SET (k))
	    {
	      /* Mode i supported, k requested */
	      if (i != k)
		{
		  if (-1 != lookup_csconvert (i, k))
		    {
		      /* Available through a cs conversion */
		      num_conversions++;
		    }
		  else
		    {
		      /* For simplicity we don't allow converting a
			 converted image. Ain't that difficult to support,
			 but i think it's better just to fail than doing too
			 many cs conversions. */
		      /* Cannot convert to k, so i is out of choice. */
		      num_conversions = 999; /* like infinite */
		      break;
		    }
		}
	    }

	/* Candidate found, record it if it requires less conversions
	   than our last candidate */
	if (num_conversions < conversions)
	  {
	    conversions = num_conversions;
	    id = i;
	  }
      }

  /* Combo not possible, try disabling the suggested modes [all of
     them, otherwise it's a bit too complex] */
  if (conversions == 999)
    {
      if (!allow_suggested)
	{
	  _pthread_rwlock_unlock (&fmt_rwlock);
	  return -1; /* No way, we were already checking without
			suggested modes */
	}
      _pthread_rwlock_unlock (&fmt_rwlock);
      return request_capture_format_real (fmt, required, FALSE, info);
    }

 req_ok:
  /* Request the new format to TVeng (should succeed) [id] */
  prev_fmt = *tv_cur_capture_format (info);

  fixed_size = find_request_size (fmt, &req_w, &req_h);

  ifmt = tv_cur_capture_format (info);
  if (ifmt->pixfmt != id
      || ifmt->width != req_w
      || ifmt->height != req_h)
    {
      tv_image_format new_fmt;

      new_fmt = *ifmt;
      new_fmt.pixfmt = id;
      new_fmt.width = req_w;
      new_fmt.height = req_h;

      printv ("Setting TVeng mode %s [%d x %d]\n",
	      tv_pixfmt_name (id), req_w, req_h);

      tv_clear_error (info);

      if (!(ifmt = tv_set_capture_format (info, &new_fmt))
	  || (fixed_size &&
	      /* may change due to driver limits */
	      (ifmt->width != req_w
	       || ifmt->height != req_h)))
	{
	  if (0)
	    g_warning ("Cannot set new mode: %s", tv_get_errstr (info));
	  /* Try to restore previous setup so we can keep working */
	  new_fmt = prev_fmt;

	  if (!(ifmt = tv_set_capture_format (info, &new_fmt)))
	    if (tv_get_capture_mode (info) == CAPTURE_MODE_NONE
		|| tv_get_capture_mode (info) == CAPTURE_MODE_TELETEXT)
	      tveng_start_capturing (info);
	  _pthread_rwlock_unlock (&fmt_rwlock);
	  return -1;
	}
    }

  /* Flags that the request list *might* have changed */
  request_id ++;
  broadcast (CAPTURE_CHANGE);

  if (fmt)
    {
      formats = g_realloc (formats, sizeof(*formats)*(num_formats+1));
      formats[num_formats].id = counter++;
      formats[num_formats].required = required;
      memcpy (&formats[num_formats].fmt, fmt, sizeof(*fmt));
      num_formats++;
      _pthread_rwlock_unlock (&fmt_rwlock);
      printv ("Format %s accepted [%s]\n",
	      tv_pixfmt_name (fmt->pixfmt),
	      tv_pixfmt_name (tv_cur_capture_format (info)->pixfmt));
      /* Safe because we only modify in one thread */
      return formats[num_formats-1].id;
    }

  _pthread_rwlock_unlock (&fmt_rwlock);
  return 0;
}

/* Hack to improve format request for recording,
   see plugins/mpeg/mpeg.c. */
tv_pixfmt
native_capture_format		(void)
{
  if (0 == num_formats)
    return TV_PIXFMT_UNKNOWN;
  else
    return formats[0].fmt.pixfmt;
}

gint request_capture_format (capture_fmt *fmt)
{
  g_assert (fmt != NULL);

  printv ("%s requested\n", tv_pixfmt_name (fmt->pixfmt));

  return request_capture_format_real (fmt, TRUE, TRUE, zapping->info);
}

gint suggest_capture_format (capture_fmt *fmt)
{
  g_assert (fmt != NULL);

  printv ("%s suggested\n", tv_pixfmt_name (fmt->pixfmt));

  return request_capture_format_real (fmt, FALSE, TRUE, zapping->info);
}

void release_capture_format (gint id)
{
  gint index;

  _pthread_rwlock_wrlock (&fmt_rwlock);

  for (index = 0; index < num_formats; ++index) {
    if (formats[index].id == id)
      break;
  }

  g_assert (index != num_formats);

  num_formats--;

  if (index != num_formats)
    memmove (&formats[index], &formats[index+1],
	    (num_formats - index) * sizeof (formats[0]));

  _pthread_rwlock_unlock (&fmt_rwlock);

  request_capture_format_real (NULL, FALSE, TRUE, zapping->info);
}

void
get_request_formats (capture_fmt **fmt, gint *num_fmts)
{
  gint i;

  _pthread_rwlock_rdlock (&fmt_rwlock);
  *fmt = g_malloc0 (num_formats * sizeof (capture_fmt));

  for (i=0; i<num_formats; i++)
    memcpy (&fmt[i], &formats[i].fmt, sizeof (capture_fmt));

  *num_fmts = num_formats;
  _pthread_rwlock_unlock (&fmt_rwlock);
}

gint
add_capture_handler (CaptureNotify notify, gpointer user_data)
{
  static int id = 0;

  handlers = g_realloc (handlers,
			sizeof (*handlers) * num_handlers);
  handlers[num_handlers].notify = notify;
  handlers[num_handlers].user_data = user_data;
  handlers[num_handlers].id = id++;
  num_handlers ++;

  return handlers[num_handlers-1].id;
}

void
remove_capture_handler (gint id)
{
  gint i;

  for (i=0; i<num_handlers; i++)
    if (handlers[i].id == id)
      break;

  g_assert (i != num_handlers);

  num_handlers--;
  if (i != num_handlers)
    memcpy (&handlers[i], &handlers[i+1],
	    (num_handlers-i)*sizeof(*handlers));
}

zimage*
retrieve_frame (capture_frame *frame,
		tv_pixfmt pixfmt)
{
  guint i;
  producer_buffer *pb = (producer_buffer*)frame;

  for (i=0; i<pb->num_images; i++)
    if (pb->images[i]->fmt.pixfmt == pixfmt)
      {
	if (!pb->converted[i])
	  {
	    zimage *src = pb->src_image;
	    zimage *dest = pb->images[i];
	    int id = lookup_csconvert (src->fmt.pixfmt, pixfmt);

	    if (id == -1)
	      return NULL;

	    csconvert (id, &src->data, &dest->data,
		       src->fmt.width,
		       src->fmt.height);

	    pb->converted[i] = TRUE;
	  }

	return pb->images[i];
      }

  return NULL;
}

void
startup_capture (void)
{
  pthread_rwlock_init (&fmt_rwlock, NULL);
}

void
shutdown_capture (void)
{
  pthread_rwlock_destroy (&fmt_rwlock);
}
