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

/* $Id: caption.c,v 1.9.2.10 2004-07-09 16:10:52 mschimek Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include "trigger.h"
#include "format.h"
#include "lang.h"
#include "hamm.h"
#include "tables.h"
#include "vbi_decoder-priv.h"
#include "misc.h"
#include "cc.h"

#define printable(c) ((((c) & 0x7F) < 0x20 || ((c) & 0x7F) > 0x7E) ? '.' : ((c) & 0x7F))
#define elements(array) (sizeof(array) / sizeof(array[0]))

#define XDS_DEBUG(x) /* x */
#define ITV_DEBUG(x) /* x */
#define XDS_SEP_DEBUG(x) /* x */
#define XDS_SEP_DUMP(x) /* x */
#define CC_DUMP(x) /* x */
#define CC_TEXT_DUMP(x) /* x */

vbi_inline void
caption_send_event(vbi_decoder *vbi, vbi_event *ev)
{
	/* Permits calling vbi_fetch_cc_page from handler */
  //	pthread_mutex_unlock(&vbi->cc.mutex);

// obsolete
//	vbi_send_event(vbi, ev);

//	pthread_mutex_lock(&vbi->cc.mutex);
}

/*
 *  XDS (Extended Data Service) decoder
 */

static void
itv_separator(vbi_decoder *vbi, vbi_caption_decoder *cc, char c)
{
  	if (ITV_DEBUG(0 &&) !(vbi->handlers.event_mask
  			      & VBI_EVENT_TRIGGER))
  		return;

	if (c >= 0x20) {
		if (c == '<') // s4-nbc omitted CR
			itv_separator(vbi, cc, 0);
		else if (cc->itv_count > sizeof(cc->itv_buf) - 2)
			cc->itv_count = 0;

		cc->itv_buf[cc->itv_count++] = c;

		return;
	}

	cc->itv_buf[cc->itv_count] = 0;
	cc->itv_count = 0;

	ITV_DEBUG(printf("ITV: <%s>\n", cc->itv_buf));

	//	vbi_atvef_trigger(vbi, cc->itv_buf);
}

/*
 *  Closed Caption decoder
 */

#define ROWS			15
#define COLUMNS			34

static void
render(vbi_page *pg, int row)
{
	vbi_event event;

	if (row < 0 || pg->dirty.roll) {
		/* no particular row or not fetched
		   since last roll/clear, redraw all */
		pg->dirty.y0 = 0;
		pg->dirty.y1 = ROWS - 1;
		pg->dirty.roll = 0;
	} else {
		pg->dirty.y0 = MIN(row, pg->dirty.y0);
		pg->dirty.y1 = MAX(row, pg->dirty.y1);
	}

	event.type = VBI_EVENT_CAPTION;
	event.ev.caption.channel = pg->pgno;

	//TODO	caption_send_event(pg->vbi, &event);
}

static void
clear(vbi_page *pg)
{
	vbi_event event;

	pg->dirty.y0 = 0;
	pg->dirty.y1 = ROWS - 1;
	pg->dirty.roll = -ROWS;

	event.type = VBI_EVENT_CAPTION;
	event.ev.caption.channel = pg->pgno;

	// TODO	caption_send_event(pg->vbi, &event);
}

static void
roll_up(vbi_page *pg, int first_row, int last_row)
{
	vbi_event event;

	if (pg->dirty.roll != 0 || pg->dirty.y0 <= pg->dirty.y1) {
		/* not fetched since last update, redraw all */
		pg->dirty.roll = 0;
		pg->dirty.y0 = MIN(first_row, pg->dirty.y0);
		pg->dirty.y1 = MAX(last_row, pg->dirty.y1);
	} else {
		pg->dirty.roll = -1;
		pg->dirty.y0 = first_row;
		pg->dirty.y1 = last_row;
	}

	event.type = VBI_EVENT_CAPTION;
	event.ev.caption.channel = pg->pgno;

	// TODO	caption_send_event(pg->vbi, &event);
}

vbi_inline void
update(cc_channel *ch)
{
	vbi_char *acp = ch->line - ch->pg[0].text + ch->pg[1].text;

	memcpy(acp, ch->line, sizeof(*acp) * COLUMNS);
}

