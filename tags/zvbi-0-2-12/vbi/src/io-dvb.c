#if 0
#error untested

/*
 *  libzvbi -- dvb driver interface
 *
 *  (c) 2003 Gerd Knorr <kraxel@bytesex.org> [SUSE Labs]
 *  (c) 2004 Michael H. Schimek (vbi_dvb_demux, new dvb_read)
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef ENABLE_DVB

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <inttypes.h>

#include <sys/select.h>
#include <sys/ioctl.h>
#include "dvb/frontend.h"
#include "dvb/dmx.h"
#include "hamm.h"
#include "io.h"
#include "vbi.h"
#include "dvb_demux.h"

/* ----------------------------------------------------------------------- */


typedef struct {
    vbi_capture		cap;
    vbi_dvb_demux *	demux;
    int 		fd;
    int     	        debug;
    vbi_capture_buffer	sliced_buffer;
    vbi_sliced		sliced_data[128];
    double		sample_time;
    uint8_t		pes_buffer[1024*8];
    const uint8_t *	bp;
    unsigned int	b_left;
    int64_t		last_pts;
    vbi_bool		bug_compatible;
} vbi_capture_dvb;

/* ----------------------------------------------------------------------- */

static vbi_capture_dvb* dvb_init(const char *dev, char **errstr, int debug)
{
    vbi_capture_dvb *dvb;

    dvb = malloc(sizeof(*dvb));
    if (NULL == dvb) {
        vbi_asprintf(errstr, _("Virtual memory exhausted."));
        errno = ENOMEM;
	return NULL;
    }
    CLEAR (*dvb);

    if (!(dvb->demux = vbi_dvb_pes_demux_new (NULL, NULL))) {
	vbi_asprintf(errstr, _("Virtual memory exhausted."));
	errno = ENOMEM;
        free (dvb);
	return NULL;
    }

    dvb->debug = debug;
    dvb->fd = open(dev, O_RDWR | O_NONBLOCK);
    if (-1 == dvb->fd) {
	vbi_asprintf(errstr, _("Cannot open '%s': %d, %s."),
		     dev, errno, strerror(errno));
	free(dvb);
	return NULL;
    }
    if (dvb->debug)
	fprintf(stderr,"dvb-vbi: opened device %s\n",dev);
    return dvb;
}

/* ----------------------------------------------------------------------- */

static __inline__ void
timeval_subtract		(struct timeval *	delta,
				 const struct timeval *	tv1,
				 const struct timeval *	tv2)
{
	if (tv1->tv_usec < tv2->tv_usec) {
		delta->tv_sec = tv1->tv_sec - tv2->tv_sec - 1;
		delta->tv_usec = 1000000 + tv1->tv_usec - tv2->tv_usec;
	} else {
		delta->tv_sec = tv1->tv_sec - tv2->tv_sec;
		delta->tv_usec = tv1->tv_usec - tv2->tv_usec;
	}
}

static void
timeout_subtract_elapsed	(struct timeval *	result,
				 const struct timeval *	timeout,
				 const struct timeval *	now,
				 const struct timeval *	start)
{
	struct timeval elapsed;

	timeval_subtract (&elapsed, now, start);

	if ((elapsed.tv_sec | elapsed.tv_usec) > 0) {
		timeval_subtract (result, timeout, &elapsed);

		if ((result->tv_sec | result->tv_usec) < 0) {
			result->tv_sec = 0;
			result->tv_usec = 0;
		}
	} else {
		*result = *timeout;
	}
}

