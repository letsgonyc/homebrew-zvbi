/*
 * Zapping (TV viewer for the Gnome Desktop)
 *
 * Copyright (C) 2000-2001 I�aki Garc�a Etxebarria 
 * Copyright (C) 2002-2003 Michael H. Schimek
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: frequencies.h,v 1.16 2007-08-30 12:21:20 mschimek Exp $ */

#ifndef FREQUENCIES_H
#define FREQUENCIES_H

#include "tveng.h"
#include "keyboard.h"

#ifdef HAVE_LIBZVBI
#  include "libvbi/bcd.h"
#  include "libvbi/lang.h"
#endif

typedef struct _tveng_tc_control tveng_tc_control;

struct _tveng_tc_control {
  gchar				name [32];
  gfloat			value;		/* [0;1] */
};

typedef struct _tveng_ttx_encoding tveng_ttx_encoding;

struct _tveng_ttx_encoding {
#ifdef HAVE_LIBZVBI
  vbi3_pgno			pgno;
  vbi3_ttx_charset_code		charset_code;
#endif
};

typedef struct _tveng_tuned_channel tveng_tuned_channel;

struct _tveng_tuned_channel {
  /* Station (RTL, Eurosport, whatever). Can be NULL. */
  gchar *			null_name;

  /* Can be NULL. */
  gchar *			null_rf_table;

  /* RF channel ("35", for example). Can be NULL. */
  gchar *			null_rf_name;


  unsigned int input, standard; /* Attached input, standard or 0 */

  z_key				accel;		/* key to select this channel */

  unsigned int index; /* Index in the tuned_channel list */
  uint32_t			frequ;		/* Frequency of this RF channel in Hz
						   (may differ from RF table due to fine tuning) */
  guint				num_controls;	/* number of saved controls for this channel */
  tveng_tc_control *		controls;	/* saved controls for this channel */

  /* last used subtitle page on this channel */
  int				caption_pgno;

  guint				num_ttx_encodings;
  tveng_ttx_encoding *		ttx_encodings;

  /* Don't use this to navigate through the tuned_channel list, use
     the API instead */
  tveng_tuned_channel *prev;
  tveng_tuned_channel *next;
};

typedef struct _tv_rf_channel tv_rf_channel;

/*
 *  tv_rf_channel is a kind of iterator representing one element in
 *  a three dimensional frequency table array. The country_code, table_name
 *  and channel_name uniquely identify a channel, the country_code
 *  and table_name a frequency table. The tv_rf_channel functions
 *  move through the array. All strings are static.
 */
struct _tv_rf_channel {
	char		country_code[4];	/* ASCII ISO 3166, e.g. "US" */
	const char *	table_name;		/* ASCII identifier, e.g. "ccir" */
	const char *	domain;			/* UTF8 localized, e.g. "cable" */
	char		channel_name[8];	/* UTF8 prefix & channel number, e.g. "S21" */
	unsigned int	frequency;		/* Hz */
	unsigned int	bandwidth;		/* Hz */
	unsigned int	video_standards;	/* future stuff */

	/* private */

	const void *	_table;
	const void *	_range;
	unsigned int	_channel;
};

#define tv_rf_channel_first_table(ch) tv_rf_channel_nth_table (ch, 0)
extern tv_bool
tv_rf_channel_next_table	(tv_rf_channel *	ch);
extern tv_bool
tv_rf_channel_nth_table		(tv_rf_channel *	ch,
				 unsigned int		nth);
extern unsigned int
tv_rf_channel_table_size	(tv_rf_channel *	ch);
extern tv_bool
tv_rf_channel_table_by_name	(tv_rf_channel *	ch,
				 const char *		name);
#define tv_rf_channel_first_table_by_country(ch, country_code) \
	tv_rf_channel_table_by_name (ch, country_code)
extern tv_bool
tv_rf_channel_next_table_by_country
				(tv_rf_channel *	ch,
				 const char *		country_code);