static void
word_break(vbi_caption_decoder *cc, cc_channel *ch, int upd)
{
	/*
	 *  Add a leading and trailing space.
	 */
	if (ch->col > ch->col1) {
		vbi_char c = ch->line[ch->col1];

		if ((c.unicode & 0x7F) != 0x20
		    && ch->line[ch->col1 - 1].opacity == VBI_TRANSPARENT_SPACE) {
			c.unicode = 0x20;
			ch->line[ch->col1 - 1] = c;
		}

		c = ch->line[ch->col - 1];

		if ((c.unicode & 0x7F) != 0x20
		    && ch->line[ch->col].opacity == VBI_TRANSPARENT_SPACE) {
			c.unicode = 0x20;
			ch->line[ch->col] = c;
		}
	}

	if (!upd || ch->mode == MODE_POP_ON)
		return;

	/*
	 *  NB we render only at spaces (end of word) and
	 *  before cursor motions and mode switching, to keep the
	 *  drawing efforts (scaling etc) at a minimum. update()
	 *  for double buffering at word granularity.
	 *
	 *  XXX should not render if space follows space,
	 *  but force in long words. 
	 */

	update(ch);
	render(ch->pg + 1, ch->row);
}

vbi_inline void
set_cursor(cc_channel *ch, int col, int row)
{
	ch->col = ch->col1 = col;
	ch->row = row;

	ch->line = ch->pg[ch->hidden].text + row * COLUMNS;
}

static void
put_char(vbi_caption_decoder *cc, cc_channel *ch, vbi_char c)
{
	/* c.foreground = rand() & 7; */
	/* c.background = rand() & 7; */

	if (ch->col < COLUMNS - 1)
		ch->line[ch->col++] = c;
	else {
		/* line break here? */

		ch->line[COLUMNS - 2] = c;
	}

	if ((c.unicode & 0x7F) == 0x20)
		word_break(cc, ch, 1);
}

vbi_inline cc_channel *
switch_channel(vbi_caption_decoder *cc, cc_channel *ch, int new_chan)
{
	word_break(cc, ch, 1); // we leave for a number of frames

	return &cc->channel[cc->curr_chan = new_chan];
}

static void
erase_memory(vbi_caption_decoder *cc, cc_channel *ch, int page)
{
	vbi_page *pg = ch->pg + page;
	vbi_char *acp = pg->text;
	vbi_char c = cc->transp_space[ch >= &cc->channel[4]];
	int i;

	for (i = 0; i < COLUMNS * ROWS; acp++, i++)
		*acp = c;

	pg->dirty.y0 = 0;
	pg->dirty.y1 = ROWS - 1;
	pg->dirty.roll = ROWS;
}

static const vbi_color
palette_mapping[8] = {
	VBI_WHITE, VBI_GREEN, VBI_BLUE, VBI_CYAN,
	VBI_RED, VBI_YELLOW, VBI_MAGENTA, VBI_BLACK
};

static int
row_mapping[] = {
	10, -1,  0, 1, 2, 3,  11, 12, 13, 14,  4, 5, 6, 7, 8, 9
};

// not verified means I didn't encounter the code in a
// sample stream yet

