/*
 *  Zapzilla/libvbi public interface
 *
 *  Copyright (C) 2001 I�aki Garc�a Etxebarria
 *  Copyright (C) 2001 Michael H. Schimek
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

/* $Id: libvbi.h,v 1.50 2001-09-02 03:25:58 mschimek Exp $ */

#ifndef __LIBVBI_H__
#define __LIBVBI_H__

#include "../src/tveng.h" /* tveng_frame_pixformat */
#include "../common/fifo.h"
#include "format.h"

/*
    public interface:
	libvbi.h: bcd stuff, struct vbi, VBI_*, vbi_*, vbi_export*
	format.h: attr_*, struct fmt_page
 */

/*
 *  BCD arithmetic for page numbers
 */

static inline unsigned int
dec2bcd(unsigned int dec)
{
	return (dec % 10) + ((dec / 10) % 10) * 16 + ((dec / 100) % 10) * 256;
}

static inline unsigned int
bcd2dec(unsigned int bcd)
{
	return (bcd & 15) + ((bcd >> 4) & 15) * 10 + ((bcd >> 8) & 15) * 100;
}

static inline unsigned int
add_bcd(unsigned int a, unsigned int b)
{
	unsigned int t;

	a += 0x06666666;
	t  = a + b;
	b ^= a ^ t;
	b  = (~b & 0x11111110) >> 3;
	b |= b * 2;

	return (t - b) & 0xFFF;
}

static inline int
is_bcd(unsigned int bcd)
{
	static const unsigned int x = 0x06666666;

	return (((bcd + x) ^ (bcd ^ x)) & 0x110) == 0;
}

/*
 *  Teletext (teletext.c, packet.c)
 */

struct vbi; /* opaque type */

extern int		vbi_fetch_vt_page(struct vbi *vbi, struct fmt_page *pg,
				          int pgno, int subno, int display_rows, int navigation);
extern void		vbi_teletext_set_default_region(struct vbi *vbi, int default_region);
extern void		vbi_teletext_set_level(struct vbi *vbi, int level);

/*
 *  Closed Caption (caption.c)
 */

extern int		vbi_fetch_cc_page(struct vbi *vbi, struct fmt_page *pg, int pgno);

/*
 *  Network identification.
 *
 *  All strings are ISO 8859-1, local language, and NUL terminated.
 *  Prepare for empty strings. Read only.
 */
typedef struct {
	nuid			nuid;			/* unique id */

	char			name[33];		/* descriptive name */
	char			label[33];		/* short name (TTX/VPS) - max. 8 chars */
	char			call[33];		/* network call letters (XDS) */

	int			tape_delay;		/* tape delay, minutes (XDS) */

	/* Private */

	int			cni_vps;
	int			cni_8301;
	int			cni_8302;
	int			cni_x26;

	int			cycle;
} vbi_network;

/*
 *  Navigation (teletext.c)
 */

typedef enum {
	VBI_LINK_NONE = 0,
	VBI_LINK_MESSAGE,
	VBI_LINK_PAGE,
	VBI_LINK_SUBPAGE,
	VBI_LINK_HTTP,
	VBI_LINK_FTP,
	VBI_LINK_EMAIL,
	VBI_LINK_LID,
	VBI_LINK_TELEWEB,
} vbi_link_type;

typedef enum {
	VBI_WEBLINK_UNKNOWN = 0,
	VBI_WEBLINK_PROGRAM_RELATED,
	VBI_WEBLINK_NETWORK_RELATED,
	VBI_WEBLINK_STATION_RELATED,
	VBI_WEBLINK_SPONSOR_MESSAGE,
	VBI_WEBLINK_OPERATOR,
} vbi_itv_type;

typedef struct {
	vbi_link_type			type;
	int				eacem;

	unsigned char 			name[80];

	unsigned char			url[256];
	unsigned char			script[256];

	nuid                            nuid;
	int				page;
	int				subpage;

	double				expires;

	vbi_itv_type			itv_type;
	int				priority;
	int				autoload;
} vbi_link;

extern void		vbi_resolve_link(struct fmt_page *pg, int column, int row, vbi_link *ld);
extern void		vbi_resolve_home(struct fmt_page *pg, vbi_link *ld);

