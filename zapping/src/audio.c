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

/* $Id: audio.c,v 1.36 2007-08-30 14:14:33 mschimek Exp $ */

/* XXX gtk+ 2.3 GtkOptionMenu */
#undef GTK_DISABLE_DEPRECATED

#include "site_def.h"

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gnome.h>

#include "audio.h"
#define ZCONF_DOMAIN "/zapping/options/audio/"
#include "zconf.h"
#include "properties.h"
#include "interface.h"
#include "zmisc.h"
#include "mixer.h"
#include "osd.h"
#include "remote.h"
#include "globals.h"
#include "v4linterface.h"
#include "tveng_private.h"
#include "common/device.h"

#ifdef HAVE_ESD
extern audio_backend_info esd_backend;
#endif
#ifdef HAVE_ARTS
extern audio_backend_info arts_backend;
#endif
#ifdef HAVE_OSS
extern audio_backend_info oss_backend;
#endif

struct _mhandle {
  gpointer		handle; /* for the backend */
  audio_backend_info *	backend;
};

void mixer_setup ( void )
{
  /* FIXME */
  if (mixer && mixer_line)
    {
      if (mixer->rec_gain)
	tv_mixer_line_set_volume (mixer->rec_gain,
				  mixer->rec_gain->reset,
				  mixer->rec_gain->reset);

      if (mixer_line->recordable)
	{
	  tv_mixer_line_record (mixer_line, /* exclusive */ TRUE);

	  if (esd_output || ivtv_audio)
	    tv_mixer_line_set_mute (mixer_line, TRUE);
	}
    }
}

mhandle *
open_audio_device (gboolean stereo, guint rate, enum audio_format
		   format)
{
  const gchar *audio_source;
  audio_backend_info *backend;
  gpointer handle;
  mhandle *mh;

  audio_source = zcg_char (NULL, "audio_source");
  if (!audio_source)
    audio_source = "";

  backend = NULL;

  if (0)
    backend = NULL;
#ifdef HAVE_ESD
  else if (0 == strcmp (audio_source, "esd"))
    backend = &esd_backend;
#endif
#ifdef HAVE_ARTS
  else if (0 == strcmp (audio_source, "arts"))
    backend = &arts_backend;
#endif
#ifdef HAVE_OSS
  else if (0 == strcmp (audio_source, "kernel"))
    backend = &oss_backend;
#endif

  handle = NULL;

  if (backend)
    handle = backend->open (stereo, rate, format, /* write */ FALSE);

  if (!handle)
    {
      ShowBox(("Cannot open the configured audio device.\n"
		"You might want to setup another kind of audio\n"
		"device in the Properties dialog."),
	      GTK_MESSAGE_WARNING);
      return NULL;
    }

  mh = g_malloc0 (sizeof (*mh));
  mh->handle = handle;
  mh->backend = backend;

  /* Make sure we record from the appropiate source. */
  mixer_setup();

  return mh;
}

typedef struct {
  int		fd;
} ivtv_handle;

static gpointer
ivtv_open			(gboolean		stereo,
				 guint			sampling_rate,
				 enum audio_format	format,
				 gboolean		write)
{
  int fd;
  ivtv_handle *h;

  if (!stereo
      || 48000 != sampling_rate
      || format != AUDIO_FORMAT_S16_LE
      || write)
    {
      g_warning ("Requested audio format won't work with ivtv driver\n");
      return NULL;
    }

  fd = device_open ((io_debug_msg > 0) ? stderr : NULL,
		    "/dev/video24", O_RDONLY, /* mode */ 0);
  if (-1 == fd)
    return NULL;

  h = (ivtv_handle *) g_malloc0 (sizeof(*h));
  h->fd = fd;

  return h;
}

static void
ivtv_close (gpointer handle)
{
  ivtv_handle *h = (ivtv_handle*)handle;

  device_close ((io_debug_msg > 0) ? stderr : NULL,
		h->fd);
  
  g_free(h);
}