static void
caption_command(vbi_decoder *vbi, vbi_caption_decoder *cc,
	unsigned char c1, unsigned char c2, vbi_bool field2)
{
	cc_channel *ch;
	int chan, col, i;
	int last_row;

	chan = (cc->curr_chan & 4) + field2 * 2 + ((c1 >> 3) & 1);
	ch = &cc->channel[chan];

	c1 &= 7;

	if (c2 >= 0x40) {	/* Preamble Address Codes  001 crrr  1ri xxxu */
		int row = row_mapping[(c1 << 1) + ((c2 >> 5) & 1)];

		if (row < 0 || !ch->mode)
			return;

		COPY_SET_COND (ch->attr.attr, VBI_UNDERLINE, c2 & 1);

		ch->attr.background = VBI_BLACK;
		ch->attr.opacity = VBI_OPAQUE;
		ch->attr.attr &= ~VBI_FLASH;

		word_break(cc, ch, 1);

		if (ch->mode == MODE_ROLL_UP) {
			int row1 = row - ch->roll + 1;

			if (row1 < 0)
				row1 = 0;

			if (row1 != ch->row1) {
				ch->row1 = row1;
				erase_memory(cc, ch, ch->hidden);
				erase_memory(cc, ch, ch->hidden ^ 1);
			}

			set_cursor(ch, 1, ch->row1 + ch->roll - 1);
		} else
			set_cursor(ch, 1, row);

		if (c2 & 0x10) {
			col = ch->col;

			for (i = (c2 & 14) * 2; i > 0 && col < COLUMNS - 1; i--)
				ch->line[col++] = cc->transp_space[chan >> 2];

			if (col > ch->col)
				ch->col = ch->col1 = col;

			ch->attr.attr &= ~VBI_ITALIC;
			ch->attr.foreground = VBI_WHITE;
		} else {
// not verified
			c2 = (c2 >> 1) & 7;

			if (c2 < 7) {
				ch->attr.attr &= ~VBI_ITALIC;
				ch->attr.foreground = palette_mapping[c2];
			} else {
				ch->attr.attr |= VBI_ITALIC;
				ch->attr.foreground = VBI_WHITE;
			}
		}

		return;
	}

	switch (c1) {
	case 0:		/* Optional Attributes		001 c000  010 xxxt */
// not verified
		ch->attr.opacity = (c2 & 1) ? VBI_SEMI_TRANSPARENT : VBI_OPAQUE;
		ch->attr.background = palette_mapping[(c2 >> 1) & 7];
		return;

	case 1:
		if (c2 & 0x10) {	/* Special Characters	001 c001  011 xxxx */
// not verified
			c2 &= 15;

			if (c2 == 9) { // "transparent space"
				if (ch->col < COLUMNS - 1) {
					ch->line[ch->col++] = cc->transp_space[chan >> 2];
					ch->col1 = ch->col;
				} else
					ch->line[COLUMNS - 2] = cc->transp_space[chan >> 2];
					// XXX boxed logic?
			} else {
				vbi_char c = ch->attr;

				c.unicode = vbi_caption_unicode(c2 & 15);

				put_char(cc, ch, c);
			}
		} else {		/* Midrow Codes		001 c001  010 xxxu */
// not verified
			ch->attr.attr &= ~VBI_FLASH;
			COPY_SET_COND (ch->attr.attr, VBI_UNDERLINE, c2 & 1);

			c2 = (c2 >> 1) & 7;

			if (c2 < 7) {
				ch->attr.attr &= ~VBI_ITALIC;
				ch->attr.foreground = palette_mapping[c2];
			} else {
				ch->attr.attr |= VBI_ITALIC;
				ch->attr.foreground = VBI_WHITE;
			}
		}

		return;

	case 2:		/* Optional Extended Characters	001 c01f  01x xxxx */
	case 3:
		/* Send specs to the maintainer of this code */
		return;

	case 4:		/* Misc Control Codes		001 c10f  010 xxxx */
	case 5:		/* Misc Control Codes		001 c10f  010 xxxx */
		/* f ("field"): purpose? */

		switch (c2 & 15) {
		case 0:		/* Resume Caption Loading	001 c10f  010 0000 */
			ch = switch_channel(cc, ch, chan & 3);

			ch->mode = MODE_POP_ON;

// no?			erase_memory(cc, ch);

			return;

		/* case 4: reserved */

		case 5:		/* Roll-Up Captions		001 c10f  010 0xxx */
		case 6:
		case 7:
		{
			int roll = (c2 & 7) - 3;

			ch = switch_channel(cc, ch, chan & 3);

			if (ch->mode == MODE_ROLL_UP && ch->roll == roll)
				return;

			erase_memory(cc, ch, ch->hidden);
			erase_memory(cc, ch, ch->hidden ^ 1);

			ch->mode = MODE_ROLL_UP;
			ch->roll = roll;

			set_cursor(ch, 1, 14);

			ch->row1 = 14 - roll + 1;

			return;
		}

		case 9:		/* Resume Direct Captioning	001 c10f  010 1001 */
// not verified
			ch = switch_channel(cc, ch, chan & 3);
			ch->mode = MODE_PAINT_ON;
			return;

		case 10:	/* Text Restart			001 c10f  010 1010 */
// not verified
			ch = switch_channel(cc, ch, chan | 4);
			set_cursor(ch, 1, 0);
			return;

		case 11:	/* Resume Text Display		001 c10f  010 1011 */
			ch = switch_channel(cc, ch, chan | 4);
			return;

		case 15:	/* End Of Caption		001 c10f  010 1111 */
			ch = switch_channel(cc, ch, chan & 3);
			ch->mode = MODE_POP_ON;

			word_break(cc, ch, 1);

			ch->hidden ^= 1;

			render(ch->pg + (ch->hidden ^ 1), -1 /* ! */);

			erase_memory(cc, ch, ch->hidden); // yes?

			/*
			 *  A Preamble Address Code should follow,
			 *  reset to a known state to be safe.
			 *  Reset ch->line for new ch->hidden.
			 *  XXX row 0?
			 */
			set_cursor(ch, 1, ROWS - 1);

			return;

		case 8:		/* Flash On			001 c10f  010 1000 */
// not verified
			ch->attr.attr |= VBI_FLASH;
			return;

		case 1:		/* Backspace			001 c10f  010 0001 */
// not verified
			if (ch->mode && ch->col > 1) {
				ch->line[--ch->col] = cc->transp_space[chan >> 2];

				if (ch->col < ch->col1)
					ch->col1 = ch->col;
			}

			return;

		case 13:	/* Carriage Return		001 c10f  010 1101 */
			if (ch == cc->channel + 5)
				itv_separator(vbi, cc, 0);

			if (!ch->mode)
				return;

			last_row = ch->row1 + ch->roll - 1;

			if (last_row > ROWS - 1)
				last_row = ROWS - 1;

			if (ch->row < last_row) {
				word_break(cc, ch, 1);
				set_cursor(ch, 1, ch->row + 1);
			} else {
				vbi_char *acp = &ch->pg[ch->hidden ^ (ch->mode != MODE_POP_ON)]
					.text[ch->row1 * COLUMNS];

				word_break(cc, ch, 1);
				update(ch);

				memmove(acp, acp + COLUMNS, sizeof(*acp) * (ch->roll - 1) * COLUMNS);

				for (i = 0; i <= COLUMNS; i++)
					ch->line[i] = cc->transp_space[chan >> 2];

				if (ch->mode != MODE_POP_ON) {
					update(ch);
					roll_up(ch->pg + (ch->hidden ^ 1), ch->row1, last_row);
				}

				ch->col1 = ch->col = 1;
			}

			return;

		case 4:		/* Delete To End Of Row		001 c10f  010 0100 */
// not verified
			if (!ch->mode)
				return;

			for (i = ch->col; i <= COLUMNS - 1; i++)
				ch->line[i] = cc->transp_space[chan >> 2];

			word_break(cc, ch, 0);

			if (ch->mode != MODE_POP_ON) {
				update(ch);
				render(ch->pg + (ch->hidden ^ 1), ch->row);
			}

			return;

		case 12:	/* Erase Displayed Memory	001 c10f  010 1100 */
// s1, s4: EDM always before EOC
			if (ch->mode != MODE_POP_ON)
				erase_memory(cc, ch, ch->hidden);

			erase_memory(cc, ch, ch->hidden ^ 1);
			clear(ch->pg + (ch->hidden ^ 1));

			return;

		case 14:	/* Erase Non-Displayed Memory	001 c10f  010 1110 */
// not verified
			if (ch->mode == MODE_POP_ON)
				erase_memory(cc, ch, ch->hidden);

			return;
		}

		return;

	/* case 6: reserved */

	case 7:
		if (!ch->mode)
			return;

		switch (c2) {
		case 0x21 ... 0x23:	/* Misc Control Codes, Tabs	001 c111  010 00xx */
// not verified
			col = ch->col;

			for (i = c2 & 3; i > 0 && col < COLUMNS - 1; i--)
				ch->line[col++] = cc->transp_space[chan >> 2];

			if (col > ch->col)
				ch->col = ch->col1 = col;

			return;

		case 0x2D:		/* Optional Attributes		001 c111  010 11xx */
// not verified
			ch->attr.opacity = VBI_TRANSPARENT_FULL;
			break;

		case 0x2E:		/* Optional Attributes		001 c111  010 11xx */
		case 0x2F:
// not verified
			ch->attr.foreground = VBI_BLACK;
			COPY_SET_COND (ch->attr.attr, VBI_UNDERLINE, c2 & 1);
			break;

		default:
			return;
		}

		/* Optional Attributes, backspace magic */

		if (ch->col > 1 && (ch->line[ch->col - 1].unicode & 0x7F) == 0x20) {
			vbi_char c = ch->attr;

			c.unicode = 0x0020;
			ch->line[ch->col - 1] = c;
		}
	}
}

