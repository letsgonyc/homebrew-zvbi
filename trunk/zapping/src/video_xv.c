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
 * XVideo backend.
 * TODO: (?) Figure a way to re-enable the tveng_set_xv_port
 *	     stuff, was a nice hack.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#define ZCONF_DOMAIN "/zapping/options/main/"
#include "zconf.h"
#include "globals.h"
#include "zimage.h"
#include "zmisc.h"
#include "capture.h"

#ifdef USE_XV /* Real stuff */

static gboolean have_mitshm;

/*
  Comment out if you have problems with the Shm extension
  (you keep getting a Gdk-error request_code:14x, minor_code:19)
*/
#define USE_XV_SHM 1

struct _zimage_private {
  gboolean		uses_shm;
  XvImage		*image;
  /* Port this image belongs to */
  XvPortID		xvport;
#ifdef USE_XV_SHM
  XShmSegmentInfo	shminfo; /* shared mem info for the xvimage */
#endif
};

static GdkWindow *window = NULL;
static GdkGC *gc = NULL;

/*
  This curious construct assures that we only grab the minimum set of
  needed ports for all the pixformats we blit, whilst assuring that we
  try as hard as possible to grab anything grabable.
  We could also forget about grabbing and try to directly use the
  ports, but i suppose that isn't good practise (?)
  In any case, that's the trivial scenario if we decide to go that route.
*/
typedef struct {
	XvPortID		xvport;
	int			refcount; /* > 0 grabbed, 0 ungrabed */
} XvPortID_ref;

static XvPortID_ref *		xvports = NULL;
static int			num_xvports = 0;

static struct {
	int			format_id;	/* XvImageFormat */
	int			swap_uv;
	/* Ports providing this format (index in xvports struct) */
	int *			ports;
	int			num_ports;
} formats [TV_MAX_PIXFMTS];

static XvPortID
grab_port (tv_pixfmt pixfmt)
{
  gint i;

  for (i=0; i<formats[pixfmt].num_ports; i++)
    {
      XvPortID_ref *ref = xvports + formats[pixfmt].ports[i];
      if (ref->refcount > 0)
	{
	  /* there's already a grabbed port with the right fmt
	     supported */
	  ref->refcount ++;
	  return ref->xvport;
	}

      if (Success == XvGrabPort (GDK_DISPLAY (), ref->xvport, CurrentTime))
	{
	  ref->refcount ++;
	  return ref->xvport;
	}
    }

  return None;
}

/* Decrease the refcount of a given port, and ungrab it when it
   reaches 0 */
static void
ungrab_port (XvPortID	xvport)
{
  int i;

  for (i = 0; i<num_xvports; i++)
    printv ("Port: %d, %d\n", (int)xvports[i].xvport,
	    xvports[i].refcount);

  for (i = 0; i<num_xvports; i++)
    if (xvports[i].xvport == xvport)
      {
	if ((--xvports[i].refcount) < 1)
	  {
	    /* Check for screwed up stuff */
	    g_assert (xvports[i].refcount == 0);
	    XvUngrabPort (GDK_DISPLAY (), xvports[i].xvport,
			  CurrentTime);
	  }

	return;
      }

  /* Port not found, meaning that we are ungrabbing a port we don't
     know about... bad */
  g_assert_not_reached ();
}

/**
 * Create a new XV image with the given attributes, returns NULL on error.
 */
