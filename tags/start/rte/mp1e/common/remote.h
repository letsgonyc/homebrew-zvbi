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

/* $Id: remote.h,v 1.1.1.1 2001-08-07 22:10:11 garetxe Exp $ */

#ifndef REMOTE_H
#define REMOTE_H

#include "threads.h"
#include "fifo.h"

extern struct remote {
	mucon			mucon;

	double			start_time;
	double			stop_time;
	double			front_time;

	unsigned int		modules;
	unsigned int		vote;
} remote;

extern void	remote_init(unsigned int modules);
extern bool	remote_start(double time);
extern bool	remote_stop(double time);
extern bool	remote_sync(consumer *c, unsigned int this_module, double frame_period);

static inline int
remote_break(unsigned int this_module, double time, double frame_period)
{
	pthread_mutex_lock(&remote.mucon.mutex);

	if (time >= remote.stop_time) {
		pthread_mutex_unlock(&remote.mucon.mutex);
		printv(4, "remote_break %08x, %f, stop_time %f\n",
			this_module, time, remote.stop_time);
		return TRUE;
	}

	remote.front_time = time + frame_period;

	pthread_mutex_unlock(&remote.mucon.mutex);

	return FALSE;
}

#endif // REMOTE_H