static gboolean
ivtv_read (gpointer handle, gpointer dest, guint num_bytes,
       double *timestamp)
{
  ivtv_handle *h = (ivtv_handle *) handle;
  ssize_t r, n = num_bytes;
  char *data = (char *) dest;
  struct timeval tv1;

  while (n > 0)
    {
      r = read(h->fd, data, (size_t) n);
		
      if (r == 0 || (r < 0 && errno == EINTR))
	continue;

      /* FIXME handle this properly. */
      if (r <= 0)
	{
	  fprintf (stderr, "IVTV read failed, r=%d errno=%d\n",
		   r, errno);
	  exit (EXIT_FAILURE);
	}

      data = (char *) data + r;
      n -= r;
    }

  if (timestamp)
    {
      gettimeofday(&tv1, NULL);
      *timestamp = tv1.tv_sec + tv1.tv_usec * (1 / 1e6);
    }

  return TRUE;
}

/* One awful hack to test if it works. */
static audio_backend_info
ivtv_backend = {
  name:		"IVTV Driver",
  open:		ivtv_open,
  close:	ivtv_close,
  read:		ivtv_read,
};

static mhandle *
open_ivtv_input			(void)
{
  audio_backend_info *backend;
  gpointer handle;
  mhandle *mh;

  backend = &ivtv_backend;
  handle = backend->open (/* stereo */ TRUE,
			  /* rate */ 48000,
			  AUDIO_FORMAT_S16_LE,
			  /* write */ FALSE);
  if (!handle)
    {
      ShowBox(("Cannot open ivtv audio device: %s.\n"),
	      GTK_MESSAGE_WARNING, strerror (errno));
      return NULL;
    }

  mh = g_malloc0 (sizeof (*mh));
  mh->handle = handle;
  mh->backend = backend;

  return mh;
}

static mhandle *
open_esd_output (gboolean stereo, guint rate, enum audio_format
		 format)
{
  audio_backend_info *backend;
  gpointer handle;
  mhandle *mh;

  backend = NULL;
#ifdef HAVE_ESD
  backend = &esd_backend;
#endif

  handle = NULL;

  if (backend)
    handle = backend->open (stereo, rate, format, /* write */ TRUE);

  if (!handle)
    {
      ShowBox(("Cannot open the Gnome sound daemon (ESD) for output.\n"),
	      GTK_MESSAGE_WARNING);
      return NULL;
    }

  mh = g_malloc0 (sizeof (*mh));
  mh->handle = handle;
  mh->backend = backend;

  return mh;
}

void
close_audio_device (mhandle *mh)
{
  g_assert (NULL != mh);

  mh->backend->close (mh->handle);

  g_free (mh);
}

gboolean
read_audio_data (mhandle *mh, gpointer dest, guint num_bytes,
		 double *timestamp)
{
  g_assert (NULL != mh);

  return mh->backend->read (mh->handle, dest, num_bytes, timestamp);
}

gboolean
write_audio_data (mhandle *mh, gpointer src, guint num_bytes,
		  double timestamp)
{
  g_assert (NULL != mh);

  return mh->backend->write (mh->handle, src, num_bytes, timestamp);
}

/*
 *  Audio loop-back thread
 */

static pthread_t		loopback_id;
static gboolean			have_loopback_thread;
static volatile gboolean	loopback_quit;
static volatile gboolean	loopback_quit_ack;
static mhandle *		loopback_src;
static mhandle *		loopback_dst;

tv_audio_line
audio_loopback_mixer_line = {
	._parent	= (void *) &audio_loopback_mixer,
	.label		= "Audio Loopback",
	.hash		= 38558,
	.minimum	= 0,
	.maximum	= 256,
	.step		= 1,
	.reset		= 204,
	.stereo		= TRUE,
	.muted		= TRUE,
	.volume		= { 204, 204 }
};

static tv_bool
loopback_mixer_set_mute		(tv_audio_line *	l,
				 tv_bool		mute)
{
	assert (l == &audio_loopback_mixer_line);

	if (l->muted != mute) {
		l->muted = mute;
		tv_callback_notify (NULL, l, l->_callback);
	}

	return TRUE;
}

static tv_bool
loopback_mixer_set_volume	(tv_audio_line *	l,
				 int			left,
				 int			right)
{
	assert (l == &audio_loopback_mixer_line);

	left = SATURATE (left, 0, 256);
	right = SATURATE (right, 0, 256);

	if (l->volume[0] != left
	    || l->volume[1] != right) {
		l->volume[0] = left;
		l->volume[1] = right;
		tv_callback_notify (NULL, l, l->_callback);
	}

	return TRUE;
}

static tv_bool
loopback_mixer_update_line	(tv_audio_line *	l)
{
	assert (l == &audio_loopback_mixer_line);

	l = l;

	return TRUE;
}