extern int		vbi_page_title(struct vbi *vbi, int pgno, int subno, char *buf);

/*
 *  Teletext search (search.c)
 */

#include "../common/ucs-2.h"

extern void		vbi_delete_search(void *p);
extern void *		vbi_new_search(struct vbi *vbi, int pgno, int subno,
				ucs2_t *pattern, int casefold, int regexp,
				int (* progress)(struct fmt_page *pg));
extern int		vbi_next_search(void *p, struct fmt_page **pg, int dir);

/*
 *  Teletext page cache (cache.c)
 */

extern int              vbi_is_cached(struct vbi *, int pgno, int subno);

/*
 *  Rendering (exp_gfx.c)
 */

extern void		vbi_draw_vt_page_region(struct fmt_page *pg, uint32_t *canvas,
				int column, int row, int width, int height, unsigned int rowstride,
				int reveal, int flash_on);
extern void		vbi_draw_cc_page_region(struct fmt_page *pg, uint32_t *canvas,
				int column, int row, int width, int height, unsigned int rowstride);

static inline void
vbi_draw_vt_page(struct fmt_page *pg, uint32_t *canvas, int reveal, int flash_on)
{
	vbi_draw_vt_page_region(pg, canvas, 0, 0,
		pg->columns, pg->rows, -1, reveal, flash_on);
}

/*
 *  Page classification
 */

#define VBI_NO_PAGE		0x00	/* not in transmission */
#define VBI_NORMAL_PAGE		0x01	/* normal page -> subpage */
#define VBI_SUBTITLE_PAGE	0x70	/* subtitle page -> language */
#define VBI_SUBTITLE_INDEX	0x78	/* subtitle index page */
#define VBI_NONSTD_SUBPAGES	0x79	/* non-std subpages, eg. clock page */
#define VBI_PROGR_WARNING	0x7A	/* program related warning */
#define VBI_CURRENT_PROGR	0x7C	/* current program info -> subpage */
#define VBI_NOW_AND_NEXT	0x7D	/* program related */
#define VBI_PROGR_INDEX		0x7F	/* program index page */
#define VBI_PROGR_SCHEDULE	0x81	/* program schedule page -> subpage */
#define VBI_UNKNOWN_PAGE	0xFF	/* libvbi internal, page not for display */

/*
 *  pgno: 1 ... 8 (Closed Caption)
 *  subpage: always 0
 *  language: returns the language of a page, NULL if unknown,
 *    using Latin-1 char set.
 *
 *  Return value is VBI_SUBTITLE_PAGE for 1 ... 4 (CC1 ... CC4),
 *  VBI_NORMAL_PAGE for 5 ... 8 (T1 ... T4), or VBI_NO_PAGE if the
 *  channel is currently silent.
 *
 *  pgno: 0x100 ... 0x8FF (Teletext)
 *  subpage: returns highest subpage number used (1 ... n),
 *    0			- single page
 *    1			- should not appear
 *    2 ... 0x3F7F	- subpages 1 ... 0x3F7F
 *    0xFFFE		- has unknown number of subpages
 *    0xFFFF		- unknown
 *  language: returns the language of a subtitle page, NULL if unknown or
 *    not VBI_SUBTITLE_PAGE. If valid, *language points to a language
 *    name in the the lang.c font_descriptors table (XXX i18n?).
 *
 *  subpage and/or language can be NULL, the function returns one of the
 *  VBI_ classification codes above.
 *
 *  WARNING the results of this function are volatile: As more information
 *  becomes available and pages are edited (eg. activation of subtitles,
 *  news updates, program related pages) VBI_NO_PAGE, VBI_UNKNOWN_PAGE,
 *  subpage 0xFFFE and 0xFFFF can be revoked; Subpage numbers can grow, page
 *  classifications and languages can change.
 *
 *  The subpage number can be larger (but not smaller) than the number of
 *  subpages actually received and cached. One cannot exclude the possibility
 *  an advertised subpage will never appear.
 */
extern int		vbi_classify_page(struct vbi *vbi, int pgno, int *subpage, char **language);

