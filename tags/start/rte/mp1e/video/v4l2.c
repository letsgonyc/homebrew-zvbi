/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 2.
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

/* $Id: v4l2.c,v 1.1.1.1 2001-08-07 22:10:11 garetxe Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "site_def.h"

#include <ctype.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <asm/types.h>
#include "../common/videodev2.h"
#include "../common/log.h"
#include "../common/fifo.h"
#include "../common/math.h"
#include "../options.h"
#include "video.h"

#define IOCTL(fd, cmd, data) (TEMP_FAILURE_RETRY(ioctl(fd, cmd, data)))

#ifdef V4L2_MAJOR_VERSION

static int			fd;
static fifo2			cap_fifo;
static producer			cap_prod;
static buffer2 *		buffers;

static struct v4l2_capability	vcap;
static struct v4l2_standard	vstd;
static struct v4l2_format	vfmt;
static struct v4l2_buffer	vbuf;
static struct v4l2_requestbuffers vrbuf;

static struct v4l2_control	old_mute;


#define VIDIOC_PSB _IOW('v', 193, int)

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

static bool
capture_on(fifo2 *unused)
{
	int str_type = V4L2_BUF_TYPE_CAPTURE;

	return IOCTL(fd, VIDIOC_STREAMON, &str_type) == 0;
}

static void
wait_full(fifo2 *f)
{
	struct v4l2_buffer vbuf;
	struct timeval tv;
	fd_set fds;
	buffer2 *b;
	int r = -1;

	while (r <= 0) {
		FD_ZERO(&fds);
		FD_SET(fd, &fds);

		tv.tv_sec = 2;
		tv.tv_usec = 0;

		r = select(fd + 1, &fds, NULL, NULL, &tv);

		if (r < 0 && errno == EINTR)
			continue;

		if (r == 0)
			FAIL("Video capture timeout");

		ASSERT("execute select", r > 0);
	}

	vbuf.type = V4L2_BUF_TYPE_CAPTURE;

	ASSERT("dequeue capture buffer",
		IOCTL(fd, VIDIOC_DQBUF, &vbuf) == 0);

	b = buffers + vbuf.index;

#if 1
	b->time = vbuf.timestamp * (1 / 1e9); // UST, currently TOD
#else
	b->time = current_time();
#endif
	send_full_buffer2(&cap_prod, b);
}

/* Attention buffers are returned out of order */

static void
send_empty(consumer *c, buffer2 *b)
{
	struct v4l2_buffer vbuf;

	// XXX
	rem_node3(&c->fifo->full, &b->node);

	vbuf.type = V4L2_BUF_TYPE_CAPTURE;
	vbuf.index = b - buffers;

	ASSERT("enqueue capture buffer",
		IOCTL(fd, VIDIOC_QBUF, &vbuf) == 0);
}

static void
mute_restore(void)
{
	if (old_mute.id)
		IOCTL(fd, VIDIOC_S_CTRL, &old_mute);
}

#define DECIMATING(mode) (mode == CM_YUYV_VERTICAL_DECIMATION ||	\
			  mode == CM_YUYV_EXP_VERTICAL_DECIMATION)
#define PROGRESSIVE(mode) (mode == CM_YUYV_PROGRESSIVE || \
			   mode == CM_YUYV_PROGRESSIVE_TEMPORAL)