static zimage *
image_new(tv_pixfmt pixfmt, guint w, guint h)
{
  zimage *new_image;
  struct _zimage_private *pimage;
  void *image_data = NULL;
  const tv_pixel_format *pf;
  XvPortID xvport;

  xvport = grab_port (pixfmt);
  if (None == xvport)
    return NULL; /* Cannot grab a suitable port */

  pimage = g_malloc0 (sizeof (*pimage));

  pf = tv_pixel_format_from_pixfmt (pixfmt);
  assert (NULL != pf);

  pimage -> uses_shm = FALSE;

#ifdef USE_XV_SHM

  if (have_mitshm) /* just in case */
    {
      CLEAR (pimage->shminfo);
      pimage->image = XvShmCreateImage(GDK_DISPLAY(), xvport,
	formats[pixfmt].format_id, NULL, (gint) w, (gint) h, &pimage->shminfo);

      if (pimage->image)
	{
	  pimage->uses_shm = TRUE;

	  pimage->shminfo.shmid =
	    shmget (IPC_PRIVATE,
		    (unsigned) pimage->image->data_size, IPC_CREAT | 0777);

	  if (-1 == pimage->shminfo.shmid)
            {
	      goto shm_error;
	    }
	  else
	    {
	      pimage->shminfo.shmaddr =
		shmat (pimage->shminfo.shmid, NULL /* anywhere */, 0);

	      pimage->image->data = pimage->shminfo.shmaddr;

	      if (pimage->shminfo.shmaddr == (void *) -1)
	        goto shm_error;

	      pimage->shminfo.readOnly = False;

	      if (!XShmAttach(GDK_DISPLAY(), &pimage->shminfo))
	        {
		  g_assert(shmdt(pimage->shminfo.shmaddr) != -1);
 shm_error:
		  XFree(pimage->image);
		  pimage->image = NULL;
	          pimage->uses_shm = FALSE;
		}

	      XSync (GDK_DISPLAY(), False);

	      /* Free the memory when the last attached
		 process quits or aborts. */
	      shmctl(pimage->shminfo.shmid, IPC_RMID, 0);
	    }
	}
    }

#endif /* USE_XV_SHM */

  if (!pimage->image)
    {
      if (pf->planar)
	image_data = malloc ((pf->color_depth * w * h) >> 3);
      else
	image_data = malloc ((pf->bits_per_pixel * w * h) >> 3);

      if (!image_data)
	{
	  g_warning("XV image data allocation failed");
	  goto error1;
	}
      pimage->image =
	XvCreateImage(GDK_DISPLAY(), xvport,
		      formats[pixfmt].format_id,
		      image_data, (gint) w, (gint) h);
      if (!pimage->image)
        goto error2;
    }

  /* Make sure we get an object with appropiate width and height */
  if ((guint) pimage->image->width != w ||
      (guint) pimage->image->height != h)
    goto error3;

  new_image = zimage_create_object ();
  new_image->priv = pimage;
  pimage->xvport = xvport;

  new_image->fmt.width = w;
  new_image->fmt.height = h;
  new_image->fmt.pixfmt = pixfmt;
  new_image->fmt.bytes_per_line = (w * pf->bits_per_pixel) >> 3;
  new_image->fmt.size = pimage->image->data_size;

  if (TV_PIXFMT_SET_YUV_PLANAR & TV_PIXFMT_SET (pixfmt))
    {
      int swap_uv = formats[pixfmt].swap_uv;

      g_assert (pimage->image->num_planes == 3);
      g_assert (pimage->image->pitches[1] ==
		pimage->image->pitches[2]);

      new_image->data.planar.y =
	pimage->image->data + pimage->image->offsets[0];
      new_image->data.planar.y_stride =
	pimage->image->pitches[0];
      new_image->data.planar.u =
	pimage->image->data + pimage->image->offsets[1 + swap_uv];
      new_image->data.planar.v =
	pimage->image->data + pimage->image->offsets[2 - swap_uv];
      new_image->data.planar.uv_stride =
	pimage->image->pitches[1];
    }
  else if (TV_PIXFMT_SET_PACKED & TV_PIXFMT_SET (pixfmt))
    {
      g_assert (pimage->image->num_planes == 1);
      new_image->data.linear.data = pimage->image->data;
      new_image->data.linear.stride = pimage->image->pitches[0];
    }
  else
    {
      g_assert_not_reached ();
    }

  printv ("Created image: %s %dx%d, %d, %d\n",
	  tv_pixfmt_name (new_image->fmt.pixfmt),
	  new_image->fmt.width,
	  new_image->fmt.height,
	  new_image->data.planar.y_stride,
	  new_image->data.planar.uv_stride);

  return new_image;

 error3:
#ifdef USE_XV_SHM
  if (pimage->uses_shm)
    XShmDetach(GDK_DISPLAY(), &pimage->shminfo);
#endif
  XFree (pimage->image);

 error2:
  if (!pimage->uses_shm)
    free(image_data);

 error1:
  g_free(pimage);

  return NULL;
}

/**
 * Puts the image in the given drawable, scales to the drawable's size.
 */