extern const char *
tv_rf_channel_table_prefix	(tv_rf_channel *	ch,
				 unsigned int		nth);
extern tv_bool
tv_rf_channel_align		(tv_rf_channel *	ch);
extern tv_bool
tv_rf_channel_first		(tv_rf_channel *	ch);
extern tv_bool
tv_rf_channel_next		(tv_rf_channel *	ch);
extern tv_bool
tv_rf_channel_nth		(tv_rf_channel *	ch,
				 unsigned int		nth);
extern tv_bool
tv_rf_channel_by_name		(tv_rf_channel *	ch,
				 const char *		name);
extern tv_bool
tv_rf_channel_by_frequency	(tv_rf_channel *	ch,
				 unsigned int		frequency);
extern tv_bool
tv_rf_channel_next_country	(tv_rf_channel *	ch);

/* ------------------------------------------------------------------------- */

extern gboolean
tveng_tuned_channel_set_control	(tveng_tuned_channel *	tc,
				 const gchar *		name,
				 gfloat			value);
#ifdef HAVE_LIBZVBI
extern gboolean
tveng_tuned_channel_set_ttx_encoding
				(tveng_tuned_channel *	tc,
				 vbi3_pgno		pgno,
				 vbi3_ttx_charset_code	charset_code);
extern gboolean
tveng_tuned_channel_get_ttx_encoding
				(tveng_tuned_channel *	tc,
				 vbi3_ttx_charset_code *charset_code,
				 vbi3_pgno		pgno);
extern void
tveng_tuned_channel_remove_ttx_encoding
				(tveng_tuned_channel *	tc,
				 vbi3_pgno		pgno);
#endif

tveng_tuned_channel *
tveng_tuned_channel_first	(tveng_tuned_channel *list);
tveng_tuned_channel *
tveng_tuned_channel_nth		(tveng_tuned_channel *list,
				 guint			nth);
tveng_tuned_channel *
tveng_tuned_channel_by_name	(tveng_tuned_channel *	list,
				 const gchar *		name);
tveng_tuned_channel *
tveng_tuned_channel_by_rf_name	(tveng_tuned_channel *	list,
				 const gchar *		rf_name);
void
tveng_tuned_channel_insert_replace
				(tveng_tuned_channel **	list,
				 tveng_tuned_channel *	tc,
				 guint			nth,
				 gboolean		replace);
#define tveng_tuned_channel_insert(list, tc, index)			\
  tveng_tuned_channel_insert_replace (list, tc, index, FALSE)
#define tveng_tuned_channel_replace(list, tc, index)			\
  tveng_tuned_channel_insert_replace (list, tc, index, TRUE)
void
tveng_tuned_channel_move	(tveng_tuned_channel **	list,
				 tveng_tuned_channel *	tc,
				 guint			new_index);
void
tveng_tuned_channel_remove	(tveng_tuned_channel **	list,
				 tveng_tuned_channel *	tc);
void
tveng_tuned_channel_copy	(tveng_tuned_channel *	dst,
				 const tveng_tuned_channel *src);
tveng_tuned_channel *
tveng_tuned_channel_new		(const tveng_tuned_channel *tc);
void
tveng_tuned_channel_delete	(tveng_tuned_channel *	tc);

tveng_tuned_channel *
tveng_tuned_channel_list_new	(tveng_tuned_channel *	list);
void
tveng_tuned_channel_list_delete (tveng_tuned_channel **	list);
gboolean
tveng_tuned_channel_in_list	(tveng_tuned_channel *	list,
				 tveng_tuned_channel *	tc);

/* old stuff */
unsigned int
tveng_tuned_channel_num		(tveng_tuned_channel *	list);
tveng_tuned_channel *
tveng_remove_tuned_channel (gchar * rf_name, int id,
			    tveng_tuned_channel * list);

#endif /* FREQUENCIES_H */

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