fifo2 *
v4l2_init(void)
{
	int aligned_width;
	int aligned_height;
	unsigned int probed_modes = 0;
	int mod;
	int min_cap_buffers = video_look_ahead(gop_sequence);
	int i;

	ASSERT("open video capture device", (fd = open(cap_dev, O_RDONLY)) != -1);

	if (IOCTL(fd, VIDIOC_QUERYCAP, &vcap) == -1) {
		close(fd);
		return NULL; /* Supposedly no V4L2 device, we'll try V4L */
	}

	if (vcap.type != V4L2_TYPE_CAPTURE)
		FAIL("%s ('%s') is not a capture device",
			cap_dev, vcap.name);

	if (!(vcap.flags & V4L2_FLAG_STREAMING) ||
	    !(vcap.flags & V4L2_FLAG_SELECT))
		FAIL("%s ('%s') does not support streaming/select(2),\n"
			"%s will not work with the v4l2 read(2) interface.",
			cap_dev, vcap.name, my_name);

	printv(2, "Opened %s ('%s')\n", cap_dev, vcap.name);

	
	ASSERT("query current video standard",
		IOCTL(fd, VIDIOC_G_STD, &vstd) == 0);

	frame_rate_code = ((double) vstd.framerate.denominator /
				    vstd.framerate.numerator < 29.0) ? 3 : 4;

	if (frame_rate_code == 4 && grab_height == 288)
		grab_height = 240; // XXX DAU
	if (frame_rate_code == 4 && grab_height == 576)
		grab_height = 480; // XXX DAU

	if (PROGRESSIVE(filter_mode)) {
		frame_rate_code += 3; // see frame_rate_value[]
		min_cap_buffers++;
	}

	printv(2, "Video standard is '%s'\n", vstd.name);


	if (mute != 2) {
		old_mute.id = V4L2_CID_AUDIO_MUTE;

		if (IOCTL(fd, VIDIOC_G_CTRL, &old_mute) == 0) {
			static const char *mute_options[] = { "unmuted", "muted" };
			struct v4l2_control new_mute;

			atexit(mute_restore);

			new_mute.id = V4L2_CID_AUDIO_MUTE;
			new_mute.value = !!mute;

			ASSERT("set mute control to %d",
				IOCTL(fd, VIDIOC_S_CTRL, &new_mute) == 0, !!mute);

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

		vfmt.type = V4L2_BUF_TYPE_CAPTURE;
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

		if (IOCTL(fd, VIDIOC_S_FMT, &vfmt) == 0) {
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

		if (IOCTL(fd, VIDIOC_S_FMT, &vfmt) != 0 ||
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

#if USE_PSB

	if (grab_width * grab_height < 128000) {
		int b = 12;
		IOCTL(fd, VIDIOC_PSB, &b);
	} else if (grab_width * grab_height < 256000) {
		int b = 6;
		IOCTL(fd, VIDIOC_PSB, &b);
	} else {
		int b = 2;
		IOCTL(fd, VIDIOC_PSB, &b);
	}
#endif

	vrbuf.type = V4L2_BUF_TYPE_CAPTURE;
	vrbuf.count = MAX(cap_buffers, min_cap_buffers);

	ASSERT("request capture buffers",
		IOCTL(fd, VIDIOC_REQBUFS, &vrbuf) == 0);

	if (vrbuf.count == 0)
		FAIL("No capture buffers granted");

	printv(2, "%d capture buffers granted\n", vrbuf.count);

	ASSERT("allocate capture buffers",
		(buffers = calloc(vrbuf.count, sizeof(buffer2))));

	init_callback_fifo2(&cap_fifo, "video-v4l2",
		NULL, NULL, wait_full, send_empty, 0, 0);

	ASSERT("init capture producer",
		add_producer(&cap_fifo, &cap_prod));

	cap_fifo.start = capture_on;

	// Map capture buffers

	for (i = 0; i < vrbuf.count; i++) {
		unsigned char *p;

		vbuf.type = V4L2_BUF_TYPE_CAPTURE;
		vbuf.index = i;

		printv(3, "Mapping capture buffer #%d\n", i);

		ASSERT("query capture buffer #%d",
			IOCTL(fd, VIDIOC_QUERYBUF, &vbuf) == 0, i);

		p = mmap(NULL, vbuf.length, PROT_READ, MAP_SHARED, fd, vbuf.offset);

		if ((int) p == -1) {
			if (errno == ENOMEM && i > 0)
				break;
			else
				ASSERT("map capture buffer #%d", 0, i);
		} else {
			add_buffer(&cap_fifo, buffers + i);

			buffers[i].data = p;
			buffers[i].used = vbuf.length;
		}

		ASSERT("enqueue capture buffer #%d",
			IOCTL(fd, VIDIOC_QBUF, &vbuf) == 0,
			vbuf.index);
	}

	if (i < min_cap_buffers)
		FAIL("Cannot allocate enough (%d) capture buffers", min_cap_buffers);

	return &cap_fifo;
}

#endif // V4L2