/**
 * @internal
 * @param vbi Initialized vbi decoding context.
 * @param line ITU-R line number this data originated from.
 * @param buf Two bytes.
 * 
 * Decode two bytes of Closed Caption data (Caption, XDS, ITV),
 * updating the decoder state accordingly. May send events.
 */
void
vbi_decode_caption(vbi_caption_decoder *cd, int line, uint8_t *buf)
{

	char c1 = buf[0] & 0x7F;
	int field2 = 1, i;

/*
	vbi_hook_call (vbi, VBI_HOOK_CLOSED_CAPTION,
		       buf, 2, 0);
 */

//	pthread_mutex_lock(&cc->mutex);

	switch (line) {
	case 21:	/* NTSC */
	case 22:	/* PAL */
		field2 = 0;
		break;

	case 335:	/* PAL, hardly XDS */
		break;

	case 284:	/* NTSC */
		CC_DUMP(
			putchar(printable(buf[0]));
			putchar(printable(buf[1]));
			fflush(stdout);
		)

		if (vbi_ipar8 (buf[0]) >= 0) {
			if (c1 == 0) {
				goto finish;
			} else if (c1 <= 0x0F) {
//TODO				vbi_xds_demux_demux (&vbi->cc.xds_demux, buf);
				cd->xds = (c1 != 0x0F);
				goto finish;
			} else if (c1 <= 0x1F) {
				cd->xds = FALSE;
			} else if (cd->xds) {
//				vbi_xds_demux_demux (&vbi->cc.xds_demux, buf);
				goto finish;
			}
		} else if (cd->xds) {
			vbi_xds_demux_demux (&cd->xds_demux, buf);
			goto finish;
		}
 
		break;

	default:
		goto finish;
	}

	if (vbi_ipar8 (buf[0]) < 0) {
		c1 = 127;
		buf[0] = c1; /* traditional 'bad' glyph, ccfont has */
		buf[1] = c1; /*  room, design a special glyph? */
	}

	CC_DUMP(
		putchar(printable(buf[0]));
		putchar(printable(buf[1]));
		fflush(stdout);
	)

	switch (c1) {
		cc_channel *ch;
		vbi_char c;

	case 0x01 ... 0x0F:
		if (!field2)
			cd->last[0] = 0;
		break; /* XDS field 1?? */

	case 0x10 ... 0x1F:
		if (vbi_ipar8 (buf[1]) >= 0) {
			if (!field2
			    && buf[0] == cd->last[0]
			    && buf[1] == cd->last[1]) {
				/* cmd repetition F1: already executed */
				cd->last[0] = 0; /* one rep */
				break;
			}

//TODO			caption_command(vbi, cc, c1, buf[1] & 0x7F, field2);

			if (!field2) {
				cd->last[0] = buf[0];
				cd->last[1] = buf[1];
			}
		} else if (!field2)
			cd->last[0] = 0;

		break;

	default:
		CC_TEXT_DUMP(
			putchar(printable(buf[0]));
			putchar(printable(buf[1]));
			fflush(stdout);
		)

		ch = &cd->channel[(cd->curr_chan & 5) + field2 * 2];

		if (buf[0] == 0x80 && buf[1] == 0x80) {
			if (ch->mode) {
				if (ch->nul_ct == 2)
					word_break(cd, ch, 1);
				ch->nul_ct += 2;
			}

			break;
		}

		if (!field2)
			cd->last[0] = 0;

		ch->nul_ct = 0;

		if (!ch->mode)
			break;

//TODO		ch->time = vbi->time; /* activity measure */

		c = ch->attr;

		for (i = 0; i < 2; i++) {
			char ci = vbi_ipar8 (buf[i]) & 0x7F; /* 127 if bad */

			if (ci <= 0x1F) /* 0x00 no char, 0x01 ... 0x1F invalid */
				continue;

//TODO			if (ch == cd->channel + 5) // 'T2'
//				itv_separator(vbi, cc, ci);

			c.unicode = vbi_caption_unicode(ci);

			put_char(cd, ch, c);
		}
	}

 finish:
	;
//	pthread_mutex_unlock(&cc->mutex);
}