static ssize_t
select_read			(vbi_capture_dvb *	dvb,
				 struct timeval *	now,
				 const struct timeval *	start,
				 const struct timeval *	timeout)
{
	struct timeval tv;
	ssize_t actual;

 select_again:
	timeout_subtract_elapsed (&tv, timeout, now, start);

	/* Shortcut. */
	if ((tv.tv_sec | tv.tv_usec) > 0) {
		fd_set set;
		int r;

 select_again_with_timeout:
		FD_ZERO (&set);
		FD_SET (dvb->fd, &set);

		/* Note Linux select() may change tv. */
		r = select (dvb->fd + 1,
			    /* readable */ &set,
			    /* writeable */ NULL,
			    /* in exception */ NULL,
			    &tv);

		switch (r) {
		case -1: /* error */
			switch (errno) {
			case EINTR:
				gettimeofday (now, /* timezone */ NULL);
				goto select_again;

			default:
				return -1; /* error */
			}

		case 0: /* timeout */
			if (dvb->bug_compatible)
				return -1; /* error */

			return 0; /* timeout */

		default:
			break;
		}
	}

 read_again:
	/* Non-blocking. */
	actual = read (dvb->fd,
		       dvb->pes_buffer,
		       sizeof (dvb->pes_buffer));

	switch (actual) {
	case -1: /* error */
		switch (errno) {
		case EAGAIN:
			if (dvb->bug_compatible)
				return -1; /* error */

			if ((timeout->tv_sec | timeout->tv_usec) <= 0)
				return 0; /* timeout */

			gettimeofday (now, /* timezone */ NULL);
			timeout_subtract_elapsed (&tv, timeout, now, start);

			if ((tv.tv_sec | tv.tv_usec) <= 0)
				return 0; /* timeout */

			goto select_again_with_timeout;

		case EINTR:
			goto read_again;

		default:
			return -1; /* error */
		}

	case 0: /* EOF? */
		if (dvb->debug)
			fprintf (stderr, "dvb-vbi: end of file\n");

		errno = 0;

		return -1; /* error */

	default:
		break;
	}

	return actual;
}

static int
dvb_read			(vbi_capture *		cap,
				 vbi_capture_buffer **	raw,
				 vbi_capture_buffer **	sliced,
				 const struct timeval *	timeout)
{
	vbi_capture_dvb *dvb = PARENT (cap, vbi_capture_dvb, cap);
	vbi_capture_buffer *sb;
	struct timeval start;
	struct timeval now;
	unsigned int n_lines;
	int64_t pts;

	if (!sliced || !(sb = *sliced)) {
		sb = &dvb->sliced_buffer;
		sb->data = dvb->sliced_data;
	}

	start.tv_sec = 0;
	start.tv_usec = 0;

	/* When timeout is zero elapsed time doesn't matter. */
	if ((timeout->tv_sec | timeout->tv_usec) > 0)
		gettimeofday (&start, /* timezone */ NULL);

	now = start;

	for (;;) {
		if (0 == dvb->b_left) {
			ssize_t actual;

			actual = select_read (dvb, &now, &start, timeout);
			if (actual <= 0)
				return actual;

			gettimeofday (&now, /* timezone */ NULL);

			/* XXX inaccurate. Should be the time when we
			   received the first byte of the first packet
			   containing data of the returned frame. Or so. */
			dvb->sample_time = now.tv_sec
				+ now.tv_usec * (1 / 1e6);

			dvb->bp = dvb->pes_buffer;
			dvb->b_left = actual;
		}

		/* Demultiplexer coroutine. Returns when one frame is complete
		   or the buffer is empty, advancing bp and b_left. Don't
		   change sb->data in flight. */
		/* XXX max sliced lines needs an API change. Currently this
		   value is determined by vbi_raw_decoder line count below,
		   256 / 2 because fields don't really apply here and in
		   practice even 32 should be enough. */
		n_lines = _vbi_dvb_demux_cor (dvb->demux,
					      sb->data,
					      /* max sliced lines */ 128,
					      &pts,
					      &dvb->bp,
					      &dvb->b_left);
		if (n_lines > 0)
			break;

		if (dvb->bug_compatible) {
			/* Only one read(), timeout ignored. */
			return 0; /* timeout */
		} else {
			/* Read until EAGAIN, another error or the
			   timeout expires, in this order. */
		}
	}

	if (sliced) {
		sb->size = n_lines * sizeof (vbi_sliced);
		sb->timestamp = dvb->sample_time;

		/* XXX PTS needs an API change.
		   sb->sample_time = dvb->sample_time;
		   sb->stream_time = pts; (first sliced line) */
		dvb->last_pts = pts;

		*sliced = sb;
	}

	if (raw && *raw) {
		/* Not implemented yet. */
		sb = *raw;
		sb->size = 0;
	}

	return 1; /* success */
}

static vbi_raw_decoder* dvb_parameters(vbi_capture *cap)
{
    static vbi_raw_decoder raw = {
	.count = { 128, 128 },
    };
    return &raw;
}