static const tv_mixer_interface
audio_loopback_mixer_interface = {
	.name 		= "Audio Loopback",
	.update_line	= loopback_mixer_update_line,
	.set_volume	= loopback_mixer_set_volume,
	.set_mute	= loopback_mixer_set_mute,
};

tv_mixer
audio_loopback_mixer = {
	.inputs		= &audio_loopback_mixer_line,
	._interface	= &audio_loopback_mixer_interface,
};

static void *
loopback_thread			(void *			x)
{
  uint8_t *buffer;
  uint8_t *buffer_end;
  guint buffer_size;

  x = x;

  printv ("audio loopback thread\n");

  buffer_size = 4096; /* 256 */

  buffer = malloc (buffer_size);
  g_assert (NULL != buffer);

  buffer_end = buffer + buffer_size;

  while (!loopback_quit) 
    {
      double timestamp;

      if (!read_audio_data (loopback_src, buffer, buffer_size, &timestamp))
	break;

      if (io_debug_msg)
	{
	  fprintf (stderr, "audio loopback: %02x%02x%02x%02x\n",
		   buffer[0], buffer[1],
		   buffer[2], buffer[3]);
	}

      if (audio_loopback_mixer_line.muted)
	{
	  /* XXX inefficient */
	  memset (buffer, 0, buffer_size);
	}
      else
	{
	  /* Can't just change rec_gain because that may not exist and
	     we don't want to affect recording (in the future). XXX SIMD me. */
#if Z_BYTE_ORDER == Z_LITTLE_ENDIAN
	  {
	    int16_t *p;

	    for (p = (int16_t *) buffer; p < (int16_t *) buffer_end; p += 2)
	      {
		p[0] = (p[0] * audio_loopback_mixer_line.volume[0]) >> 8;
		p[1] = (p[1] * audio_loopback_mixer_line.volume[1]) >> 8;
	      }
	  }
#elif Z_BYTE_ORDER == Z_BIG_ENDIAN
	  {
	    uint8_t *p;

	    for (p = buffer; p < buffer_end; p += 4)
	      {
		gint n;

		n = ((p[0] + p[1] * 256)
		     * audio_loopback_mixer_line.volume[0]) >> 8;
		p[0] = n;
		p[1] = n >> 8;

		n = ((p[2] + p[3] * 256)
		     * audio_loopback_mixer_line.volume[1]) >> 8;
		p[2] = n;
		p[3] = n >> 8;
	      }
	  }
#else
#  error unknown endianess
#endif
	}

#if 0 /* test */
      {
	static int wfd = -1;

#include <sys/ioctl.h>
#include <sys/soundcard.h>

	if (-1 == wfd) {
	  int Format = AFMT_S16_LE;
	  int Stereo = 1;
	  int Speed = 44100;
	  int arg = 0x7FFF0008; /* 0xMMMMSSSS */

	  fprintf (stderr, "DSPOUT TEST\n");
	  wfd = open ("/dev/dsp", O_WRONLY, 0);
	  ioctl(wfd, SNDCTL_DSP_SETFMT, &Format);
	  ioctl(wfd, SNDCTL_DSP_STEREO, &Stereo);
	  ioctl(wfd, SNDCTL_DSP_SPEED, &Speed);
	  ioctl(wfd, SNDCTL_DSP_SETFRAGMENT, &arg);
	}

	write (wfd, buffer, buffer_size);
      }
#else
      if (!write_audio_data (loopback_dst, buffer, buffer_size, timestamp))
	break;
#endif
    }

  free (buffer);
  buffer = NULL;

  printv ("audio loopback thread quits\n");

  loopback_quit_ack = TRUE;

  return NULL;
}

static void
loopback_stop			(void)
{
  printv ("stopping audio loopback\n");

  if (have_loopback_thread)
    {
      z_join_thread_with_timeout ("audio loopback",
				  loopback_id,
				  &loopback_quit,
				  &loopback_quit_ack,
				  /* timeout */ 15);

      have_loopback_thread = FALSE;
    }

  if (loopback_dst)
    {
      close_audio_device (loopback_dst);
      loopback_dst = NULL;
    }

  if (loopback_src)
    {
      close_audio_device (loopback_src);
      loopback_src = NULL;
    }
}