/*
 *  Aspect ratio
 *
 *  first/last_line: Active video, ITU-R *field* line numbering (first field)
 *    eg. 4:3: PAL 23-310 (288 lines), NTSC 22-262 (240 lines)
 *  ratio: eg. 14/9, 16/9 (ie. anamorphic; letterbox is 1/1).
 */
typedef enum {
	VBI_SUBT_NONE,
	VBI_SUBT_ACTIVE,	/* in active area */
	VBI_SUBT_MATTE,		/* letterbox, below active video */
	VBI_SUBT_UNKNOWN,
} vbi_subt;

typedef struct {
	int			first_line;
	int			last_line;
	double			ratio;
	int			film_mode;		/* bool, if known */
	vbi_subt		open_subtitles;
} vbi_ratio;

/*
 *  Event (vbi.c)
 */

#define VBI_EVENT_NONE		0
#define	VBI_EVENT_CLOSE		(1 << 0)

#define	VBI_EVENT_TTX_PAGE	(1 << 1)
/*
 *  Received (and cached) another Teletext page designated by
 *  ev.ttx_page.pgno and ev.ttx_page.subno.
 * 
 *  ev.ttx_page.roll_header flags the page header as suitable for
 *  rolling page numbers (eg. excluding subtitle pages).
 *
 *  The ev.ttx_page.header_update flag is set when the header,
 *  excluding the page number and real time clock, changed since the
 *  last TTX_PAGE event. Note this may happen at midnight when the
 *  date string changes. The ev.ttx_page.clock_update flag is set when
 *  the real time clock changed since the last TTX_PAGE event (that is
 *  at most once per second). They are both set at the first TTX_EVENT
 *  and unset while the received header or clock field is corrupted.
 *
 *  If any of the roll_header, header_update or clock_update flags
 *  are set ev.ttx_page.raw_header is a pointer to the raw header data
 *  (40 bytes), which remains valid until the event handler returns,
 *  and ev.ttx_page.pn_offset is the offset (0 ... n) of the three
 *  digit page number in the raw or formatted header. Use the raw header
 *  only to determine changes, but call vbi_fetch_vt_page for proper
 *  translation of national characters and character attributes.
 */

#define VBI_EVENT_CAPTION	(1 << 2)
/*
 *  A closed caption page has changed and needs visual update.
 *  The page a.k.a. CC channel is designated by ev.caption.pgno,
 *  which is CC_PAGE_BASE (1) plus
 *
 *  0:	"Caption 1" "Primary synchronous caption service [English]"
 *  1:  "Caption 2" "Special non-synchronous data that is intended to
 *                   augment information carried in the program"
 *  2:  "Caption 3" "Secondary synchronous caption service, usually
 *                   second language [Spanish, French]"
 *  3:  "Caption 4" "Special non-synchronous data similar to Caption 2"
 *  4:  "Text 1"    "First text service, data usually not program
 *                   related"
 *  5:  "Text 2"    "Second text service, additional data usually
 *                   not program related (ITV)"
 *  6:  "Text 3"    "Third and forth text services, to be used only
 *  7:  "Text 4"     if Text 1 and Text 2 are not sufficient"
 *
 *  If the client is monitoring this page, the expected action is
 *  to call vbi_fetch_cc_page(). To speed up rendering more detailed
 *  update information is provided in fmt_page.dirty (see format.h).
 *  The fmt_page is will be a snapshot of the current status,
 *  fmt_page.dirty accumulates all changes since the last fetch.
 *
 *  XXX the dirty mechanism is not prepared for multiple
 *  caption event handlers.
 */

#define	VBI_EVENT_NETWORK	(1 << 4)
/*
 *  Some station/network identifier has been received or is no longer
 *  transmitted (vbi_network all zero, eg. after a channel switch).
 *  ev.network is a vbi_network object, read only. The event will not
 *  repeat*) unless a different identifier has been received and confirmed.
 *
 *  Minimum time for recognition
 *
 *  VPS (DE/AT/CH only):	0.08 s
 *  Teletext PDC, 8/30:		2 s
 *  Teletext X/26:		unknown
 *  XDS (US only):		unknown, between 0.1x to 10x seconds
 *
 *  *) VPS/TTX and XDS will not combine in real life, sample insertion
 *     can confuse the logic.
 */

