/*
 *  Zapzilla - Teletext and Closed Caption character set
 *
 *  Copyright (C) 2000-2001 Michael H. Schimek
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

/* $Id: lang.h,v 1.10 2001-04-27 20:53:54 garetxe Exp $ */

#ifndef LANG_H
#define LANG_H

#include <iconv.h>

typedef enum {
	LATIN_G0 = 1,
	LATIN_G2,
	CYRILLIC_1_G0,
	CYRILLIC_2_G0,
	CYRILLIC_3_G0,
	CYRILLIC_G2,
	GREEK_G0,
	GREEK_G2,
	ARABIC_G0,
	ARABIC_G2,
	HEBREW_G0,
	BLOCK_MOSAIC_G1,
	SMOOTH_MOSAIC_G3
} character_set;

typedef enum {
	NO_SUBSET,
	CZECH_SLOVAK,
	ENGLISH,
	ESTONIAN,
	FRENCH,
	GERMAN,
	ITALIAN,
	LETT_LITH,
	POLISH,
	PORTUG_SPANISH,
	RUMANIAN,
	SERB_CRO_SLO,
	SWE_FIN_HUN,
	TURKISH
} national_subset;

struct vbi_font_descr {
	character_set		G0;
	character_set		G2;	
	national_subset		subset;		/* applies only to LATIN_G0 */
	char *			label;		/* Latin-1 (ISO 8859-1) */
};

extern struct vbi_font_descr	font_descriptors[88];

#define VALID_CHARACTER_SET(n) ((n) < 88 && font_descriptors[n].G0)

/* Glyph codes */

#define GL_LATIN_G0				(0x0000)	/* 0x00 ... 0x1F reserved */
#define GL_LATIN_G2				(0x0080)	/* 0x80 ... 0x9F reserved */
#define GL_CYRILLIC_2_G0_ALPHA			(0x0100)
#define GL_GREEK_G0_ALPHA			(0x0140)
#define GL_ARABIC_G0_ALPHA			(0x0180)
#define GL_ARABIC_G2				(0x01C0 - 0x20)	/* 0x20 ... 0x3F only */
#define GL_HEBREW_G0_LOWER			(0x01E0)
#define GL_GRAPHICS				(0x0200)
#define GL_CONTIGUOUS_BLOCK_MOSAIC_G1		(0x0200 - 0x20)
#define GL_SEPARATED_BLOCK_MOSAIC_G1		(0x0220 - 0x20)	/* interleaved 2-2-6-6 */
#define GL_SMOOTH_MOSAIC_G3			(0x0280 - 0x20)
#define GL_ITALICS				(0x02E0)	/* repeats glyphs 0x0000 ... 0x017F (not coded, -> attr_char.italic) */
#define GL_CAPTION				(0x0800)
#define GL_DRCS					(0x8000)	/* vector 0 .. n << 8, char 0 ... 47, not in the font file */

#define GL_SPACE				(GL_LATIN_G0 + ' ')

static inline int
gl_isalnum(int glyph)
{
	glyph &= 0xFFFF;

	if (glyph >= GL_DRCS)
		return 0;
	if (glyph >= GL_ITALICS)
		return 1;
	if (glyph >= GL_GRAPHICS)
		return 0;
	else
		return 1;
}

static inline int
gl_isg1(int glyph)
{
	return (glyph >= GL_GRAPHICS
		&& glyph < GL_GRAPHICS + 0x80);
}

/*
 *  Glyph modifiers:
 *    Regular:	0 ... 15 << 20, diacritical mark from G2 0x40 ... 0x4F,
 *              1 << 19 'uppercase', 1 << 18 'centered' (just for aesthetical purposes)
 *    DRCS:	0 ... 63 << 16, DRCS colour lut offset
 *
 *  Closed caption characters have no diacritical marks, and there are no
 *  DRCS in CC fmt_pages.
 */

/* Translate raw Teletext character code c = 32 ... 127 to glyph code */

extern int		glyph_lookup(character_set s, national_subset n, int c);

/* 
 *  Compose a new glyph from base glyph code and diacritical
 *  mark = 0 ... 15 (Teletext only)
 */
extern int		compose_glyph(int glyph, int mark);

/* Translate glyph code to Unicode (0 == no mapping) */

extern int		glyph2unicode(int glyph);

/*
 *  Translate glyph code to non-zero Latin-1 character (ISO 8859-1)
 *  Range 0x20 ... 0x7F, 0xA0 ... 0xFF (0x20 == no mapping)
 */
extern unsigned char	glyph2latin(int glyph);

/*
 *  The iconv source must be "UCS2".
 *  Returns *8 bit* code equv. glyph; -unicode if not representable
 *  or target is not 8 bit; 0x20 or gfx_substitute if no unicode exists.
 */
extern int		glyph_iconv(iconv_t cd, int glyph, int gfx_substitute);

#if 0
/*
 * i18n support
 */
#ifdef ENABLE_NLS
#    include <libintl.h>
#    define _(String) gettext (String)
#    ifdef gettext_noop
#        define N_(String) gettext_noop (String)
#    else
#        define N_(String) (String)
#    endif
#else
/* Stubs that do something close enough.  */
#    define textdomain(String) (String)
#    define gettext(String) (String)
#    define dgettext(Domain,Message) (Message)
#    define dcgettext(Domain,Message,Type) (Message)
#    define bindtextdomain(Domain,Directory) (Domain)
#    define _(String) (String)
#    define N_(String) (String)
#endif
#endif

#endif /* LANG_H */