static void
image_put(zimage *image, guint width, guint height)
{
  zimage_private *pimage = image->priv;
  Display *display;

  if (!window || !gc)
    return;

  if (!pimage->image)
    {
      g_warning("Trying to put an empty XV image");
      return;
    }

  display = GDK_DISPLAY ();

#ifdef USE_XV_SHM
  if (pimage->uses_shm)
    XvShmPutImage(display, pimage->xvport,
		  GDK_WINDOW_XWINDOW(window),
		  GDK_GC_XGC(gc), pimage->image,
		  0, 0, image->fmt.width, image->fmt.height, /* source */
		  0, 0, width, height, /* dest */
		  True);
#endif

  if (!pimage->uses_shm)
    XvPutImage(display, pimage->xvport,
	       GDK_WINDOW_XWINDOW(window),
	       GDK_GC_XGC(gc), pimage->image,
	       0, 0, image->fmt.width, image->fmt.height, /* source */
	       0, 0, width, height /* dest */);

  XFlush (display);
}

/**
 * Frees the data associated with the image
 */
static void
image_destroy(zimage *image)
{
  zimage_private *pimage = image->priv;

#ifdef USE_XV_SHM
  if (pimage->uses_shm)
    XShmDetach(GDK_DISPLAY(), &pimage->shminfo);
#endif

  if (!pimage->uses_shm)
    free(pimage->image->data);

  XFree(pimage->image);

#ifdef USE_XV_SHM
  if (pimage->uses_shm)
    g_assert(shmdt(pimage->shminfo.shmaddr) != -1);
#endif

  printv ("ungrabing %d\n", (gint)pimage->xvport);
  ungrab_port (pimage->xvport);

  g_free(image->priv);
}

static void
set_destination (GdkWindow *_w, GdkGC *_gc,
		 tveng_device_info *info _unused_)
{
  window = _w;
  gc = _gc;
}

static void
unset_destination(tveng_device_info *info _unused_)
{
  window = NULL;
  gc = NULL;
}

static gboolean
suggest_format (void)
{
  /*  tv_pixfmt pixfmt;*/

/* FIXME suggest what (as capture format)? We should
   not suggest anything, just list supported formats and
   how expensive they are in terms of CPU and memory usage. */
#if 0
  for (pixfmt = 0; pixfmt < TV_MAX_PIXFMTS; pixfmt++)
    if (TV_PIXFMT_SET_ALL & TV_PIXFMT_SET (pixfmt))
      if (formats[pixfmt].num_ports > 0)
	{
	  capture_fmt fmt;
	  fmt.pixfmt = pixfmt;
	  fmt.locked = FALSE;
	  if (-1 != suggest_capture_format (&fmt))
	    /* Capture format granted */
	    return TRUE;
	}
#endif
  return FALSE;
}

static video_backend xv = {
  name:			"XVideo Backend Scaler",
  set_destination:	set_destination,
  unset_destination:	unset_destination,
  image_new:		image_new,
  image_destroy:	image_destroy,
  image_put:		image_put,
  suggest_format:	suggest_format
};

/*
  XV_VIDEO
  XV_IMAGE
  XV_ENCODING

  XV_BRIGHTNESS
  XV_CONTRAST
  XV_SATURATION
  XV_HUE
  XV_COLORKEY 0x000000 ... 0xFFFFFF
  XV_INTERLACE
    Neomagic: 0 combine fields, 1 one field, 2 interlaced
    SMI: boolean
    PM2: boolean
  XV_CAPTURE_BRIGHTNESS
    SMI: SAA7111 brightness (vs. XV_BRIGHTNESS of overlay)
  XV_DOUBLE_BUFFER
    PM3: boolean
    MGA: boolean
    ATI: boolean
    NV: boolean
  XV_AUTOPAINT_COLORKEY
    PM3: boolean
    NV: boolean
  XV_FILTER
    PM3: 0 off, 1 full, 2 partial
    PM2: boolean
  XV_ALPHA:
    PM2: boolean (use alpha channel)
  XV_BKGCOLOR:
    PM2: 0x00RRGGBB for tv out
  XV_FILTER_QUALITY:
    TDFX: 0, 1
  XV_SET_DEFAULTS:
    SIS: action
    NV: action
  XV_FREQ:
    v4l: n * 62500 Hz unit
  XV_MUTE:
    v4l: boolean
  XV_VOLUME:
    v4l: -1000, 1000
  XV_COLORKEYMODE:
    NSC: ?
  XV_RED_INTENSITY:
    ati: -1000, 1000
  XV_GREEN_INTENSITY:
    ati: -1000, 1000
  XV_BLUE_INTENSITY:
    ati: -1000, 1000
  XV_ITURBT_709:
    nv: boolean
 */