static vbi_rgba
default_color_map[8] = {
	VBI_RGBA(0x00, 0x00, 0x00), VBI_RGBA(0xFF, 0x00, 0x00),
	VBI_RGBA(0x00, 0xFF, 0x00), VBI_RGBA(0xFF, 0xFF, 0x00),	
	VBI_RGBA(0x00, 0x00, 0xFF), VBI_RGBA(0xFF, 0x00, 0xFF),
	VBI_RGBA(0x00, 0xFF, 0xFF), VBI_RGBA(0xFF, 0xFF, 0xFF)
};

/**
 * @internal
 * @param vbi Initialized vbi decoding context.
 * 
 * After the client changed text brightness and saturation
 * this function adjusts the Closed Caption color palette.
XXX changed
 */
void
vbi_caption_color_level(vbi_caption_decoder *cd)
{
	int i;

	//	vbi_transp_colormap (vbi, vbi->cc.channel[0].pg[0].color_map,
	//		     default_color_map, 8);

	memcpy (cd->channel[0].pg[0].color_map, default_color_map,
		sizeof (default_color_map)); 

	for (i = 1; i < 16; i++)
		memcpy(cd->channel[i >> 1].pg[i & 1].color_map,
		       cd->channel[0].pg[0].color_map,
		       sizeof(default_color_map));
}

