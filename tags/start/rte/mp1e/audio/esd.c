/*
 *  MPEG-1 Real Time Encoder
 *  ESD [Enlightened Sound Daemon] interface
 *
 *  Copyright (C) 2000 I�aki G.E.
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

/* $Id: esd.c,v 1.1.1.1 2001-08-07 22:09:44 garetxe Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <linux/soundcard.h>

#include "../common/log.h" 
#include "../common/mmx.h" 
#include "../common/math.h" 
#include "../common/alloc.h"
#include "audio.h"

#ifdef USE_ESD

#include <esd.h>

#define BUFFER_SIZE (ESD_BUF_SIZE) // bytes per read(), appx.

struct esd_context {
	struct pcm_context	pcm;

	int			socket;
	int			scan_range;
	int			look_ahead;
	int			samples_per_frame;
	short *			p;
	int			left;
	double			time;
};

static void
wait_full(fifo2 *f)
{
	struct esd_context *esd = f->user_data;
	buffer2 *b = PARENT(f->buffers.head, buffer2, added);

	assert(b->data == NULL); /* no queue */

	if (esd->left <= 0) {
		struct timeval tv;
		unsigned char *p;
		ssize_t r, n;

		memcpy(b->allocated, (short *) b->allocated + esd->scan_range,
			esd->look_ahead * sizeof(short));

		p = b->allocated + esd->look_ahead * sizeof(short);
		n = esd->scan_range * sizeof(short);

		while (n > 0) {
			fd_set rdset;
			int err;

			FD_ZERO(&rdset);
			FD_SET(esd->socket, &rdset);
			tv.tv_sec = 1;
			tv.tv_usec = 0;
			err = select(esd->socket+1, &rdset,
				   NULL, NULL, &tv);

			if ((err == -1) || (err == 0))
				continue;

			r = read(esd->socket, p, n);
			
			if (r < 0 && errno == EINTR)
				continue;

			if (r == 0) {
				memset(p, 0, n);
				break;
			}

			ASSERT("read PCM data, %d bytes", r > 0, n);

			p += r;
			n -= r;
		}

		esd->time = current_time()
			- ((esd->scan_range - n / sizeof(short)) >> esd->pcm.stereo)
				/ (double) esd->pcm.sampling_rate;

		esd->p = (short *) b->allocated;
		esd->left = esd->scan_range - esd->samples_per_frame;

		b->time = esd->time;
		b->data = b->allocated;

		send_full_buffer2(&esd->pcm.producer, b);
		return;
	}

	b->time = esd->time
		+ ((esd->p - (short *) b->allocated) >> esd->pcm.stereo)
			/ (double) esd->pcm.sampling_rate;

	esd->p += esd->samples_per_frame;
	esd->left -= esd->samples_per_frame;

	b->data = (unsigned char *) esd->p;

	send_full_buffer2(&esd->pcm.producer, b);
}

static void
send_empty(consumer *c, buffer2 *b)
{
	// XXX
	rem_node3(&c->fifo->full, &b->node);
	b->data = NULL;
}

fifo2 *
open_pcm_esd(char *unused, int sampling_rate, bool stereo)
{
	struct esd_context *esd;
	esd_format_t format;
	int buffer_size;
	buffer2 *b;

	ASSERT("allocate pcm context",
		(esd = calloc(1, sizeof(struct esd_context))));

	esd->pcm.sampling_rate = sampling_rate;
	esd->pcm.stereo = stereo;

	esd->samples_per_frame = SAMPLES_PER_FRAME << stereo;
	esd->scan_range = MAX(BUFFER_SIZE / sizeof(short) / esd->samples_per_frame, 1) * esd->samples_per_frame;
	esd->look_ahead = (512 - 32) << stereo;

	buffer_size = (esd->scan_range + esd->look_ahead) * sizeof(short);

	format = ESD_STREAM | ESD_RECORD | ESD_BITS16;

	if (stereo)
		format |= ESD_STEREO;
	else
		format |= ESD_MONO;

	esd->socket =
		esd_record_stream_fallback(format, sampling_rate, NULL, NULL);

	if (esd->socket <= 0)
		FAIL("Couldn't create esd recording socket");

	printv(2, "Opened ESD socket\n");

	ASSERT("init esd fifo",	init_callback_fifo2(
		&esd->pcm.fifo, "audio-esd",
		NULL, NULL, wait_full, send_empty,
		1, buffer_size));

	ASSERT("init esd producer",
		add_producer(&esd->pcm.fifo, &esd->pcm.producer));

	esd->pcm.fifo.user_data = esd;

	b = PARENT(esd->pcm.fifo.buffers.head, buffer2, added);

	b->data = NULL;
	b->used = (esd->samples_per_frame + esd->look_ahead) * sizeof(short);

	return &esd->pcm.fifo;
}

#else // !USE_ESD

fifo2 *
open_pcm_esd(char *dev_name, int sampling_rate, bool stereo)
{
	FAIL("Not compiled with ESD interface.\n"
	     "More about ESD at http://www.tux.org/~ricdude/EsounD.html\n");
}

#endif // !USE_ESD