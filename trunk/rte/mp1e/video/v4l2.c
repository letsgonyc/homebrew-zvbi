/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999, 2000, 2001, 2002 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
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

/* $Id: v4l2.c,v 1.11 2002-01-21 13:54:53 mschimek Exp $ */

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

static int			fd;
static fifo			cap_fifo;
static producer			cap_prod;
static buffer *		        buffers;

static struct v4l2_capability	vcap;
static struct v4l2_standard	vstd;
static struct v4l2_format	vfmt;
static struct v4l2_buffer	vbuf;
static struct v4l2_requestbuffers vrbuf;

static struct v4l2_control	old_mute;

static pthread_t		thread_id;
static struct tfmem		tfmem;
static const int		syncio = TRUE;

#define VIDIOC_PSB _IOW('v', 193, int)

#undef V4L2_DROP_TEST
#undef V4L2_TF_TEST
#undef V4L2_TOD_STAMPS

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

/* We don't need this here, but I can't test v4l directly */
static inline double
timestamp1(buffer *b)
{
	static double cap_time = 0.0;
	static double frame_period_near;
	static double frame_period_far;
	double now = current_time();

	if (cap_time > 0) {
		double dt = now - cap_time;
		double ddt = frame_period_far - dt;

		if (fabs(frame_period_near) < frame_period_far * 1.5) {
			frame_period_near = (frame_period_near - dt) * 0.8 + dt;
			frame_period_far = ddt * 0.9999 + dt;
			cap_time += frame_period_far;
		} else {
			frame_period_near = frame_period_far;
			cap_time = now;
		}
#if 0
		printv(0, "now %f dt %+f ddt %+f n %+f f %+f\n",
		       now, dt, ddt, frame_period_near, frame_period_far);
#endif
	} else {
		cap_time = now;
		frame_period_near = tfmem.ref;
		frame_period_far = tfmem.ref;
	}

	return cap_time;
}

static inline double
timestamp2(buffer *b)
{
	static double cap_time = 0.0;
	static double dt_acc;
	double now = current_time();

	if (cap_time > 0) {
		double dt = now - cap_time;

		dt_acc += (dt - dt_acc) * 0.1;

		if (dt_acc > tfmem.ref * 1.5) {
			/* bah. */
#if 0
			printv(0, "video dropped %f > %f * 1.5\n",
			       dt_acc, tfmem.ref);
#endif
			tfmem.err -= dt_acc;
			cap_time = now;
			dt_acc = tfmem.ref;
		} else {
			cap_time += mp1e_timestamp_filter
				(&tfmem, dt, 0.001, 1e-7, 0.1);
		}
#if 0
		printv(0, "now %f dt %+f dta %+f err %+f t/b %+f\n",
		       now, dt, dt_acc, tfmem.err, tfmem.ref);
#endif
	} else {
		cap_time = now;
		dt_acc = tfmem.ref;
	}

	return cap_time;
}

/*
 *  Synchronous I/O
 */

static bool
capture_on(fifo *unused)
{
	int str_type = V4L2_BUF_TYPE_CAPTURE;

	return IOCTL(fd, VIDIOC_STREAMON, &str_type) == 0;
}