static gboolean
loopback_start			(void)
{
  gboolean stereo = TRUE;
  gboolean rate = 44100;
  gboolean format = AUDIO_FORMAT_S16_LE;

  if (have_loopback_thread)
    loopback_stop ();

  printv ("starting %saudio loopback\n", ivtv_audio ? "ivtv " : "");

  if (ivtv_audio)
    {
      rate = 48000;
      loopback_src = open_ivtv_input ();
    }
  else
    {
      loopback_src = open_audio_device (stereo, rate, format);
    }

  if (NULL == loopback_src)
    return FALSE;

  loopback_dst = open_esd_output (stereo, rate, format);
  if (NULL == loopback_dst)
    {
      close_audio_device (loopback_src);
      loopback_src = NULL;
      return FALSE;
    }

  loopback_quit = FALSE;
  loopback_quit_ack = FALSE;

  if (pthread_create (&loopback_id,
		      /* attr */ NULL,
		      loopback_thread,
		      /* arg */ NULL))
    {
      close_audio_device (loopback_dst);
      loopback_dst = NULL;

      close_audio_device (loopback_src);
      loopback_src = NULL;

      return FALSE;
    }

  have_loopback_thread = TRUE;

  printv ("audio loopback started\n");

  return TRUE;
}

/*
 *  Audio preferences
 */

#ifndef AUDIO_MIXER_LOG_FP
#define AUDIO_MIXER_LOG_FP 0 /* stderr */
#endif

static void
general_audio_apply		(GtkWidget *		page)
{
  GtkWidget *widget;
  gboolean active;

  widget = lookup_widget (page, "general-audio-start-muted");
  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  zconf_set_boolean (active, "/zapping/options/main/start_muted");

  widget = lookup_widget (page, "general-audio-quit-muted");
  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  zconf_set_boolean (active, "/zapping/options/main/quit_muted");

  widget = lookup_widget (page, "general-audio-mute-chsw");
  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  zconf_set_boolean (active, "/zapping/options/main/avoid_noise");
}

static void
general_audio_setup		(GtkWidget *		page)
{
  GtkWidget *widget;
  gboolean active;

  zconf_get_boolean (&active, "/zapping/options/main/start_muted");
  widget = lookup_widget (page, "general-audio-start-muted");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), active);

  zconf_get_boolean (&active, "/zapping/options/main/quit_muted");
  widget = lookup_widget (page, "general-audio-quit-muted");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), active);

  zconf_get_boolean (&active, "/zapping/options/main/avoid_noise");
  widget = lookup_widget (page, "general-audio-mute-chsw");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), active);
}

static void
devices_audio_source_apply	(GtkWidget *		page)
{
  GtkWidget *widget;
  const gchar *audio_source;
  tv_device_node *n;

  audio_source = "none";

  widget = lookup_widget (page, "devices-audio-gnome");
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    {
      audio_source = "esd";
    }
  else
    {
#ifdef HAVE_ARTS
      widget = lookup_widget (page, "devices-audio-kde");
      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
	{
	  audio_source = "arts";
	}
      else
#endif
	{
#ifdef HAVE_OSS
	  widget = lookup_widget (page, "devices-audio-kernel");
	  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
	    audio_source = "kernel";
#endif
	}
    }

  zcs_char (audio_source, "audio_source");

  widget = lookup_widget (page, "devices-audio-kernel-table");
  if ((n = z_device_entry_selected (widget)))
    zcs_char (n->device, "pcm_device");
}

static void
devices_audio_mixer_apply	(GtkWidget *		page)
{
  GtkWidget *widget;
  gboolean active;
  tv_device_node *n;

  widget = lookup_widget (page, "devices-audio-mixer");
  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  zconf_set_boolean (active, "/zapping/options/audio/force_mixer");

  widget = lookup_widget (page, "devices-audio-mixer-table");

  if ((n = z_device_entry_selected (widget)))
    {
      tv_audio_line *l;
      gint index;

      zcs_char (n->device, "mixer_device");

      widget = g_object_get_data (G_OBJECT (widget), "mixer_lines");

      if (widget)
	{
	  index = z_option_menu_get_active (widget);

	  for (l = PARENT (n, tv_mixer, node)->inputs; l; l = l->_next)
	    if (0 == index--)
	      {
		zcs_int ((int) l->hash, "mixer_input");
		break;
	      }
	}
    }
}

