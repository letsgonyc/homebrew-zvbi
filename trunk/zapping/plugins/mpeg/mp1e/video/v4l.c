/*
 *  MPEG-1 Real Time Encoder
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

/* $Id: v4l.c,v 1.16 2001-07-26 05:41:31 mschimek Exp $ */

#include <ctype.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <asm/types.h>
#include "../common/videodev.h"
#include "../common/log.h"
#include "../common/fifo.h"
#include "../common/alloc.h"
#include "../common/math.h"
#include "../options.h"
#include "video.h"

#if 0

#define IOCTL(fd, cmd, data) (TEMP_FAILURE_RETRY(ioctl(fd, cmd, data)))

#warning The V4L interface is unsupported.

static int			fd;
static fifo 			cap_fifo;

static struct video_capability	vcap;
static struct video_channel	chan;
static struct video_mbuf	buf;
static struct video_mmap	vmmap;
static struct video_audio	vaud;

static int			vaud_flags;
static int			vaud_volume;
static int			cframe;
static unsigned long		buf_base;

static buffer *
wait_full(fifo *f)
{
	buffer *b;
	int r = -1;
	double now;
	unsigned char * data;
	int oldcframe;

	oldcframe = cframe + 1;
	if (oldcframe == f->num_buffers)
		oldcframe = 0;

	vmmap.frame = oldcframe;
	ASSERT("enqueue capture buffer",
	       IOCTL(fd, VIDIOCMCAPTURE, &vmmap) == 0);

	while (r < 0) {
		vmmap.frame = cframe;

        	r = ioctl(fd, VIDIOCSYNC, &(vmmap.frame));

		now = current_time();

		if (r < 0 && errno == EINTR)
			continue;

		ASSERT("execute video sync", r >= 0);
	}

	data = (unsigned char *)(buf_base + buf.offsets[vmmap.frame]);

	b = f->buffers + cframe;
	b->time = now;

	memcpy(b->data, data, b->size);

	cframe = oldcframe;

	return b;
}

static bool
capture_on(fifo *unused)
{
	cframe = 0;
	return TRUE;
}

static void
mute_restore(void)
{
	ASSERT("query audio capabilities",
	       IOCTL(fd, VIDIOCGAUDIO, &vaud) == 0);

	fprintf(stderr, "muting again\n");

	vaud.flags |= VIDEO_AUDIO_MUTE;
	vaud.volume = 0;

	ASSERT("Audio muting", IOCTL(fd, VIDIOCSAUDIO, &vaud) == 0);
}

fifo2 *
v4l_init(void)
{
	int aligned_width;
	int aligned_height;
	unsigned long bufsize;

	FAIL("The V4L interface doesn't work, please use V4L2 instead.");

	grab_width = width = saturate(width, 1, MAX_WIDTH);
	grab_height = height = saturate(height, 1, MAX_HEIGHT);

	aligned_width  = (width + 15) & -16;
	aligned_height = (height + 15) & -16;

	ASSERT("open capture device", (fd = open(cap_dev, O_RDONLY)) != -1);
	ASSERT("query video capture capabilities", IOCTL(fd, VIDIOCGCAP, &vcap) == 0);

	if (!(vcap.type&VID_TYPE_CAPTURE))
		FAIL("%s ('%s') is not a capture device", cap_dev, vcap.name);

	ASSERT("query audio capabilities", IOCTL(fd, VIDIOCGAUDIO, &vaud) == 0);

	vaud_flags = vaud.flags;
	vaud_volume = vaud.volume;

	vaud.flags &= ~VIDEO_AUDIO_MUTE;
	vaud.volume = 60000;

	ASSERT("Audio unmuting", IOCTL(fd, VIDIOCSAUDIO, &vaud) == 0);

	atexit(mute_restore);

	printv(2, "Opened %s ('%s')\n", cap_dev, vcap.name);

	ASSERT("query video channel", IOCTL(fd, VIDIOCGCHAN, &chan) == 0);

	if (chan.norm == 0) /* PAL */
		frame_rate_code = 3;
	else /* NTSC */
		frame_rate_code = 4;

	printv(2, "Video standard is '%s'\n", chan.norm == 0 ? "PAL" : "NTSC");

	if (frame_rate_code == 4 && grab_height == 288)
		height = aligned_height = grab_height = 240; // XXX DAU

	vmmap.width	= aligned_width;
	vmmap.height	= aligned_height;
	vmmap.format	= VIDEO_PALETTE_YUV420P;

	filter_mode = CM_YUV;
	filter_init(vmmap.width);

	ASSERT("request capture buffers", IOCTL(fd, VIDIOCGMBUF, &buf) == 0);

	if (buf.frames == 0)
		FAIL("No capture buffers granted");

	printv(2, "%d capture buffers granted\n", buf.frames);

	printv(3, "Mapping capture buffers.\n");

	buf_base=(unsigned long)mmap(NULL, buf.size, PROT_READ,
		  MAP_SHARED, fd, 0);

	ASSERT("map capture buffer", (int)buf_base != -1);

	ASSERT("init capture fifo", init_callback_fifo(
		&cap_fifo, "video-v4l",
		wait_full, NULL, buf.frames, 0));

	cap_fifo.start = capture_on;

	cap_fifo.num_buffers = 0;
	/* malloc() is needed because usually only 2 buffers are available */
	bufsize = vmmap.width * vmmap.height * 1.5;

	fprintf(stderr, "allocating %ld bytes\n", bufsize);

	for (cap_fifo.num_buffers = 0; cap_fifo.num_buffers < buf.frames;
		cap_fifo.num_buffers++) {
		cap_fifo.buffers[cap_fifo.num_buffers].data =
			cap_fifo.buffers[cap_fifo.num_buffers].allocated =
			calloc_aligned(bufsize,
				       bufsize < 4096 ? CACHE_LINE : 4096);
		cap_fifo.buffers[cap_fifo.num_buffers].size = bufsize;
		ASSERT("allocate mem for the capture buffers",
		       cap_fifo.buffers[cap_fifo.num_buffers].data);
	}

	vmmap.frame = 0;
	ASSERT("queue buffer",
	       IOCTL(fd, VIDIOCMCAPTURE, &vmmap) == 0);

	return &cap_fifo;
}

#endif

fifo2 *
v4l_init(void)
{
	FAIL("The V4L interface is out of order, please install V4L2.");

	return NULL;
}