static void
wait_full(fifo *f)
{
	struct v4l2_buffer vbuf;
	struct timeval tv;
	fd_set fds;
	buffer *b;
	int r = -1;

#ifdef V4L2_DROP_TEST
drop:
#endif
	for (r = -1; r <= 0;) {
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

#ifdef V4L2_DROP_TEST
	if ((rand() % 100) > 98) {
		ASSERT("enqueue capture buffer",
		       IOCTL(fd, VIDIOC_QBUF, &vbuf) == 0);
		fprintf(stderr, "video drop\n");
		goto drop;
	}
#endif
	b = buffers + vbuf.index;

#if defined(V4L2_TF_TEST)
	b->time = timestamp2(b);
#elif defined(V4L2_TOD_STAMPS)
	b->time = current_time();
#else
	b->time = vbuf.timestamp * (1 / 1e9); // UST, currently TOD
#endif
	send_full_buffer(&cap_prod, b);
}

/* Attention buffers are returned out of order */

static void
send_empty(consumer *c, buffer *b)
{
	struct v4l2_buffer vbuf;

	// XXX
	unlink_node(&c->fifo->full, &b->node);

	vbuf.type = V4L2_BUF_TYPE_CAPTURE;
	vbuf.index = b - buffers;

	ASSERT("enqueue capture buffer",
		IOCTL(fd, VIDIOC_QBUF, &vbuf) == 0);
}

/*
 *  Asynchronous I/O
 */

static void *
v4l2_cap_thread(void *unused)
{
	int str_type = V4L2_BUF_TYPE_CAPTURE;

	ASSERT("start capturing",
	       IOCTL(fd, VIDIOC_STREAMON, &str_type) == 0);

	printv(0, "TEST - v4l2 thread\n");

	for (;;) {
		struct v4l2_buffer vbuf;
		struct timeval tv;
		fd_set fds;
		buffer *b;
		int r = -1;

		b = wait_empty_buffer(&cap_prod);

		for (r = -1; r <= 0;) {
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

		b->time = timestamp2(b);

		b->used = buffers[vbuf.index].used;
		memcpy(b->data, buffers[vbuf.index].data, b->used);

		ASSERT("enqueue capture buffer",
		       IOCTL(fd, VIDIOC_QBUF, &vbuf) == 0);

		send_full_buffer(&cap_prod, b);
	}

	return NULL;
}

/*
 *  Initialization 
 */

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

fifo *
v4l2_init(double *frame_rate)
{
	int aligned_width;
	int aligned_height;
	unsigned int probed_modes = 0;
	int mod;
	int min_cap_buffers = video_look_ahead(gop_sequence);
	int i;

	ASSERT("open video capture device",
	       (fd = open(cap_dev, O_RDWR)) != -1);

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
			cap_dev, vcap.name, program_invocation_short_name);

	printv(2, "Opened %s ('%s')\n", cap_dev, vcap.name);

	
	ASSERT("query current video standard",
		IOCTL(fd, VIDIOC_G_STD, &vstd) == 0);

	*frame_rate = (double) vstd.framerate.denominator /
				vstd.framerate.numerator;
	mp1e_timestamp_init(&tfmem, 1.0 / *frame_rate);

	if (*frame_rate > 29.0 && grab_height == 288)
		grab_height = 240;
	if (*frame_rate > 29.0 && grab_height == 576)
		grab_height = 480;

	if (PROGRESSIVE(filter_mode)) {
		FAIL("Sorry, progressive mode out of order\n");
		min_cap_buffers++;
	}

	printv(2, "Video standard is '%s' (%5.2f Hz)\n",
		vstd.name, *frame_rate);

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
		int new_mode, new_height;

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

		new_height = aligned_height;

		if (filter_mode == CM_YUYV)
			new_mode = CM_YUV;
		else if (filter_mode == CM_YUYV_VERTICAL_DECIMATION) {
			new_mode = CM_YUYV_VERTICAL_INTERPOLATION;
			new_height = (grab_height + 15) & -16;
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
		aligned_height = new_height;
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
		(buffers = calloc(vrbuf.count, sizeof(buffer))));

	if (syncio) {
		init_callback_fifo(&cap_fifo, "video-v4l2",
				   NULL, NULL, wait_full, send_empty, 0, 0);

		ASSERT("init capture producer",
		       add_producer(&cap_fifo, &cap_prod));

		cap_fifo.start = capture_on;
	}

	// Map capture buffers

	for (i = 0; i < vrbuf.count; i++) {
		unsigned char *p;

		vbuf.type = V4L2_BUF_TYPE_CAPTURE;
		vbuf.index = i;

		printv(3, "Mapping capture buffer #%d\n", i);

		ASSERT("query capture buffer #%d",
			IOCTL(fd, VIDIOC_QUERYBUF, &vbuf) == 0, i);

		p = mmap(NULL, vbuf.length, PROT_READ | PROT_WRITE,
			 MAP_SHARED, fd, vbuf.offset);

		if ((int) p == -1) {
			if (errno == ENOMEM && i > 0)
				break;
			else
				ASSERT("map capture buffer #%d", 0, i);
		} else {
			if (syncio)
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

	if (!syncio) {
		ASSERT("initialize v4l2-async fifo",
		       init_buffered_fifo(&cap_fifo, "video-v4l2-async",
					  i, buffers[0].used));

		ASSERT("init v4l2-async capture producer",
		       add_producer(&cap_fifo, &cap_prod));

		ASSERT("create v4l2-async capture thread",
		       !pthread_create(&thread_id, NULL,
				       v4l2_cap_thread, NULL));

		printv(2, "V4L2 capture thread launched\n");
	}

	return &cap_fifo;
}
