/*
 *  Zapping (TV viewer for the Gnome Desktop)
 *
 * Copyright (C) 2000 I�aki Garc�a Etxebarria
 * Copyright (C) 2003 Michael H. Schimek
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

/* $Id: v4l25.c,v 1.4 2004-05-16 11:43:18 mschimek Exp $ */

#include "../config.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "zapping_setup_fb.h"

#ifdef ENABLE_V4L

#include <fcntl.h>
#include <sys/ioctl.h>

#include <linux/types.h>		/* __u32 etc */
#include <sys/time.h>			/* struct timeval */
#include "../common/videodev25.h"	/* V4L2 header file */
#include "../common/_videodev25.h"

#define v4l25_ioctl(fd, cmd, arg)					\
  (IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),					\
   device_ioctl (log_fp, fprint_ioctl_arg, fd, cmd, arg))

int
setup_v4l25			(const char *		device_name,
				 x11_dga_parameters *	dga)
{
  int fd;
  struct v4l2_capability cap;
  struct v4l2_framebuffer fb;

  message (2, "Opening video device.\n");

  if (-1 == (fd = device_open_safer (device_name, 81, O_RDWR)))
    return -1;

  message (2, "Querying device capabilities.\n");

  if (-1 == v4l25_ioctl (fd, VIDIOC_QUERYCAP, &cap))
    {
      errmsg ("VIDIOC_QUERYCAP ioctl failed,\n  probably not a V4L2 2.5 device");
      close (fd);
      return -1;
    }

  message (1, "Using V4L2 2.5 interface.\n");

  message (2, "Checking overlay capability.\n");

  if (!(cap.capabilities & V4L2_CAP_VIDEO_OVERLAY))
    {
      message (1, "Device '%s' does not support video overlay.\n", device_name);
      goto failure;
    }

  message (2, "Getting current frame buffer parameters.\n");

  if (-1 == v4l25_ioctl (fd, VIDIOC_G_FBUF, &fb))
    {
      errmsg ("VIDIOC_G_FBUF ioctl failed");
      goto failure;
    }

  if (fb.capability & V4L2_FBUF_CAP_EXTERNOVERLAY)
    {
      message (2, "Genlock device, no setup necessary.\n");
      close (fd);
      return 1;
    }

  memset (&fb, 0, sizeof (fb));

  fb.base		= dga->base;
  fb.fmt.width		= dga->width;
  fb.fmt.height		= dga->height;

  switch (dga->depth)
    {
    case  8:
      fb.fmt.pixelformat = V4L2_PIX_FMT_HI240; /* XXX bttv only */
      break;

      /* Note defines and Spec (0.4) are wrong: r <-> b,
	 RGB32 == A,R,G,B in bttv 0.9 unlike description in Spec */

#if BYTE_ORDER == BIG_ENDIAN /* safe? */
    case 15:
      fb.fmt.pixelformat = V4L2_PIX_FMT_RGB555X;
      break;
    case 16:
      fb.fmt.pixelformat = V4L2_PIX_FMT_RGB565X;
      break;
    case 24:
    case 32:
      if (dga->bits_per_pixel == 24)
	fb.fmt.pixelformat = V4L2_PIX_FMT_RGB24;
      else
	fb.fmt.pixelformat = V4L2_PIX_FMT_RGB32;
      break;
#else
    case 15:
      fb.fmt.pixelformat = V4L2_PIX_FMT_RGB555;
      break;
    case 16:
      fb.fmt.pixelformat = V4L2_PIX_FMT_RGB565;
      break;
    case 24:
    case 32:
      if (dga->bits_per_pixel == 24)
	fb.fmt.pixelformat = V4L2_PIX_FMT_BGR24;
      else
	fb.fmt.pixelformat = V4L2_PIX_FMT_BGR32;
      break;
#endif
    }

  fb.fmt.bytesperline	= dga->bytes_per_line;
  fb.fmt.sizeimage	= dga->height * fb.fmt.bytesperline;

  message (2, "Setting new frame buffer parameters.\n");

  /*
   *  This ioctl is privileged because it sets up
   *  DMA to a random (video memory) address. 
   */
  {
    int success;
    int saved_errno;

    if (!restore_root_privileges ())
      goto failure;

    success = ioctl (fd, VIDIOC_S_FBUF, &fb);
    saved_errno = errno;

    drop_root_privileges ();

    if (success == -1)
      {
	errno = saved_errno;

        errmsg ("VIDIOC_S_FBUF ioctl failed");

        if (EPERM == saved_errno && ROOT_UID != euid)
	  privilege_hint ();

      failure:
	close (fd);
        return 0;
      }
  }

  close (fd);

  return 1;
}

#else /* !ENABLE_V4L */

int
setup_v4l25			(const char *		device_name,
				 x11_dga_parameters *	dga)
{
  return -1;
}

#endif /* !ENABLE_V4L */