static void
devices_audio_apply		(GtkWidget *		page)
{
  tveng_tc_control *tcc;
  guint num_controls;

  loopback_stop ();

  devices_audio_source_apply (page);
  devices_audio_mixer_apply (page);

  store_control_values (zapping->info, &tcc, &num_controls);

  startup_mixer (zapping->info);

  update_control_box (zapping->info);

  load_control_values (zapping->info, tcc, num_controls);

  if (esd_output || ivtv_audio)
    loopback_start ();
}

static void
on_enable_device_entry_toggled	(GtkToggleButton *	toggle_button,
				 GtkWidget *		widget)
{
  gboolean active;

  active = gtk_toggle_button_get_active (toggle_button);

  gtk_widget_set_sensitive (widget, active);

  if (active)
    z_device_entry_grab_focus (widget);
}

static tv_device_node *
devices_audio_kernel_open	(GtkWidget *		table _unused_,
				 tv_device_node *	list,
				 const char *		name,
				 gpointer		user_data _unused_)
{
  tv_device_node *n;

  if ((n = tv_device_node_find (list, name)))
    return n;

  /* FIXME report errors */
  if ((n = oss_pcm_open (NULL,
			 /* log_fp */ (io_debug_msg > 0) ? stderr : NULL,
			 name)))
    return n;

  return NULL;
}

static void
devices_audio_source_setup	(GtkWidget *		page)
{
  GtkWidget *alignment;
  GtkWidget *table;
  GtkWidget *widget;
  tv_device_node *list;
  const gchar *audio_source;

  audio_source = zcg_char (NULL, "audio_source");
  if (!audio_source)
    audio_source = "";

  widget = lookup_widget (page, "devices-audio-none");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);

  widget = lookup_widget (page, "devices-audio-gnome");
  if (0 == strcmp (audio_source, "esd"))
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);

  widget = lookup_widget (page, "devices-audio-kde");
#ifdef HAVE_ARTS
  if (0 == strcmp (audio_source, "arts"))
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
#else
  gtk_widget_set_sensitive (widget, FALSE);
#endif

  widget = lookup_widget (page, "devices-audio-kernel");
#ifdef HAVE_OSS
  if (0 == strcmp (audio_source, "kernel"))
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);

  /* XXX "oss_"? */
  list = oss_pcm_scan (NULL, NULL);

  alignment = lookup_widget (page, "devices-audio-kernel-alignment");
  
  table = z_device_entry_new (/* prompt */ NULL,
			      list,
			      zcg_char (NULL, "pcm_device"),
			      devices_audio_kernel_open, NULL,
			      /* user_data */ NULL);
  gtk_widget_show (table);
  gtk_container_add (GTK_CONTAINER (alignment), table);
  register_widget (NULL, table, "devices-audio-kernel-table");

  g_signal_connect (G_OBJECT (widget), "toggled",
		    G_CALLBACK (on_enable_device_entry_toggled), table);
  on_enable_device_entry_toggled (GTK_TOGGLE_BUTTON (widget), table);
#else /* !HAVE_OSS */
  gtk_widget_set_sensitive (widget, FALSE);
#endif
}

static void
devices_audio_mixer_select	(GtkWidget *		table,
				 tv_device_node *	n,
				 gpointer		user_data _unused_)
{
  GtkWidget *menu;
  GtkWidget *optionmenu;

  if ((optionmenu = g_object_get_data (G_OBJECT (table), "mixer_lines")))
    {
      gtk_widget_destroy (optionmenu);
      optionmenu = NULL;
    }

  if (n)
    {
      tv_mixer *mixer;
      tv_audio_line *line;
      guint hash = (guint) -1;
      guint index;

      mixer = PARENT (n, tv_mixer, node);
      g_assert (mixer->inputs != NULL);

      if (0 == strcmp (zcg_char (NULL, "mixer_device"), n->device))
	hash = zcg_int (NULL, "mixer_input");

      optionmenu = gtk_option_menu_new ();
      gtk_widget_show (optionmenu);

      menu = gtk_menu_new ();
      gtk_widget_show (menu);
      gtk_option_menu_set_menu (GTK_OPTION_MENU (optionmenu), menu);
  
      index = 0;

      for (line = mixer->inputs; line; line = line->_next)
	{
	  GtkWidget *menu_item;
	  
	  menu_item = gtk_menu_item_new_with_label (line->label);
	  gtk_widget_show (menu_item);
	  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

	  if (index == 0 || line->hash == hash)
	    gtk_option_menu_set_history (GTK_OPTION_MENU (optionmenu), index);

	  index++;
	}

      if (mixer->inputs->_next == NULL)
	gtk_widget_set_sensitive (GTK_WIDGET (optionmenu), FALSE);

      gtk_table_attach (GTK_TABLE (table), optionmenu, 1, 1 + 1, 3, 3 + 1,
			GTK_FILL | GTK_EXPAND, 0, 0, 0);
    }

  g_object_set_data (G_OBJECT (table), "mixer_lines", optionmenu);
}

