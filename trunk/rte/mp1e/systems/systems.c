/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
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

/* $Id: systems.c,v 1.5 2001-10-08 05:49:44 mschimek Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include "../video/mpeg.h"
#include "../audio/libaudio.h"
#include "../video/video.h"
#include "../common/math.h"
#include "../options.h"
#include "mpeg.h"
#include "systems.h"

void
mux_free(multiplexer *mux)
{
	stream *str;

	asserts(mux != NULL);

	while ((str = PARENT(rem_xhead(&mux->streams),
			     stream, fifo.node))) {
		destroy_fifo(&str->fifo);
		free(str);
	}

	destroy_xlist(&mux->streams);

	memset(mux, 0, sizeof(*mux));

	free(mux);
}

multiplexer *
mux_alloc(void *user_data)
{
	multiplexer *mux;

	if (!(mux = calloc(1, sizeof(*mux))))
		return NULL;

	init_xlist(&mux->streams);

	/* a) should be an argument, 
	   b) this func should init the output fifo */
	mux->packet_size = (mux_syn == 4) ? 2324 /* VCD */ : 2048;

	assert(mux->packet_size >= 512 && mux->packet_size <= 32768);

	mux->user_data = user_data;

	return mux;
}

/*
 *  stream_id	VIDEO_STREAM
 *  max_size	frame buffer
 *  buffers	1++
 *  frame_rate	24 Hz
 *  bit_rate	upper bound
 */

fifo *
mux_add_input_stream(multiplexer *mux, int stream_id, char *name,
	int max_size, int buffers, double frame_rate, int bit_rate)
{
	stream *str;
	int r;

	/*
	 *  Returns EBUSY while the mux runs.
	 */
	if ((r = pthread_rwlock_trywrlock(&mux->streams.rwlock)) != 0) {
		errno = r;
		return NULL;
	}

	if (!(str = calloc(1, sizeof(stream)))) {
		return NULL;
	}

	str->stream_id = stream_id;
	str->frame_rate = frame_rate;
	str->bit_rate = bit_rate;

	buffers = init_buffered_fifo(&str->fifo, name, buffers, max_size);

	if (!buffers) {
		free(str);
		return NULL;
	}

	add_consumer(&str->fifo, &str->cons);

	add_tail((list *) &mux->streams, &str->fifo.node);

	pthread_rwlock_unlock(&mux->streams.rwlock);

	return &str->fifo;
}

void *
stream_sink(void *muxp)
{
	multiplexer *mux = muxp;
	unsigned long long bytes_out = 0;
	int num_streams;
	buffer *buf = NULL;
	stream *str;

	pthread_cleanup_push((void (*)(void *)) pthread_rwlock_unlock, (void *) &mux->streams.rwlock);
	assert(pthread_rwlock_rdlock(&mux->streams.rwlock) == 0);

	for_all_nodes (str, &mux->streams, fifo.node)
		str->left = 1;

	num_streams = list_members((list *) &mux->streams);

	while (num_streams > 0) {
		for_all_nodes (str, &mux->streams, fifo.node)
			if (str->left && (buf = recv_full_buffer(&str->cons)))
				break;

		if (buf->used <= 0) {
			/* XXX EOF/error */
			str->left = 0;
			num_streams--;
			continue;
		}

		bytes_out += buf->used;

		send_empty_buffer(&str->cons, buf);

		if (verbose > 0) {
			double system_load = 1.0 - get_idle();

			printv(1, "%.3f MB >0, %.2f %% dropped, system load %.1f %%  %c",
				bytes_out / (double)(1 << 20),
				video_frame_count ? 100.0 * video_frames_dropped / video_frame_count : 0.0,
				100.0 * system_load, (verbose > 3) ? '\n' : '\r');

			fflush(stderr);
		}
	}

	pthread_cleanup_pop(1);

	return NULL;
}

/*
 *  NB Use MPEG-2 mux to create an elementary VBI stream.
 */

