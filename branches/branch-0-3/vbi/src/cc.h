/*
 *  libzvbi - Closed Caption decoder
 *
 *  Copyright (C) 2000, 2001, 2002 Michael H. Schimek
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

/* $Id: cc.h,v 1.2.2.4 2004-04-05 04:42:26 mschimek Exp $ */

#ifndef CC_H
#define CC_H

#include <pthread.h>

#include "bcd.h"
#include "format.h"
#include "xds_decoder.h"

#ifndef VBI_DECODER
#define VBI_DECODER
typedef struct vbi_decoder vbi_decoder;
#endif

typedef enum {
	MODE_NONE,
	MODE_POP_ON,
	MODE_PAINT_ON,
	MODE_ROLL_UP,
	MODE_TEXT
} cc_mode;

typedef struct {
	cc_mode			mode;

	int			col, col1;
	int			row, row1;
	int			roll;

        int			nul_ct;	/* XXX should be 'silence count' */
	double			time;

	/*
	 *  ISO 639 language code (e.g. "fr"), and a NULL array terminator.
	 *  Used by vbi_cache_page_language().
	 */
	const char *		lang_code[2];

	vbi_char		attr;
	vbi_char *		line;

	int			hidden;
	vbi_page		pg[2];
} cc_channel;

struct caption {
	pthread_mutex_t		mutex;

	unsigned char		last[2];		/* field 1, cc command repetition */

	int			curr_chan;
	vbi_char		transp_space[2];	/* caption, text mode */

	/*
	 *  The CC "page cache". Indices refer to caption channel
	 *  1-4, text channel 1-4. channel[8] stores the garbage
	 *  when we don't know the channel number.
	 */
	cc_channel		channel[9];

	vbi_xds_demux		xds_demux;
	int			xds;

	unsigned char		itv_buf[256];
	int			itv_count;

	int			info_cycle[2];
};

/* Public */

/**
 * @addtogroup Cache
 * @{
 */
extern vbi_bool		vbi_fetch_cc_page(vbi_decoder *vbi, vbi_page *pg,
					  vbi_pgno pgno, vbi_bool reset);
/** @} */

/* Private */

extern void		vbi_caption_init(vbi_decoder *vbi);
extern void		vbi_caption_destroy(vbi_decoder *vbi);
extern void		vbi_decode_caption(vbi_decoder *vbi, int line, uint8_t *buf);
extern void		vbi_caption_desync(vbi_decoder *vbi);
extern void		vbi_caption_channel_switched(vbi_decoder *vbi);
extern void		vbi_caption_color_level(vbi_decoder *vbi);

#endif /* CC_H */