static void
register_port			(XvPortID		xvport,
				 tv_pixfmt		pixfmt,
				 int			format_id,
				 int			swap_uv,
				 unsigned int		adaptor_index)
{
  int i, id = -1;

  /* First check whether there's an existing entry for this port. */
  for (i=0; i<num_xvports; i++)
    if (xvports[i].xvport == xvport)
      id = i;

  /* Not yet registered, add a new entry for the port */
  if (id == -1)
    {
      xvports = g_realloc (xvports, (num_xvports+1)*(sizeof(*xvports)));
      id = num_xvports ++;
      xvports[id].refcount = 0;
      xvports[id].xvport = xvport;
    }

  /* Now check whether we already knew this port for this format (we
     shouldn't, but checking won't harm). */
  for (i=0; i<formats[pixfmt].num_ports; i++)
    if (formats[pixfmt].ports[i] == id)
      return; /* All done */

  /* Check if this port uses the same format_id as other ports
     supporting the same format. */
  if (formats[pixfmt].num_ports > 0
      && formats[pixfmt].format_id != format_id)
    return; /* Bizarre. */

  /* Add a new entry for the port in this format */
  formats[pixfmt].ports = g_realloc
    (formats[pixfmt].ports,
     (formats[pixfmt].num_ports+1)*sizeof(formats[pixfmt].ports[0]));
  formats[pixfmt].ports[formats[pixfmt].num_ports++] = id;

  formats[pixfmt].format_id = format_id;
  formats[pixfmt].swap_uv = swap_uv;

  /* If this is the first port we add for the given format add
     ourselves to the backend list for the pixformat. */
  if (formats[pixfmt].num_ports == 1)
    register_video_backend (pixfmt, &xv);

  printv ("Registered XVideo adaptor %u, image port 0x%x with pixfmt %s\n",
	  adaptor_index,
	  (unsigned int) xvport,
	  tv_pixfmt_name (pixfmt));
}

static void
traverse_ports			(Display *		display,
				 const XvAdaptorInfo *	pAdaptor,
				 unsigned int		index)
{
  unsigned int i;

  for (i = 0; i < pAdaptor->num_ports; ++i)
    {
      XvImageFormatValues *pImageFormats;
      int nImageFormats;
      XvPortID xvport;
      int j;
      tv_pixfmt pixfmt;

      xvport = pAdaptor->base_id + i;

      if ((XvPortID) xv_image_port != (XvPortID) -1
	  && xvport != (XvPortID) xv_image_port)
	continue;

      pImageFormats = XvListImageFormats (display, xvport, &nImageFormats);

      if (NULL == pImageFormats || 0 == nImageFormats)
	continue;

      for (j = 0; j < nImageFormats; ++j)
	{
	  pixfmt = x11_xv_image_format_to_pixfmt (pImageFormats + j);

	  switch (pixfmt)
	    {
	    case TV_PIXFMT_YUV420:
	      register_port (xvport, TV_PIXFMT_YUV420,
			     pImageFormats[j].id, FALSE, index);
	      register_port (xvport, TV_PIXFMT_YVU420,
			     pImageFormats[j].id, TRUE, index);
	      break;

	    case TV_PIXFMT_YVU420:
	      register_port (xvport, TV_PIXFMT_YUV420,
			     pImageFormats[j].id, TRUE, index);
	      register_port (xvport, TV_PIXFMT_YVU420,
			     pImageFormats[j].id, FALSE, index);
	      break;

	    case TV_PIXFMT_YUYV:
	    case TV_PIXFMT_YVYU:
	    case TV_PIXFMT_UYVY:
	    case TV_PIXFMT_VYUY:
	      register_port (xvport, pixfmt,
			     pImageFormats[j].id, FALSE, index);
	      break;

	    case TV_PIXFMT_RGBA32_LE:
	    case TV_PIXFMT_RGBA32_BE:
	    case TV_PIXFMT_BGRA32_LE:
	    case TV_PIXFMT_BGRA32_BE:
	    case TV_PIXFMT_RGB24_LE:
	    case TV_PIXFMT_BGR24_LE:
	    case TV_PIXFMT_RGB16_LE:
	    case TV_PIXFMT_RGB16_BE:
	    case TV_PIXFMT_BGR16_LE:
	    case TV_PIXFMT_BGR16_BE:
	    case TV_PIXFMT_RGBA16_LE:
	    case TV_PIXFMT_RGBA16_BE:
	    case TV_PIXFMT_BGRA16_LE:
	    case TV_PIXFMT_BGRA16_BE:
	    case TV_PIXFMT_ARGB16_LE:
	    case TV_PIXFMT_ARGB16_BE:
	    case TV_PIXFMT_ABGR16_LE:
	    case TV_PIXFMT_ABGR16_BE:
	    case TV_PIXFMT_RGBA12_LE:
	    case TV_PIXFMT_RGBA12_BE:
	    case TV_PIXFMT_BGRA12_LE:
	    case TV_PIXFMT_BGRA12_BE:
	    case TV_PIXFMT_ARGB12_LE:
	    case TV_PIXFMT_ARGB12_BE:
	    case TV_PIXFMT_ABGR12_LE:
	    case TV_PIXFMT_ABGR12_BE:
	    case TV_PIXFMT_RGB8:
	    case TV_PIXFMT_BGR8:
	    case TV_PIXFMT_RGBA8:
	    case TV_PIXFMT_BGRA8:
	    case TV_PIXFMT_ARGB8:
	    case TV_PIXFMT_ABGR8:
	      register_port (xvport, pixfmt, pImageFormats[j].id, FALSE, index);
	      break;

	    default:
	      break;
	    }
	}

      if (nImageFormats > 0)
	XFree (pImageFormats);
    }
}