void *
elementary_stream_bypass(void *muxp)
{
	multiplexer *mux = muxp;
	unsigned long long bytes_out = 0;
	unsigned long long frame_count = 0;
	double system_load;
	stream *str;

	pthread_cleanup_push((void (*)(void *)) pthread_rwlock_unlock, (void *) &mux->streams.rwlock);
	assert(pthread_rwlock_rdlock(&mux->streams.rwlock) == 0);

	assert(list_members((list *) &mux->streams) == 1);

	str = PARENT(mux->streams.head, stream, fifo.node);

	for (;;) {
		buffer *buf;

		buf = wait_full_buffer(&str->cons);

		if (buf->used <= 0) // EOF / error
			break;

		frame_count++;
		bytes_out += buf->used;

		buf = mux_output(mux, buf);

		send_empty_buffer(&str->cons, buf);

		if (verbose > 0) {
			int min, sec;
			long long frame;

			system_load = 1.0 - get_idle();

			if (IS_VIDEO_STREAM(str->stream_id)) {
				frame = lroundn(frame_count * frame_rate / str->frame_rate);
				// exclude padding frames

				sec = frame / str->frame_rate;
				frame -= sec * str->frame_rate;
				min = sec / 60;
				sec -= min * 60;

				if (video_frames_dropped > 0)
					printv(1, "%d:%02d.%02d (%.1f MB), %.2f %% dropped, system load %.1f %%  %c",
						min, sec, (int) frame, bytes_out / (double)(1 << 20),
						100.0 * video_frames_dropped / video_frame_count,
						100.0 * system_load, (verbose > 3) ? '\n' : '\r');
				else
					printv(1, "%d:%02d.%02d (%.1f MB), system load %.1f %%    %c",
						min, sec, (int) frame, bytes_out / (double)(1 << 20),
						100.0 * system_load, (verbose > 3) ? '\n' : '\r');
			} else {
				sec = lroundn(frame_count / str->frame_rate);
				min = sec / 60;
				sec -= min * 60;

				printv(1, "%d:%02d (%.3f MB), system load %.1f %%    %c",
					min, sec, bytes_out / (double)(1 << 20),
					100.0 * system_load, (verbose > 3) ? '\n' : '\r');
			}

			fflush(stderr);
		}
	}

	pthread_cleanup_pop(1);

	return NULL;
}

double
get_idle(void)
{
	static double last_uptime = -1, last_idle;
	static double system_idle = 0.0;
	static int upd_idle = 15;
	double uptime, idle, period;
	char buf[80];
	ssize_t r;
	int fd;

	if (--upd_idle > 0)
		return system_idle;

	upd_idle = 15;

	if ((fd = open("/proc/uptime", O_RDONLY)) < 0)
		return system_idle;

	r = read(fd, buf, sizeof(buf) - 1);

	close(fd);

	if (r == -1)
		return system_idle;

	buf[r] = 0;

	sscanf(buf, "%lf %lf", &uptime, &idle);

	period = uptime - last_uptime;

	if (period > 0.5) {
		if (last_uptime >= 0.0)
			system_idle = 0.5 * (system_idle + (idle - last_idle) / period);

		last_idle = idle;
		last_uptime = uptime;
	}

	return system_idle;
}

char *
mpeg_header_name(unsigned int code)
{
	static char buf[40];

	switch (code |= 0x00000100) {
	case PICTURE_START_CODE:
		return "picture_start";
		break;

	case SLICE_START_CODE + 0 ...
	     SLICE_START_CODE + 174:
		sprintf(buf, "slice_start_%d", code & 0xFF);
		return buf;
		break;

	case 0x000001B0:
	case 0x000001B1:
	case 0x000001B6:
		sprintf(buf, "reserved_%02x", code & 0xFF);
		return buf;
		break;

	case USER_DATA_START_CODE:
		return "user_data";
		break;

	case SEQUENCE_HEADER_CODE:
		return "sequence_header";
		break;

	case SEQUENCE_ERROR_CODE:
		return "sequence_error";
		break;

	case EXTENSION_START_CODE:
		return "extension_start";
		break;

	case SEQUENCE_END_CODE:
		return "sequence_end";
		break;

	case GROUP_START_CODE:
		return "group_start";
		break;

	/* system start codes */

	case ISO_END_CODE:
		return "iso_end";
		break;

	case PACK_START_CODE:
		return "pack_start";
		break;

	case SYSTEM_HEADER_CODE:
		return "system_header";
		break;

	case PACKET_START_CODE + PROGRAM_STREAM_MAP:
		return "program_stream_map";
		break;

	case PACKET_START_CODE + PRIVATE_STREAM_1:
		return "private_stream_1";
		break;

	case PACKET_START_CODE + PADDING_STREAM:
		return "padding_stream";
		break;

	case PACKET_START_CODE + PRIVATE_STREAM_2:
		return "private_stream_2";
		break;

	case PACKET_START_CODE + AUDIO_STREAM ... 
	     PACKET_START_CODE + AUDIO_STREAM + 0x1F:
		sprintf(buf, "audio_stream_%d", code & 0x1F);
		return buf;
		break;

	case PACKET_START_CODE + VIDEO_STREAM ... 
	     PACKET_START_CODE + VIDEO_STREAM + 0x0F:
		sprintf(buf, "video_stream_%d", code & 0x0F);
		return buf;
		break;

	case PACKET_START_CODE + ECM_STREAM:
		return "ecm_stream";
		break;

	case PACKET_START_CODE + EMM_STREAM:
		return "emm_stream";
		break;

	case PACKET_START_CODE + DSM_CC_STREAM:
		return "dsm_cc_stream";
		break;

	case PACKET_START_CODE + ISO_13522_STREAM:
		return "iso_13522_stream";
		break;

	case PACKET_START_CODE + 0xF4 ...
	     PACKET_START_CODE + 0xFE:
		sprintf(buf, "reserved_stream_%02x", code & 0xFF);
		return buf;
		break;

	case PACKET_START_CODE + PROGRAM_STREAM_DIRECTORY:
		return "program_stream_directory";
		break;

	default:
		return "invalid";
		break;	
	}
}