static tv_device_node *
devices_audio_mixer_open	(GtkWidget *		table _unused_,
				 tv_device_node *	list,
				 const char *		name,
				 gpointer		user_data _unused_)
{
  tv_mixer *m;
  tv_device_node *n;

  if ((n = tv_device_node_find (list, name)))
    return n;

  /* FIXME report errors */
  if ((m = tv_mixer_open ((io_debug_msg > 0) ? stderr
			  : AUDIO_MIXER_LOG_FP, name)))
    {
      if (m->inputs != NULL)
	return &m->node;

      tv_mixer_close (m);
    }

  return NULL;
}

static void
devices_audio_mixer_setup	(GtkWidget *		page)
{
  tv_mixer *m;
  tv_device_node *list, *n;
  GtkWidget *checkbutton;
  GtkWidget *alignment;
  GtkWidget *table;
  GtkWidget *label;

  m = tv_mixer_scan (AUDIO_MIXER_LOG_FP);
  list = m ? &m->node : NULL;

  /* Remove tv card mixer interfaces, these are special
     and we fall back to them automatically. */
  for (n = list; n;)
    {
      if (PARENT (n, tv_mixer, node)->inputs == NULL)
	{
	  tv_device_node *next;
	  
	  next = n->next;
	  tv_device_node_delete (&list, n, FALSE);
	  n = next;
	}
      else
	{
	  n = n->next;
	}
    }

  checkbutton = lookup_widget (page, "devices-audio-mixer");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton),
				zcg_bool (NULL, "force_mixer"));

  alignment = lookup_widget (page, "devices-audio-mixer-alignment");

  table = z_device_entry_new (/* prompt */ NULL,
			      list,
			      zcg_char (NULL, "mixer_device"),
			      devices_audio_mixer_open,
			      devices_audio_mixer_select,
			      /* user_data */ NULL);
  gtk_widget_show (table);
  gtk_container_add (GTK_CONTAINER (alignment), table);
  register_widget (NULL, table, "devices-audio-mixer-table");

  g_signal_connect (G_OBJECT (checkbutton), "toggled",
		    G_CALLBACK (on_enable_device_entry_toggled), table);
  on_enable_device_entry_toggled (GTK_TOGGLE_BUTTON (checkbutton), table);

  /* TRANSLATORS: Soundcard mixer line. */
  label = gtk_label_new (_("Input:"));
  gtk_widget_show (label);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_misc_set_padding (GTK_MISC (label), 3, 3);
  gtk_table_attach (GTK_TABLE (table), label,
		    0, 0 + 1,
		    3, 3 + 1,
		    GTK_FILL, 0, 0, 0);
}

static void
devices_audio_setup		(GtkWidget *		page)
{
  devices_audio_source_setup (page);
  devices_audio_mixer_setup (page);
}

static void
properties_add			(GtkDialog *		dialog)
{
  SidebarEntry devices [] = {
    { N_("Audio"), "gnome-grecord.png", "vbox53",
      devices_audio_setup, devices_audio_apply,
      .help_link_id = "zapping-settings-audio-device" }
  };
  SidebarEntry general [] = {
    { N_("Audio"), "gnome-grecord.png", "vbox39",
      general_audio_setup, general_audio_apply,
      .help_link_id = "zapping-settings-audio-options" }
  };
  SidebarGroup groups [] = {
    { N_("Devices"),	     devices, G_N_ELEMENTS (devices) },
    { N_("General Options"), general, G_N_ELEMENTS (general) }
  };

  standard_properties_add (dialog, groups, G_N_ELEMENTS (groups),
			   "zapping.glade2");
}

/*
 *  Mute stuff
 */

static guint		quiet_timeout_id = NO_SOURCE_ID;