static void
dvb_delete(vbi_capture *cap)
{
    vbi_capture_dvb *dvb = PARENT (cap, vbi_capture_dvb, cap);

    if (dvb->fd != -1)
	close(dvb->fd);

    _vbi_dvb_demux_delete (dvb->demux);

    /* Make unusable. */
    CLEAR (*dvb);

    free(dvb);
}

static int
dvb_fd(vbi_capture *cap)
{
    vbi_capture_dvb *dvb = PARENT (cap, vbi_capture_dvb, cap);
    return dvb->fd;
}

/* ----------------------------------------------------------------------- */
/* public interface                                                        */

int64_t
vbi_capture_dvb_last_pts	(const vbi_capture *	cap)
{
	const vbi_capture_dvb *dvb = CONST_PARENT (cap, vbi_capture_dvb, cap);

	return dvb->last_pts;
}

int vbi_capture_dvb_filter(vbi_capture *cap, int pid)
{
    vbi_capture_dvb *dvb = PARENT (cap, vbi_capture_dvb, cap);
    struct dmx_pes_filter_params filter;

    CLEAR (filter);
    filter.pid = pid;
    filter.input = DMX_IN_FRONTEND;
    filter.output = DMX_OUT_TAP;
    filter.pes_type = DMX_PES_OTHER;
    filter.flags = DMX_IMMEDIATE_START;
    if (0 != ioctl(dvb->fd, DMX_SET_PES_FILTER, &filter)) {
	if (dvb->debug)
	    perror("ioctl DMX_SET_PES_FILTER");
	return -1;
    }
    if (dvb->debug)
	fprintf(stderr,"dvb-vbi: filter setup done | fd %d pid %d\n",
		dvb->fd, pid);
    return 0;
}

vbi_capture *
vbi_capture_dvb_new2		(const char *		device_name,
				 unsigned int		pid,
				 char **		errstr,
				 vbi_bool		trace)
{
    char *error = NULL;
    vbi_capture_dvb *dvb;

    if (!errstr)
	    errstr = &error;
    *errstr = NULL;

    dvb = dvb_init(device_name,errstr,trace);
    if (NULL == dvb)
	goto failure;

    dvb->cap.parameters = dvb_parameters;
    dvb->cap.read       = dvb_read;
    dvb->cap.get_fd     = dvb_fd;
    dvb->cap._delete    = dvb_delete;

    if (0 != pid) {
	if (-1 == vbi_capture_dvb_filter (&dvb->cap, pid)) {
	    vbi_asprintf (errstr, "DMX_SET_PES_FILTER: %s",
			  strerror (errno));
	    dvb_delete (&dvb->cap);
	    goto failure;
	}
    }

    if (errstr == &error) {
	free (error);
	error = NULL;
    }

    return &dvb->cap;

 failure:
    if (errstr == &error) {
	free (error);
	error = NULL;
    }

    return NULL;
}

vbi_capture*
vbi_capture_dvb_new(char *dev, int scanning,
		    unsigned int *services, int strict,
		    char **errstr, vbi_bool trace)
{
	char *error = NULL;
	vbi_capture *cap;

	if (!errstr)
		errstr = &error;
	*errstr = NULL;

	if ((cap = vbi_capture_dvb_new2 (dev, 0, errstr, trace))) {
		vbi_capture_dvb *dvb = PARENT (cap, vbi_capture_dvb, cap);
		dvb->bug_compatible = TRUE;
	}

	if (errstr == &error) {
		free (error);
		error = NULL;
	}

	return cap;
}

#else /* !ENABLE_DVB */

#include "io.h"
#include "vbi.h"

/**
 * @param cap Initialized DVB vbi_capture context.
 *
 * @since 0.2.10
 *
 * Returns the presentation time stamp associated with the data
 * last read from the context. The PTS refers to the first sliced
 * VBI line, not the last packet containing data of that frame.
 *
 * Note timestamps returned by vbi_capture read functions contain
 * the sampling time of the data, that is the time at which the
 * packet containing the first sliced line arrived.
 *
 * @returns
 * Presentation time stamp (33 bits).
 *
 * @bug PTS' should be part of the generic I/O interface.
 */