/**
 * @param vbi Initialized vbi decoding context.
 * @param pg Place to store the formatted page.
 * @param pgno Page number 1 ... 8 of the page to fetch, see vbi_pgno.
 * @param reset @c TRUE resets the vbi_page dirty fields in cache after
 *   fetching. Pass @c FALSE only if you plan to call this function again
 *   to update other displays.
 * 
 * Fetches a Closed Caption page designated by @a pgno from the cache,
 * formats and stores it in @a pg. CC pages are transmitted basically in
 * two modes: at once and character by character ("roll-up" mode).
 * Either way you get a snapshot of the page as it should appear on
 * screen at present. With vbi_event_handler_add() you can request a
 * @c VBI_EVENT_CAPTION event to be notified about pending changes
 * (in case of "roll-up" mode that is with each new word received)
 * and the vbi_page->dirty fields will mark the lines actually in
 * need of updates, to speed up rendering.
 * 
 * Although safe to do, this function is not supposed to be
 * called from an event handler, since rendering may block decoding
 * for extended periods of time.
 * 
 * @return
 * @c FALSE if some error occured.
 */
vbi_bool
vbi_fetch_cc_page(vbi_caption_decoder *cd, vbi_page *pg, vbi_pgno pgno, vbi_bool reset)
{
	cc_channel *ch = cd->channel + ((pgno - 1) & 7);
	vbi_page *spg;

	if (pgno < 1 || pgno > 8)
		return FALSE;

//	pthread_mutex_lock(&vbi->cc.mutex);

	spg = ch->pg + (ch->hidden ^ 1);

	memcpy(pg, spg, sizeof(*pg)); /* shortcut? */

	spg->dirty.y0 = ROWS;
	spg->dirty.y1 = -1;
	spg->dirty.roll = 0;

//	pthread_mutex_unlock(&vbi->cc.mutex);

	return 1;
}










const vbi_program_id *
vbi_caption_decoder_program_id	(vbi_caption_decoder *	cd)
{
	/* TODO */
	return NULL;
}

void
vbi_caption_decoder_remove_event_handler
				(vbi_caption_decoder *	cd,
				 vbi_event_cb *		callback,
				 void *			user_data)
{
	/* TODO */
}

vbi_bool
vbi_caption_decoder_add_event_handler
				(vbi_caption_decoder *	cd,
				 unsigned int		event_mask,
				 vbi_event_cb *		callback,
				 void *			user_data)
{
	/* TODO */

	if (0 == event_mask)
		return TRUE;

	return TRUE;
}

/** @internal */
void
vbi_caption_decoder_resync	(vbi_caption_decoder *	cd)
{


	/* cc->curr_chan = 8; *//* garbage */

	cd->xds = FALSE;

	vbi_xds_demux_reset (&cd->xds_demux);

	cd->itv_count = 0;
}