/**
 * Check whether XV is present and could work.
 */
void add_backend_xv (void);
void add_backend_xv (void)
{
  Display *display;
  unsigned int version;
  unsigned int revision;
  unsigned int major_opcode;
  unsigned int event_base;
  unsigned int error_base;
  Window root;
  XvAdaptorInfo *pAdaptors;
  int nAdaptors;
  int i;

  if (disable_xv || disable_xv_image)
    return;

  printv ("xv_image_port 0x%x\n", xv_image_port);

  display = GDK_DISPLAY ();

  if (Success != XvQueryExtension (display,
				   &version, &revision,
				   &major_opcode,
				   &event_base, &error_base))
    {
      printv ("XVideo extension not available\n");
      return;
    }

  printv ("XVideo opcode %d, base %d, %d, version %d.%d\n",
	  major_opcode,
	  event_base, error_base,
	  version, revision);

  if (version < 2 || (version == 2 && revision < 2))
    {
      printv ("XVideo extension not usable\n");
      return;
    }

  root = GDK_ROOT_WINDOW();

  if (Success != XvQueryAdaptors (display, root, &nAdaptors, &pAdaptors))
    {
      printv ("XvQueryAdaptors failed\n");
      return;
    }

  if (nAdaptors <= 0)
    {
      printv ("No XVideo adaptors\n");
      return;
    }

  for (i = 0; i < nAdaptors; ++i)
    {
      XvAdaptorInfo *pAdaptor;
      unsigned int type;

      pAdaptor = pAdaptors + i;

      type = pAdaptor->type;

      if (0 == strcmp (pAdaptor->name, "NVIDIA Video Interface Port")
	  && type == (XvInputMask | XvVideoMask))
	type = XvOutputMask | XvVideoMask; /* Bug. This is TV out. */

      /* FIXME */
      if (0 == strcmp (pAdaptor->name, "NV Video Overlay")
          || 0 == strcmp (pAdaptor->name, "NV10 Video Overlay"))
	continue;

      if ((XvInputMask | XvImageMask) == (type & (XvInputMask | XvImageMask)))
	traverse_ports (display, pAdaptor, (unsigned) i);
    }

  XvFreeAdaptorInfo (pAdaptors);

#ifdef USE_XV_SHM
  have_mitshm = !!XShmQueryExtension (display);
#endif
}

#else /* !USE_XV, do not add the backend */

void add_backend_xv (void);
void add_backend_xv (void)
{
}

#endif
