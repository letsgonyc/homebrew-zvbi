/*
 *  Zapping (TV viewer for the Gnome Desktop)
 *
 * Copyright (C) 2000 I�aki Garc�a Etxebarria
 * Copyright (C) 2003, 2005 Michael H. Schimek
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

/* $Id: v4l2.c,v 1.11 2005-10-14 23:40:14 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "zapping_setup_fb.h"

#ifdef ENABLE_V4L

#include "common/videodev2.h" /* V4L2 header file */
#include "common/_videodev2.h"

#define xioctl(fd, cmd, arg)						\
  (IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),					\
   device_ioctl (device_log_fp, fprint_v4l2_ioctl_arg, fd, cmd, arg))

/* Attn: device_name may be NULL, device_fd may be -1. */
int
setup_v4l2			(const char *		device_name,
				 int			device_fd,
				 const tv_overlay_buffer *buffer)
{
  int fd;
  struct v4l2_capability cap;

  buffer = buffer; /* unused */

  fd = device_open_safer (device_name, device_fd, /* major */ 81, O_RDWR);
  if (-1 == fd)
    return -1; /* failed */

  message (/* verbosity */ 2,
	   "Querying device capabilities.\n");

  if (-1 == xioctl (fd, VIDIOC_QUERYCAP, &cap))
    {
      int saved_errno = errno;

      device_close (device_log_fp, fd);

      if (EINVAL == saved_errno)
	{
	  message (/* verbosity */ 2,
		   "Not a V4L2 0.20 device.\n");

	  errno = EINVAL;
	  return -2; /* unknown API */
	} 
      else
	{
	  errno = saved_errno;
	  return -1; /* failed */
	}
    }

  device_close (device_log_fp, fd);

  /* V4L2 0.20 is obsolete, superseded by V4L2 of Linux 2.6. */

  message (/* verbosity */ 2,
	   "V4L2 0.20 API is no longer supported.\n");

  errno = EINVAL;
  return -2; /* unknown API (may talk V4L) */
}

#else /* !ENABLE_V4L */

int
setup_v4l2			(const char *		device_name,
				 const tv_overlay_buffer *buffer)
{
  message (/* verbosity */ 2,
	   "No V4L2 0.20 support compiled in.\n");

  errno = EINVAL;
  return -2; /* unknown API */
}

#endif /* !ENABLE_V4L */