void
vbi_caption_decoder_reset	(vbi_caption_decoder *	cd,
				 const vbi_network *	nk)
{

	cc_channel *ch;
	int i;

	for (i = 0; i < 9; i++) {
		ch = &cd->channel[i];

		if (i < 4) {
			ch->mode = MODE_NONE; // MODE_ROLL_UP;
			ch->row = ROWS - 1;
			ch->row1 = ROWS - 3;
			ch->roll = 3;
		} else {
			ch->mode = MODE_TEXT;
			ch->row1 = ch->row = 0;
			ch->roll = ROWS;
		}

		ch->attr.opacity = VBI_OPAQUE;
		ch->attr.foreground = VBI_WHITE;
		ch->attr.background = VBI_BLACK;

		set_cursor(ch, 1, ch->row);

		ch->time = 0.0;

		ch->hidden = 0;

		ch->pg[0].dirty.y0 = 0;
		ch->pg[0].dirty.y1 = ROWS - 1;
		ch->pg[0].dirty.roll = 0;

		erase_memory(cd, ch, 0);

		memcpy(&ch->pg[1], &ch->pg[0], sizeof(ch->pg[1]));
	}

	cd->xds = FALSE;

	vbi_xds_demux_reset (&cd->xds_demux);

	cd->info_cycle[0] = 0;
	cd->info_cycle[1] = 0;

	vbi_caption_decoder_resync (cd);
}

vbi_bool
vbi_caption_decoder_decode	(vbi_caption_decoder *	cd,
				 const uint8_t		buffer[2],
				 double			timestamp)
{
	/* TODO */
	return FALSE;
}

void
cache_network_destroy_caption	(cache_network *	cn)
{
  /* TODO */
}

void
cache_network_init_caption	(cache_network *	cn)
{
  /* TODO */
}

/** @internal */
void
_vbi_caption_decoder_destroy	(vbi_caption_decoder *	cd)
{
	assert (NULL != cd);

//	pthread_mutex_destroy(&vbi->cc.mutex);

	_vbi_xds_demux_destroy (&cd->xds_demux);
}

void reset (vbi_caption_decoder *cd, cache_network *cn, double time)
{
// TODO
}

/** @internal */
void
_vbi_caption_decoder_init	(vbi_caption_decoder *	cd,
				 vbi_cache *		ca,
				 const vbi_network *	nk,
				 vbi_videostd_set	videostd_set)
{
	cc_channel *ch;
	int i;

	assert (NULL != cd);

	memset(cd, 0, sizeof(*cd));

//	pthread_mutex_init(&cc->mutex, NULL);

	for (i = 0; i < 9; i++) {
		ch = &cd->channel[i];

		// TODO		ch->pg[0].vbi = NULL; //vbi;

		ch->pg[0].pgno = i + 1;
		ch->pg[0].subno = 0;

		ch->pg[0].rows = ROWS;
		ch->pg[0].columns = COLUMNS;

		ch->pg[0].screen_color = 0;
		ch->pg[0].screen_opacity = (i < 4) ? VBI_TRANSPARENT_SPACE : VBI_OPAQUE;

#warning todo
//		ch->pg[0].font[0] = vbi_font_descriptors; /* English */
//		ch->pg[0].font[1] = vbi_font_descriptors;

		memcpy(&ch->pg[1], &ch->pg[0], sizeof(ch->pg[1]));
	}

       	for (i = 0; i < 2; i++) {
		cd->transp_space[i].foreground = VBI_WHITE;
		cd->transp_space[i].background = VBI_BLACK;
		cd->transp_space[i].unicode = 0x0020;
	}

	cd->transp_space[0].opacity = VBI_TRANSPARENT_SPACE;
	cd->transp_space[1].opacity = VBI_OPAQUE;

//TODO	_vbi_xds_demux_init (&cd->xds_demux, _vbi_decode_xds, vbi);

//TODO	vbi_caption_channel_switched(vbi);

//TODO	vbi_caption_color_level(vbi);

	cd->virtual_reset = reset;
}

void
vbi_caption_decoder_delete	(vbi_caption_decoder *	cd)
{
	/* TODO */
}

vbi_caption_decoder *
vbi_caption_decoder_new		(vbi_cache *		ca)
{
	/* TODO */
	return NULL;
}