int64_t
vbi_capture_dvb_last_pts	(const vbi_capture *	cap)
{
	return 0;
}

/**
 * @param cap Initialized DVB vbi_capture context.
 * @param pid Filter out a stream with this PID.
 *
 * Programs the DVB device transport stream demultiplexer to filter
 * out PES packets with this PID.
 * 
 * @returns
 * -1 on failure, 0 on success.
 */
int
vbi_capture_dvb_filter		(vbi_capture *		cap,
				 int			pid)
{
	return -1;
}

/**
 * @param device_name Name of the DVB device to open.
 * @param pid Filter out a stream with this PID. You can pass 0 here
 *   and set or change the PID later with vbi_capture_dvb_filter().
 * @param errstr If not @c NULL the function stores a pointer to an error
 *   description here. You must free() this string when no longer needed.
 * @param trace If @c TRUE print progress and warning messages on stderr.
 *
 * @since 0.2.10
 *
 * Initializes a vbi_capture context reading from a Linux DVB device.
 * 
 * @returns
 * Initialized vbi_capture context, @c NULL on failure.
 */
vbi_capture *
vbi_capture_dvb_new2		(const char *		device_name,
				 unsigned int		pid,
				 char **		errstr,
				 vbi_bool		trace)
{
	if (errstr)
		vbi_asprintf (errstr, _("DVB interface not compiled."));

	return NULL;
}

/**
 * @param dev Name of the DVB device to open.
 * @param scanning Ignored.
 * @param services Ignored.
 * @param strict Ignored.
 * @param errstr If not @c NULL the function stores a pointer to an error
 *   description here. You must free() this string when no longer needed.
 * @param trace If @c TRUE print progress and warning messages on stderr.
 * 
 * @deprecated
 * This function is deprecated. Use vbi_capture_dvb_new2() instead.
 *
 * Initializes a vbi_capture context reading from a Linux DVB device.
 * You must call vbi_capture_dvb_filter() before you can read.
 *
 * @returns
 * Initialized vbi_capture context, @c NULL on failure.
 *
 * @bug
 * This function ignores the scanning, services and strict parameter.
 * The read method of this DVB capture context returns -1 on timeout
 * instead of 0. It returns 0 when a single read() does not complete
 * a frame, regardless if the timeout expired. (Decoding resumes with
 * the next call.) Older versions pass select or read EINTR errors
 * back to the caller. They may return partial frames when VBI data
 * of one frame is distributed across multiple PES packets. They will
 * not return VPS, CC, or WSS data and can malfunction or segfault
 * given unusual PES streams. On error and select timeout older versions
 * invariably print a warning on stderr.
 */
vbi_capture*
vbi_capture_dvb_new(char *dev, int scanning,
		    unsigned int *services, int strict,
		    char **errstr, vbi_bool trace)
{
	if (errstr)
		vbi_asprintf (errstr, _("DVB interface not compiled."));

	return NULL;
}

#endif /* !ENABLE_DVB */

#else /* !untested ######################################################### */

/*
 *  libzvbi -- dvb driver interface
 *
 *  (c) 2003 Gerd Knorr <kraxel@bytesex.org> [SUSE Labs]
 *  Modified by Michael H. Schimek
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef ENABLE_DVB

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <inttypes.h>

#include <sys/select.h>
#include <sys/ioctl.h>
#include "dvb/frontend.h"
#include "dvb/dmx.h"
#include "hamm.h"
#include "io.h"
#include "vbi.h"

/* ----------------------------------------------------------------------- */


struct vbi_capture_dvb {
    vbi_capture	   cap;
    int            fd;
    int            debug;
    int            resync;
    unsigned char  buffer[1024*8];
    unsigned int   bbytes;
    unsigned int   psize;
};

/* ----------------------------------------------------------------------- */