#define	VBI_EVENT_TRIGGER	(1 << 5)
/*
 *  A trigger has fired, ev.trigger is a vbi_link *pointer*.
 */

#define VBI_EVENT_IO_ERROR	(1 << 6)
/*
 *  A fatal I/O error occured, as reported by the vbi fifo producer.
 *  On return from the event handler the vbi mainloop will terminate
 *  and must be restarted after the problem has been solved.
 *
 *  (XXX this should include an error code and message)
 *  (XXX rethink termination)
 */

#define	VBI_EVENT_RATIO		(1 << 7)
/*
 *  Aspect ratio change, ev.ratio is a vbi_ratio object, read only.
 *  (From PAL WSS, NTSC XDS or EIA-J CPR-1204)
 */

typedef struct {
	int			type;
	union {
		struct {
		        int			pgno;
		        int			subno;
			uint8_t *               raw_header;
			int                     pn_offset;
			unsigned int            roll_header : 1;
		        unsigned int            header_update : 1;
			unsigned int            clock_update : 1;
	        }                       ttx_page;
		struct {
			int                     pgno;
		}                       caption;
		vbi_network             network;
                vbi_link *              trigger;
                vbi_ratio               ratio;
	}                       ev;
} vbi_event;

extern int		vbi_event_handler(struct vbi *vbi, int event_mask, void (* handler)(vbi_event *, void *), void *user_data); 
/* Affects vbi_fetch_*_page(), the fmt_page, not export */
extern void		vbi_set_colour_level(struct vbi *vbi, int brig, int cont);
extern void             vbi_channel_switched(struct vbi *vbi, nuid nuid);
extern void		vbi_close(struct vbi *vbi);
extern struct vbi *	vbi_open(fifo *source);

/* Video feed (WSS from RGB) */

extern void		vbi_push_video(struct vbi *vbi, void *data, int width,
			       enum tveng_frame_pixformat fmt, double time);

/*
 *  Export (export.c)
 */

typedef struct vbi_export vbi_export; /* opaque type */

typedef struct {
	char *			keyword;
	char *			label;		/* or NULL, gettext()ized */
	char *			tooltip;	/* or NULL, gettext()ized */
} vbi_export_module;

typedef enum {
	VBI_EXPORT_BOOL = 1,	/* TRUE (1) or FALSE (0), def.num */
	VBI_EXPORT_INT,		/* Integer min - max, def.num */
	VBI_EXPORT_MENU,	/* Index of menu[], min - max, def.num */
	VBI_EXPORT_STRING,	/* String, def.str */
} vbi_export_option_type;

typedef struct {
	vbi_export_option_type	type;
	char *			keyword;
	char *			label;		/* gettext()ized */
	union {
		char *			str;	/* gettext()ized */
		int			num;
	}			def;
	int			min, max;
	char **			menu;		/* max - min + 1 entries, gettext()ized */
	char *			tooltip;	/* or NULL, gettext()ized */
} vbi_export_option;

extern vbi_export_module *vbi_export_enum(int index);
extern vbi_export *	vbi_export_open(char *keyword, vbi_network *, int reveal);
extern void		vbi_export_close(vbi_export *e);
extern vbi_export_module *vbi_export_info(vbi_export *e);
extern vbi_export_option *vbi_export_query_option(vbi_export *exp, int index);
extern int		vbi_export_set_option(vbi_export *exp, int index, ...);
extern char *		vbi_export_mkname(vbi_export *e, char *fmt, int pgno, int subno, char *usr);
extern int		vbi_export_name(vbi_export *e, char *name, struct fmt_page *pg);
extern int		vbi_export_file(vbi_export *e, FILE *fp, struct fmt_page *pg);

/* XXX */
void vbi_get_max_rendered_size(int *w, int *h);
void vbi_get_vt_cell_size(int *w, int *h);
int vbi_cache_hi_subno(struct vbi *vbi, int pgno);

#endif /* __LIBVBI_H__ */













