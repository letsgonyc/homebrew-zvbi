/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: v4l2.c,v 1.1 2000-07-04 17:40:20 garetxe Exp $ */

#include <ctype.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <asm/types.h>
#include "../videodev2.h"
#include "../log.h"
#include "../fifo.h"
#include "../options.h"
#include "video.h"

#ifdef V4L2_MAJOR_VERSION

static int			fd;
static int			uindex = -1;
static unsigned char *		cap_buffer[256];

static struct v4l2_capability	vcap;
static struct v4l2_standard	vstd;
static struct v4l2_format	vfmt;
static struct v4l2_buffer	vbuf;
static struct v4l2_requestbuffers vrbuf;

static struct v4l2_control	old_mute;

extern int			min_cap_buffers;

static char *
le4cc2str(int n)
{
	static char buf[4 * 4 + 1];
	int i, c;

	buf[0] = '\0';

	for (i = 0; i < 4; i++) {
		c = (n >> (8 * i)) & 0xFF;
		sprintf(buf + strlen(buf), isprint(c) ? "%c" : "\\%o", c);
	}

	return buf;
}

static void
capture_on(void)
{
	int str_type = V4L2_BUF_TYPE_CAPTURE;

	ASSERT("activate capturing", ioctl(fd, VIDIOC_STREAMON, &str_type) == 0);
}

static unsigned char *
wait_frame(double *ftime, int *buf_index)
{
	static double utime;
	int r = -1;

	if (uindex >= 0) {
		*buf_index = uindex;
		uindex = -1;
		*ftime = utime;
	} else
	while (r <= 0)
	{
 		fd_set fds;

		FD_ZERO(&fds);
		FD_SET(fd, &fds);

		// XXX time out if no response
		r = select(fd + 1, &fds, NULL, NULL, NULL);

		if (r < 0 && errno == EINTR)
			continue;

		ASSERT("execute select", r > 0);

		vbuf.type = V4L2_BUF_TYPE_CAPTURE;

		ASSERT("dequeue capture buffer", ioctl(fd, VIDIOC_DQBUF, &vbuf) == 0);
#if 1
		*ftime = utime = vbuf.timestamp / 1e9; // UST, currently TOD
#else
		gettimeofday(&tv, NULL);
		*ftime = utime = tv.tv_sec + tv.tv_usec / 1e6;
#endif
		*buf_index = vbuf.index;
	}

	return cap_buffer[*buf_index];
}

static void
frame_done(int buf_index)
{
	vbuf.type = V4L2_BUF_TYPE_CAPTURE;
	vbuf.index = buf_index;

	ASSERT("enqueue capture buffer", ioctl(fd, VIDIOC_QBUF, &vbuf) == 0);
}

static void
unget_frame(int buf_index)
{
	assert(uindex < 0);
	uindex = buf_index;
}

static void
mute_restore(void)
{
	if (old_mute.id)
		ioctl(fd, VIDIOC_S_CTRL, &old_mute);
}

#define DECIMATING(mode) (mode == CM_YUYV_VERTICAL_DECIMATION ||	\
			  mode == CM_YUYV_EXP_VERTICAL_DECIMATION)
#define PROGRESSIVE(mode) (mode == CM_YUYV_PROGRESSIVE || \
			   mode == CM_YUYV_PROGRESSIVE_TEMPORAL)