static int dvb_payload(struct vbi_capture_dvb *dvb,
		       unsigned char *buf, unsigned int len,
		       vbi_sliced *dest)
{
    int i,j,line;
    int slices = 0;

    if (dvb->debug > 1)
	fprintf(stderr,"dvb-vbi: new PES packet\n");
    if (buf[0] < 0x10 || buf[0] > 0x1f)
	return slices;  /* no EBU teletext data */

    for (i = 1; i < len; i += buf[i+1]+2) {
	line = buf[i+2] & 0x1f;
	if (buf[i+2] & 0x20)
	    line += 312;
	if (dvb->debug > 2)
	    fprintf(stderr,"dvb-vbi: id 0x%02x len %d | line %3d framing 0x%02x\n",
		    buf[i], buf[i+1], line, buf[i+3]);
	switch (buf[i]) {
	case 0x02:
	    dest[slices].id   = VBI_SLICED_TELETEXT_B;
	    dest[slices].line = line;
	    for (j = 0; j < sizeof(dest[slices].data) && j < buf[i+1]-2; j++)
		dest[slices].data[j] = vbi_rev8 (buf[i+j+4]);
	    slices++;
	    break;
	}
    }
    return slices;
}

static int dvb_wait(int fd, struct timeval *timeout)
{
    struct timeval tv = *timeout;
    int rc;
    
    fd_set set;
    FD_ZERO(&set);
    FD_SET(fd,&set);
    rc = select(fd+1,&set,NULL,NULL,&tv);
    switch (rc) {
    case -1:
	perror("dvb-vbi: select");
	return -1;
    case 0:
	fprintf(stderr,"dvb-vbi: timeout\n");
	return -1;
    default:
	return 0;
    }
}

static struct vbi_capture_dvb* dvb_init(char *dev, char **errstr, int debug)
{
    struct vbi_capture_dvb *dvb;

    dvb = malloc(sizeof(*dvb));
    if (NULL == dvb) {
        vbi_asprintf(errstr, _("Virtual memory exhausted."));
        errno = ENOMEM;
	return NULL;
    }
    memset(dvb,0,sizeof(*dvb));

    dvb->debug = debug;
    dvb->fd = open(dev, O_RDWR | O_NONBLOCK);
    if (-1 == dvb->fd) {
	vbi_asprintf(errstr, _("Cannot open '%s': %d, %s."),
		     dev, errno, strerror(errno));
	free(dvb);
	return NULL;
    }
    if (dvb->debug)
	fprintf(stderr,"dvb-vbi: opened device %s\n",dev);
    return dvb;
}

/* ----------------------------------------------------------------------- */

static int dvb_read(vbi_capture *cap,
		    vbi_capture_buffer **raw,
		    vbi_capture_buffer **sliced,
		    const struct timeval *timeout)
{
    struct vbi_capture_dvb *dvb = (struct vbi_capture_dvb*)cap;
    static unsigned char peshdr[4] = { 0x00, 0x00, 0x01, 0xbd };
    vbi_sliced *dest = (vbi_sliced *)(*sliced)->data;
    unsigned int off;
    int lines = 0;
    int ret = 0;
    int rc;

    if (0 != dvb_wait(dvb->fd,timeout))
	return -1;
    rc = read(dvb->fd, dvb->buffer + dvb->bbytes,
	      sizeof(dvb->buffer) - dvb->bbytes);
    switch (rc) {
    case -1:
	perror("read");
	return -1;
    case 0:
	fprintf(stderr,"EOF\n");
	return -1;
    };
    if (dvb->debug > 1)
	fprintf(stderr,"dvb-vbi: read %d bytes (@%d)\n",rc,dvb->bbytes);
    dvb->bbytes += rc;

    if (dvb->resync) {
	/* grep for start code in resync mode */
	unsigned char *start;
	start = memmem(dvb->buffer,dvb->bbytes,peshdr,4);
	if (NULL != start) {
	    if (dvb->debug)
		fprintf(stderr,"vbi-dvb: start code found, RESYNC cleared\n");
	    dvb->bbytes -= (start - dvb->buffer);
	    memmove(dvb->buffer,start,dvb->bbytes);
	    dvb->resync = 0;
	} else {
	    if (dvb->debug)
		fprintf(stderr,"vbi-dvb: RESYNC [still no pes start magic]\n");
	    dvb->bbytes = 0;
	    dvb->resync = 1;
	    return ret;
	}
    }

    for (;;) {
	if (6 > dvb->bbytes) {
	    /* need header */
	    return ret;
	}
	if (0 != memcmp(dvb->buffer,peshdr,4)) {
	    if (dvb->debug)
		fprintf(stderr,"vbi-dvb: RESYNC [no pes start magic]\n");
	    dvb->bbytes = 0;
	    dvb->resync = 1;
	    return ret;
	}
	dvb->psize = dvb->buffer[4] << 8 | dvb->buffer[5];
	if (dvb->psize+6 > sizeof(dvb->buffer)) {
	    if (dvb->debug)
		fprintf(stderr,"vbi-dvb: RESYNC [insane payload size %d]\n",dvb->psize);
	    dvb->bbytes = 0;
	    dvb->resync = 1;
	    return ret;
	}
	if (dvb->psize+6 > dvb->bbytes) {
	    /* need more data */
	    return ret;
	}

	/* ok, have a complete PES packet, pass it on ... */
	off = dvb->buffer[8]+9;
	rc = dvb_payload(dvb,dvb->buffer+off,dvb->psize-off, dest + lines);
	if (rc > 0) {
	    struct timeval tv;
	    ret = 1;
	    lines += rc;
	    gettimeofday(&tv, NULL);
	    (*sliced)->size      = lines * sizeof(vbi_sliced);
	    (*sliced)->timestamp = tv.tv_sec + tv.tv_usec * (1 / 1e6);
	}

	dvb->bbytes -= (dvb->psize+6);
	if (dvb->bbytes) {
	    /* shift buffer */
	    memmove(dvb->buffer,dvb->buffer + dvb->psize+6,dvb->bbytes);
	}
    }
}