static gboolean
quiet_timeout			(gpointer		user_data _unused_)
{
  tv_quiet_set (zapping->info, FALSE);
  return FALSE; /* don't call again */
}

void
reset_quiet			(tveng_device_info *	info _unused_,
				 guint			delay)
{
  if (quiet_timeout_id > 0)
    g_source_remove (quiet_timeout_id);

  if (delay > 0)
    {
      quiet_timeout_id =
	g_timeout_add (delay /* ms */, (GSourceFunc) quiet_timeout, NULL);
    }
  else
    {
      tv_quiet_set (zapping->info, FALSE);
    }
}

/*
 *  Global mute function. XXX switch to callbacks.
 *  mode: 0=sound, 1=mute, 2=toggle, 3=just update GUI
 *  controls: update controls box
 *  osd: show state on screen
 */
gboolean
set_mute				(gint	        mode,
					 gboolean	controls _unused_,
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
      mute = tv_mute_get (zapping->info, TRUE);
      if (mute == -1)
        goto failure;

      if (mode == 2)
	mute = !mute;
    }
  else
    mute = !!mode;

  /* Set new state */

  if (mode <= 2)
    if (-1 == tv_mute_set (zapping->info, mute))
      goto failure;

  /* Update GUI */

  {
    GtkAction *action;

    action = gtk_action_group_get_action (zapping->generic_action_group,
					  "Mute");
    if (mute != gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)))
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), mute);

    if (osd)
      {
	BonoboDockItem *dock_item;

	dock_item = gnome_app_get_dock_item_by_name
	  (&zapping->app, GNOME_APP_TOOLBAR_NAME);

	if (DISPLAY_MODE_BACKGROUND == zapping->display_mode
	    || DISPLAY_MODE_FULLSCREEN == zapping->display_mode
	    || dock_item == NULL
	    || !GTK_WIDGET_VISIBLE (GTK_WIDGET (dock_item)))
	  osd_render_markup_printf (NULL, mute ?
			     _("<span foreground=\"blue\">Audio off</span>") :
			     _("<span foreground=\"yellow\">Audio on</span>"));
      }
  }

  recursion = FALSE;
  return TRUE;

 failure:
  recursion = FALSE;
  return FALSE;
}

static PyObject *
py_mute				(PyObject *		self _unused_,
				 PyObject *		args)
{
  int value = 2; /* toggle by default */

  if (!ParseTuple (args, "|i", &value))
    {
      g_warning ("zapping.mute(|i)");
      py_return_false;
    }

  set_mute (value, TRUE, TRUE);

  py_return_true;
}

void startup_audio ( void )
{
  static const property_handler audio_handler = {
    .add = properties_add,
  };

  prepend_property_handler (&audio_handler);

  zcc_char ("kernel", "PCM recording source", "audio_source");
  zcc_char ("/dev/dsp", "PCM recording kernel device", "pcm_device");

  zcc_char ("", "Mixer device", "mixer_device");
  zcc_int (-1, "Mixer input line (hash)", "mixer_input");

  zconf_create_boolean (FALSE, "Mute when Zapping is started",
			"/zapping/options/main/start_muted");
  zconf_create_boolean (TRUE, "Mute on exit",
			"/zapping/options/main/quit_muted");
  zconf_create_boolean (TRUE, "Mute when changing channels",
			"/zapping/options/main/avoid_noise");

#ifdef HAVE_ESD
  if (esd_backend.init)
    esd_backend.init ();
#endif
#ifdef HAVE_ARTS
  if (arts_backend.init)
    arts_backend.init ();
#endif
#ifdef HAVE_OSS
  if (oss_backend.init)
    oss_backend.init ();
#endif

  cmd_register ("mute", py_mute, METH_VARARGS,
		("Mute/unmute"), "zapping.mute()",
		("Mute"), "zapping.mute(1)",
		("Unmute"), "zapping.mute(0)");

  if (esd_output || ivtv_audio)
    loopback_start ();
}

void shutdown_audio ( void )
{
    loopback_stop ();

#ifdef HAVE_ESD
  if (esd_backend.shutdown)
    esd_backend.shutdown ();
#endif
#ifdef HAVE_ARTS
  if (arts_backend.shutdown)
    arts_backend.shutdown ();
#endif
#ifdef HAVE_OSS
  if (oss_backend.shutdown)
    oss_backend.shutdown ();
#endif
}

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