void
v4l2_init(void)
{
	int aligned_width;
	int aligned_height;
	unsigned int probed_modes = 0;
	int mod;

	ASSERT("open video capture device", (fd = open(cap_dev, O_RDONLY)) != -1);
	ASSERT("query video capture capabilities", ioctl(fd, VIDIOC_QUERYCAP, &vcap) == 0);

	if (vcap.type != V4L2_TYPE_CAPTURE)
		FAIL("%s ('%s') is not a capture device",
			cap_dev, vcap.name);

	if (!(vcap.flags & V4L2_FLAG_STREAMING) ||
	    !(vcap.flags & V4L2_FLAG_SELECT))
		FAIL("%s ('%s') does not support streaming/select(2),\n"
			"%s will not work with the v4l2 read(2) interface.",
			cap_dev, vcap.name, my_name);

	printv(2, "Opened %s ('%s')\n", cap_dev, vcap.name);

	
	ASSERT("query current video standard", ioctl(fd, VIDIOC_G_STD, &vstd) == 0);

	frame_rate_code = ((double) vstd.framerate.denominator /
				    vstd.framerate.numerator < 29.0) ? 3 : 4;

	if (frame_rate_code == 4 && grab_height == 288)
		grab_height = 240; // XXX DAU

	if (PROGRESSIVE(filter_mode)) {
		frame_rate_code += 3; // see frame_rate_value[]
		min_cap_buffers++;
	}

	printv(2, "Video standard is '%s'\n", vstd.name);


	if (mute != 2) {
		old_mute.id = V4L2_CID_AUDIO_MUTE;

		if (ioctl(fd, VIDIOC_G_CTRL, &old_mute) == 0) {
			static const char *mute_options[] = { "unmuted", "muted" };
			struct v4l2_control new_mute;

			atexit(mute_restore);

			new_mute.id = V4L2_CID_AUDIO_MUTE;
			new_mute.value = !!mute;

			ASSERT("set mute control to %d",
				ioctl(fd, VIDIOC_S_CTRL, &new_mute) == 0, !!mute);

			printv(2, "Audio %s\n", mute_options[!!mute]);
		} else {
			old_mute.id = 0;

			ASSERT("read mute control", errno == EINVAL /* unsupported */);
		}
	}


	grab_width = saturate(grab_width, 1, MAX_WIDTH);
	grab_height = saturate(grab_height, 1, MAX_HEIGHT);

	if (DECIMATING(filter_mode))
		aligned_height = (grab_height * 2 + 15) & -16;
	else
		aligned_height = (grab_height + 15) & -16;

	aligned_width  = (grab_width + 15) & -16;

	for (;;) {
		int new_mode;

		probed_modes |= 1 << filter_mode;

		vfmt.fmt.pix.width = aligned_width;
		vfmt.fmt.pix.height = aligned_height;

		if (filter_mode == CM_YUV) {
			vfmt.fmt.pix.depth = 12;
			vfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
		} else {
			vfmt.fmt.pix.depth = 16;
			vfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
		}

		if (PROGRESSIVE(filter_mode))
			vfmt.fmt.pix.flags = V4L2_FMT_FLAG_TOPFIELD |
					     V4L2_FMT_FLAG_BOTFIELD;
		else
			vfmt.fmt.pix.flags = V4L2_FMT_FLAG_INTERLACED;

		if (ioctl(fd, VIDIOC_S_FMT, &vfmt) == 0) {
			if (!DECIMATING(filter_mode))
				break;
			if (vfmt.fmt.pix.height > aligned_height * 0.7)
				break;
		}

		if (filter_mode == CM_YUYV)
			new_mode = CM_YUV;
		else if (filter_mode == CM_YUYV_VERTICAL_DECIMATION) {
			new_mode = CM_YUYV_VERTICAL_INTERPOLATION;
			aligned_height = (grab_height + 15) & -16;
		} else
			new_mode = CM_YUYV;

		if (probed_modes & (1 << new_mode)) {
			FAIL("%s ('%s') does not support the requested format at size %d x %d",
				cap_dev, vcap.name, aligned_width, aligned_height);
		}

		printv(3, "Format '%s' %d x %d not accepted,\ntrying '%s'\n",
			filter_labels[filter_mode],
			aligned_width, aligned_height,
			filter_labels[new_mode]);

		filter_mode = new_mode;
	}

	mod = DECIMATING(filter_mode) ? 32 : 16;

	if (vfmt.fmt.pix.width & 15 || vfmt.fmt.pix.height & (mod - 1)) {
		printv(3, "Format granted %d x %d, attempt to modify\n",
			vfmt.fmt.pix.width, vfmt.fmt.pix.height);

		vfmt.fmt.pix.width	&= -16;
		vfmt.fmt.pix.height	&= -mod;

		if (ioctl(fd, VIDIOC_S_FMT, &vfmt) != 0 ||
		    vfmt.fmt.pix.width & 15 || vfmt.fmt.pix.height & (mod - 1)) {
			FAIL("Please try a different grab size");
		}
	}

	if (vfmt.fmt.pix.width != aligned_width ||
	    vfmt.fmt.pix.height != aligned_height) {
		if (verbose > 0) {
			char str[256];

			fprintf(stderr, "'%s' offers a grab size %d x %d, continue? ",
				vcap.name, vfmt.fmt.pix.width,
				vfmt.fmt.pix.height / (DECIMATING(filter_mode) ? 2 : 1));
			fflush(stderr);

			fgets(str, 256, stdin);

			if (tolower(*str) != 'y')
				exit(EXIT_FAILURE);
		} else
			FAIL("Requested grab size not available");
	}

	grab_width = vfmt.fmt.pix.width & -16;
	grab_height = vfmt.fmt.pix.height & -mod;

	if (width > grab_width)
		width = grab_width;
	if (height > grab_height)
		height = grab_height;

	filter_init(
		vfmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUV420 ?
		vfmt.fmt.pix.width : vfmt.fmt.pix.width * 2);

	printv(2, "Image format '%s' %d x %d granted\n",
		le4cc2str(vfmt.fmt.pix.pixelformat), vfmt.fmt.pix.width, vfmt.fmt.pix.height);

	vrbuf.type = V4L2_BUF_TYPE_CAPTURE;
	vrbuf.count = MAX(cap_buffers, min_cap_buffers);

	ASSERT("request capture buffers", ioctl(fd, VIDIOC_REQBUFS, &vrbuf) == 0);

	if (vrbuf.count == 0)
		FAIL("No capture buffers granted");

	printv(2, "%d capture buffers granted\n", vrbuf.count);


	// Map capture buffers

	for (cap_buffers = 0; cap_buffers < vrbuf.count; cap_buffers++) {
		vbuf.type = V4L2_BUF_TYPE_CAPTURE;
		vbuf.index = cap_buffers;

		printv(3, "Mapping capture buffer #%d\n", cap_buffers);

		ASSERT("query capture buffer #%d", ioctl(fd, VIDIOC_QUERYBUF, &vbuf) == 0, cap_buffers);

		cap_buffer[cap_buffers] = mmap(NULL, vbuf.length, PROT_READ,
			MAP_SHARED, fd, vbuf.offset);

		if ((int) cap_buffer[cap_buffers] == -1) {
			if (errno == ENOMEM && cap_buffers > 0)
				break;
			else
				ASSERT("map capture buffer #%d", 0, cap_buffers);
		}

		ASSERT("enqueue capture buffer #%d", ioctl(fd, VIDIOC_QBUF, &vbuf) == 0, cap_buffers);
	}

	if (cap_buffers < min_cap_buffers)
		FAIL("Cannot allocate enough (%d) capture buffers", min_cap_buffers);

	video_start = capture_on;
	video_wait_frame = wait_frame;
	video_frame_done = frame_done;
	video_unget_frame = unget_frame;
}

#endif // V4L2