static vbi_raw_decoder* dvb_parameters(vbi_capture *cap)
{
    static vbi_raw_decoder raw = {
	.count = { 128, 128 },
    };
    return &raw;
}

static void
dvb_delete(vbi_capture *cap)
{
    struct vbi_capture_dvb *dvb = (struct vbi_capture_dvb*)cap;

    if (dvb->fd != -1)
	close(dvb->fd);
    free(dvb);
}

static int
dvb_fd(vbi_capture *cap)
{
    struct vbi_capture_dvb *dvb = (struct vbi_capture_dvb*)cap;
    return dvb->fd;
}

/* ----------------------------------------------------------------------- */
/* public interface                                                        */

int vbi_capture_dvb_filter(vbi_capture *cap, int pid)
{
    struct vbi_capture_dvb *dvb = (struct vbi_capture_dvb*)cap;
    struct dmx_pes_filter_params filter;

    memset(&filter, 0, sizeof(filter));
    filter.pid = pid;
    filter.input = DMX_IN_FRONTEND;
    filter.output = DMX_OUT_TAP;
    filter.pes_type = DMX_PES_OTHER;
    filter.flags = DMX_IMMEDIATE_START;
    if (0 != ioctl(dvb->fd, DMX_SET_PES_FILTER, &filter)) {
	perror("ioctl DMX_SET_PES_FILTER");
	return -1;
    }
    if (dvb->debug)
	fprintf(stderr,"dvb-vbi: filter setup done | fd %d pid %d\n",
		dvb->fd, pid);
    return 0;
}

vbi_capture*
vbi_capture_dvb_new(char *dev, int scanning,
		    unsigned int *services, int strict,
		    char **errstr, vbi_bool trace)
{
    char *error = NULL;
    struct vbi_capture_dvb *dvb;

    if (!errstr)
	errstr = &error;
    *errstr = NULL;

    dvb = dvb_init(dev,errstr,trace);
    if (NULL == dvb)
	goto failure;

    dvb->cap.parameters = dvb_parameters;
    dvb->cap.read       = dvb_read;
    dvb->cap.get_fd     = dvb_fd;
    dvb->cap._delete    = dvb_delete;

    if (errstr == &error) {
	free (error);
	error = NULL;
    }

    return &dvb->cap;

 failure:
    if (errstr == &error) {
	free (error);
	error = NULL;
    }

    return NULL;
}

#else /* !ENABLE_DVB */

#include "io.h"
#include "vbi.h"

int vbi_capture_dvb_filter(vbi_capture *cap, int pid)
{
	return -1;
}

vbi_capture*
vbi_capture_dvb_new(char *dev, int scanning,
		    unsigned int *services, int strict,
		    char **errstr, vbi_bool trace)
{
	if (errstr)
		vbi_asprintf(errstr, ("DVB interface not compiled."));

	return NULL;
}

#endif /* !ENABLE_DVB */

#endif /* !untested */