/*
 *  Zapzilla - Teletext character set
 *
 *  Copyright (C) 2000 Michael H. Schimek
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

/* $Id: lang.c,v 1.7 2001-01-05 03:51:52 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <wchar.h>
#include "lang.h"

/* 
 *  ETS 300 706 Table 32, 33, 34
 */
font_descriptor
font_descriptors[88] = {
	/* 0 - Western and Central Europe */
	{ LATIN_G0, LATIN_G2, ENGLISH,		"English" },
	{ LATIN_G0, LATIN_G2, GERMAN,		"Deutsch" },
	{ LATIN_G0, LATIN_G2, SWE_FIN_HUN,	"Svenska / Suomi / Magyar" },
	{ LATIN_G0, LATIN_G2, ITALIAN,		"Italiano" },
	{ LATIN_G0, LATIN_G2, FRENCH,		"Fran�ais" },
	{ LATIN_G0, LATIN_G2, PORTUG_SPANISH,	"Portugu�s / Espa�ol" },
	{ LATIN_G0, LATIN_G2, CZECH_SLOVAK,	"Cesky / Slovencina" },
	{ LATIN_G0, LATIN_G2, NO_SUBSET		},

	/* 8 - Eastern Europe */
	{ LATIN_G0, LATIN_G2, POLISH,		"Polski" },
	{ LATIN_G0, LATIN_G2, GERMAN,		"Deutsch" },
	{ LATIN_G0, LATIN_G2, SWE_FIN_HUN,	"Svenska / Suomi / Magyar" },
	{ LATIN_G0, LATIN_G2, ITALIAN,		"Italiano" },
	{ LATIN_G0, LATIN_G2, FRENCH,		"Fran�ais" },
	{ LATIN_G0, LATIN_G2, NO_SUBSET		},
	{ LATIN_G0, LATIN_G2, CZECH_SLOVAK,	"Cesky / Slovencina" },
	{ LATIN_G0, LATIN_G2, NO_SUBSET		},

	/* 16 - Western Europe and Turkey */
	{ LATIN_G0, LATIN_G2, ENGLISH,		"English" },
	{ LATIN_G0, LATIN_G2, GERMAN,		"Deutsch" },
	{ LATIN_G0, LATIN_G2, SWE_FIN_HUN,	"Svenska / Suomi / Magyar" },
	{ LATIN_G0, LATIN_G2, ITALIAN,		"Italiano" },
	{ LATIN_G0, LATIN_G2, FRENCH,		"Fran�ais" },
	{ LATIN_G0, LATIN_G2, PORTUG_SPANISH,	"Portugu�s / Espa�ol" },
	{ LATIN_G0, LATIN_G2, TURKISH,		"T�rk�e" },
	{ LATIN_G0, LATIN_G2, NO_SUBSET		},

	/* 24 - Central and Southeast Europe */
	{ LATIN_G0, LATIN_G2, NO_SUBSET		},
	{ LATIN_G0, LATIN_G2, NO_SUBSET		},
	{ LATIN_G0, LATIN_G2, NO_SUBSET		},
	{ LATIN_G0, LATIN_G2, NO_SUBSET		},
	{ LATIN_G0, LATIN_G2, NO_SUBSET		},
	{ LATIN_G0, LATIN_G2, SERB_CRO_SLO,	"Srbski / Hrvatski / Slovenscina" },
	{ LATIN_G0, LATIN_G2, NO_SUBSET		},
	{ LATIN_G0, LATIN_G2, RUMANIAN,		"Rom�na" },

	/* 32 - Cyrillic */
	{ CYRILLIC_1_G0, CYRILLIC_G2, NO_SUBSET, "Srpski / Hrvatski" },
	{ LATIN_G0, LATIN_G2, GERMAN,		"Deutsch" },
	{ LATIN_G0, LATIN_G2, ESTONIAN,		"Eesti" },
	{ LATIN_G0, LATIN_G2, LETT_LITH,	"Lettish / Lietuviskai" },
	{ CYRILLIC_2_G0, CYRILLIC_G2, NO_SUBSET, "Russky / Balgarski " },
	{ CYRILLIC_3_G0, CYRILLIC_G2, NO_SUBSET, "Ukrayins'ka" },
	{ LATIN_G0, LATIN_G2, CZECH_SLOVAK,	"Cesky / Slovencina" },
	{ 0, 0, NO_SUBSET,			},

	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},

	/* 48 */
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ LATIN_G0, LATIN_G2, TURKISH,		"T�rk�e" },
	{ GREEK_G0, GREEK_G2, NO_SUBSET,	"Ellinika'" },

	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},

	/* 64 - Arabic */
	{ LATIN_G0, ARABIC_G2, ENGLISH,		"Alarabia / English" },
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ LATIN_G0, ARABIC_G2, FRENCH,		"Alarabia / Fran�ais" },
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ ARABIC_G0, ARABIC_G2, NO_SUBSET,	"Alarabia" },

	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},

	/* 80 */
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ HEBREW_G0, ARABIC_G2, NO_SUBSET,	"Ivrit" },
	{ 0, 0, NO_SUBSET			},
	{ ARABIC_G0, ARABIC_G2, NO_SUBSET,	"Alarabia" },
};

/*
 *  ETS 300 706 Table 36: Latin National Option Sub-sets
 *
 *  Latin G0 character code to glyph mapping per national_subset,
 *  unmodified codes (NO_SUBSET) in row zero.
 */
static const unsigned int
national_subst[14][13] = {
	{ 0x23, 0x24, 0x40, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x7B, 0x7C, 0x7D, 0x7E },

	{ 0x000023,       0xA00000 + 'u', 0xF00000 + 'c', 0x000016,       0xF00000 + 'z', 0x200000 + 'y', 0x2400F5,       0xF00000 + 'r', 0x200000 + 'e', 0x240000 + 'a', 0xF00000 + 'e', 0x200000 + 'u', 0xF00000 + 's' },
	{ 0x0000A3,       0x0000A4,       0x000040,       0x0000AC,       0x0000BD,       0x0000AE,       0x0000AD,       '#',            0x0000D0,       0x0000BC,       0x0001FC,       0x0000BE,       0x0000B8 },
	{ 0x000023,       0x400000 + 'o', 0xF00013,       0x880001,       0x88000F,       0xF0001A,       0x880015,       0x40000F,       0xF00000 + 's', 0x000004,       0x800000 + 'o', 0xF00000 + 'z', 0x800000 + 'u' },
	{ 0x200000 + 'e', 0x00000D,       0x100000 + 'a', 0x800000 + 'e', 0x300000 + 'e', 0x100000 + 'u', 0x3400F5,       '#',            0x100000 + 'e', 0x340000 + 'a', 0x300000 + 'o', 0x300000 + 'u', 0xB00000 + 'c' },
	{ 0x000023,       0x0000A4,       0x0000A7,       0x880001,       0x88000F,       0x880015,       0x00005E,       0x5F,           0x0000B0,       0x000004,       0x800000 + 'o', 0x800000 + 'u', 0x0000FB },
	{ 0x0000A3,       0x0000A4,       0x200000 + 'e', 0x0000B0,       0xB00000 + 'c', 0x0000AE,       0x0000AD,       '#',            0x100000 + 'u', 0x100000 + 'a', 0x100000 + 'o', 0x100000 + 'e', 0x1000F5 },
	{ 0x000023,       0x0000A4,       0xF00013,       0x700000 + 'e', 0xB00000 + 'e', 0xF0001A,       0xF00000 + 'c', 0x500000 + 'u', 0xF00000 + 's', 0xE40000 + 'a', 0xE00000 + 'u', 0xF00000 + 'z', 0xB40000 + 'i' },
	{ 0x000023,       0x200000 + 'n', 0xE40000 + 'a', 0x00001E,       0x200013,       0x0000E8,       0x200000 + 'c', 0x200000 + 'o', 0xE00000 + 'e', 0x700000 + 'z', 0x200000 + 's', 0x0000F8,       0x200000 + 'z' },
	{ 0xB00000 + 'c', 0x0000A4,       0x0000A1,       0x240000 + 'a', 0x200000 + 'e', 0x2400F5,       0x200000 + 'o', 0x200000 + 'u', 0x0000BF,       0x800000 + 'u', 0x400000 + 'n', 0x100000 + 'e', 0x100000 + 'a' },
	{ 0x000023,       0x000024,       0xB00000 + 'T', 0x300002,       0xB00000 + 'S', 0xF00001,       0x300006,       0x0000F5,       0xB00000 + 't', 0x340000 + 'a', 0xB00000 + 's', 0xF40000 + 'a', 0x3400F5 },
	{ 0x000023,       0x880005,       0xF00003,       0x200003,       0xF0001A,       0x0000E2,       0xF00013,       0x800000 + 'e', 0xF00000 + 'c', 0x200000 + 'c', 0xF00000 + 'z', 0x0000F2,       0xF00000 + 's' },
	{ 0x000023,       0x000024,       0x200005,       0x880001,       0x88000F,       0x000017,       0x880015,       0x00005F,       0x200000 + 'e', 0x000004,       0x800000 + 'o', 0xA40000 + 'a', 0x800000 + 'u' },
	{ 0x00001F,       0x600000 + 'g', 0x780009,       0xB00000 + 'S', 0x88000F,       0xB00000 + 'C', 0x880015,       0x600007,       0x0000F5,       0xB00000 + 's', 0x800000 + 'o', 0xB00000 + 'c', 0x800000 + 'u' }
};

/*
 *  ETS 300 706 Table 38: Cyrillic G0 Primary Set - Option 1 - Serbian/Croatian
 *
 *  G0 character code 0x40 ... 0x7F to glyph mapping.
 */
static const unsigned short
cyrillic_1_g0_alpha_subst[64] = {
	0x011E, 0x0101, 0x0102, 0x0103,	0x0104, 0x0105, 0x0106, 0x0107,
	0x0108, 0x0109,    'J', 0x010B,	0x010C, 0x010D, 0x010E, 0x010F,
	0x0080, 0x0081, 0x0112, 0x0113, 0x0114, 0x0115, 0x0117, 0x0082,
	0x0083, 0x0084, 0x011A, 0x0085, 0x0116, 0x0086, 0x011B, 0x0087,
	0x013E, 0x0121, 0x0122, 0x0123, 0x0124, 0x0125, 0x0126, 0x0127,
	0x0128, 0x0129,    'j', 0x012B,	0x012C, 0x012D, 0x012E, 0x012F,
	0x0090, 0x0091, 0x0132, 0x0133, 0x0134, 0x0135, 0x0137, 0x0092,
	0x0093, 0x0094, 0x013A, 0x0095, 0x0136, 0x0096, 0x013B, 0x013F
};

/*
 *  ETS 300 706 Table 43: Greek G2 Supplementary Set
 *
 *  G2 character code 0x20 ... 0x7F to glyph mapping.
 */
static const unsigned char
greek_g2_subst[96] = {
	0xA0,  'a',  'b', 0xA3,  'e',  'h',  'i', 0xA7,
	 ':', 0xA9, 0xAA,  'k', 0xAC, 0xAD, 0xAE, 0xAF,
	0xB0, 0xB1, 0xB2, 0xB3,  'x',  'm',  'n',  'p',
	0xB8, 0xB9, 0xBA,  't', 0xBC, 0xBD, 0xBE,  'x',
	0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
	0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
	 '?', 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
	0xD8, 0x89, 0x8A, 0x8B, 0xDC, 0xDD, 0xDE, 0xDF,
	 'C',  'D',  'F',  'G',  'J',  'L',  'Q',  'R',
	 'S',  'U',  'V',  'W',  'Y',  'Z', 0x8C, 0x8D,
	 'c',  'd',  'f',  'g',  'j',  'l',  'q',  'r',
	 's',  'u',  'v',  'w',  'y',  'z', 0x8E, 0xFF,
};

/* XXX optimize me */

int
glyph_lookup(character_set s, national_subset n, int c)
{
	int i;

	switch (s) {
	case LATIN_G0:
		for (i = 0; i < 13; i++)
			if (c == national_subst[0][i])
				return national_subst[n][i];
		return GL_LATIN_G0 + c;

	case LATIN_G2:
		return GL_LATIN_G2 + c;

	case CYRILLIC_1_G0:
		if (c == 0x24)
			return 0x0000A4;
		if (c <= 0x3F)
			return GL_LATIN_G0 + c;
		return cyrillic_1_g0_alpha_subst[c - 0x40];

	case CYRILLIC_2_G0:
		if (c == 0x24)
			return 0x0000A4;
		if (c == 0x26)
			return 0x000097;
		if (c <= 0x3F)
			return GL_LATIN_G0 + c;
		return GL_CYRILLIC_2_G0_ALPHA - 0x40 + c;

	case CYRILLIC_3_G0:
		if (c == 0x24)
			return 0x0000A4;
		if (c == 0x26)
			return 0x00000D;
		if (c <= 0x3F)
			return GL_LATIN_G0 + c;
		if (c == 0x59)
			return GL_LATIN_G0 + 'I';
		if (c == 0x79)
			return GL_LATIN_G0 + 'i';
		if (c == 0x5C)
			return 0x000088;
		if (c == 0x5F)
			return 0x000010;
		if (c == 0x67)
			return 0x000099;
		if (c == 0x7C)
			return 0x000098;
		return GL_CYRILLIC_2_G0_ALPHA - 0x40 + c;

	case CYRILLIC_G2:
		if (c == 0x59)
			return 0x0000E8;
		if (c == 0x5A)
			return 0x0000F8;
		if (c == 0x5B)
			return 0x0000FB;
		if (c <= 0x5F)
			return GL_LATIN_G2 + c;
		return GL_LATIN_G0 + "DEFGIJKLNQRSUVWZdefgijklnqrsuvwz"[c - 0x60];

	case GREEK_G0:
		if (c == 0x24)
			return 0x0000A4;
		if (c == 0x3C)
			return 0x0000AB;
		if (c == 0x3E)
			return 0x0000BB;
		if (c <= 0x3F)
			return GL_LATIN_G0 + c;
		return GL_GREEK_G0_ALPHA - 0x40 + c;

	case GREEK_G2:
		return greek_g2_subst[c - 0x20];

	case ARABIC_G0:
		if (c == 0x23)
			return 0x0000A3;
		if (c == 0x24)
			return 0x0000A4;
		if (c == 0x26)
			return 0x00009D;
		if (c == 0x27)
			return 0x00009E;
		if (c == 0x28)
			return GL_LATIN_G0 + ')';
		if (c == 0x29)
			return GL_LATIN_G0 + '(';
		if (c == 0x2C)
			return 0x00009C;
		if (c == 0x3B)
			return 0x00009B;
		if (c == 0x3C)
			return GL_LATIN_G0 + '>';
		if (c == 0x3E)
			return GL_LATIN_G0 + '<';
		if (c == 0x3F)
			return 0x00009F;
		return GL_ARABIC_G0_ALPHA - 0x40 + c;

	case ARABIC_G2:
		if (c <= 0x3F)
			return GL_ARABIC_G2 + c;
		if (c == 0x40)
			return GL_LATIN_G0 + 0x100000 + 'a';
		if (c == 0x60)
			return GL_LATIN_G0 + 0x200000 + 'e';
		if (c == 0x4B)
			return GL_LATIN_G0 + 0x800000 + 'e';
		if (c == 0x4C)
			return GL_LATIN_G0 + 0x300000 + 'e';
		if (c == 0x4D)
			return GL_LATIN_G0 + 0x100000 + 'u';
		if (c == 0x4E)
			return 0xF4F5;
		if (c == 0x4F)
			return 0x008F;
		if (c == 0x4B)
			return GL_LATIN_G0 + 0x340000 + 'a';
		if (c == 0x4C)
			return GL_LATIN_G0 + 0x300000 + 'o';
		if (c == 0x4D)
			return GL_LATIN_G0 + 0x300000 + 'u';
		if (c == 0x4E)
			return GL_LATIN_G0 + 0xB00000 + 'c';
		return GL_LATIN_G0 + c;

	case HEBREW_G0:
		if (c == 0x23)
			return 0x0000A3;
		if (c == 0x24)
			return 0x0000A4;
		if (c == 0x4B)
			return 0x0000AC;
		if (c == 0x4C)
			return 0x0000BD;
		if (c == 0x4D)
			return 0x0000AE;
		if (c == 0x4E)
			return 0x0000AD;
		if (c == 0x4F)
			return GL_LATIN_G0 + '#';
		return GL_HEBREW_G0_LOWER - 0x60 + c;

	default:
		return GL_LATIN_G0 + '?';
	}
}

int
compose_glyph(int glyph, int mark)
{
	int mm = 1 << mark;

	/* requested by the aestethics department */
	if (0xA5FE & mm) {
		if (glyph >= 0x40 && glyph <= 0x60
		    && (0x063CDFAA & (1 << glyph))) {
			if (glyph == 'I')
				glyph = (0x0048 & mm) ? 0x040006 : 0x040009;
			else if (glyph == 'A' && (0x0048 & mm))
				glyph =  0x000002;
			else
				glyph -= 0x000040; /* reduced uppercase */
			if (0x01A0 & mm)
				glyph += 0x080000;
		} else if (glyph == 'a')
			glyph = 0x040061;
		else if (glyph == 'i')
			glyph = (mark == 1) ? 0x0000F5 : 0x0400F5;
		else if (glyph == 'j')
			glyph = 0x000011;
	}

	glyph |= mark << 20;

	if (glyph == 0xA00001)
		glyph = 0x000017;
	else if (glyph == 0x800009)
		glyph = 0x000010;
	else if (glyph == 0x800061)
		glyph = 0x000004;
	else if (glyph == 0xF00074)
		glyph = 0x000016;
	else if (glyph == 0x8000F5)
		glyph = 0x00000D;

	return glyph;
}

#define block 0x25A0

static const unsigned short
unicode_forward[] = {
	/* Reserved 0x00 ... 0x1F */
	/* 0x1F (Turkish national subset 2/3, currency sign) Unicode? */
	0x0000, 0x0041, 0x0041, 0x0043, 0x00E4, 0x0045, 0x0049, 0x0047,
	0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x00EF, 0x004E, 0x004F,
	0x00CF, 0x0000, 0x0052, 0x0053, 0x0054, 0x0055, 0x0165, 0x00C5,
	0x0000, 0x0059, 0x005A, 0x0000, 0x0000, 0x0000, 0x01B5, 0x0000,

	/* Latin G0 */
	0x0020, 0x0021, 0x0022, 0x0023, 0x00A4, 0x0025, 0x0026, 0x0027,
	0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
	0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
	0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,

	0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
	0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
	0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
	0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,

	0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
	0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,
	0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
	0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, block,

	/* Reserved 0x80 ... 0x9F */
	/* can't identify 0x8F, 0x9D, 0x9E (Arabic G2 4/F, Arabic G0 2/6 2/7) */
	0x041F, 0x040C, 0x0403, 0x0409, 0x040A, 0x040B, 0x0402, 0x040F,
	0x0404, 0x038A, 0x038E, 0x038F, 0x0386, 0x0389, 0x0388, 0x0000,
	0x043F, 0x045C, 0x0453, 0x0459, 0x045A, 0x045B, 0x0452, 0x044B,
	0x0454, 0x0433, 0x0000, 0x061B, 0x060C, 0x0000, 0x0000, 0x061F,

	/* Latin G2 */
	0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x0024, 0x00A5, 0x0023, 0x00A7,
	0x00A4, 0x2018, 0x201C, 0x00AB, 0x2190, 0x2191, 0x2192, 0x2193,
	0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00D7, 0x00B5, 0x00B6, 0x00B7,
	0x00F7, 0x2019, 0x201D, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF,

	0x0020, 0x02CB, 0x02CA, 0x02C6, 0x02DC, 0x02C9, 0x02D8, 0x02D9,
	0x00A8, 0x0000, 0x02DA, 0x02CF, 0x00B8, 0x02BA, 0x02DB, 0x02C7,
	0x2013, 0x00B9, 0x00AE, 0x00A9,	0x2122, 0x266A, 0x20A0, 0x2030,
	0x0251, 0x0000, 0x0000, 0x0000, 0x215B, 0x215C, 0x215D, 0x215E,

	0x2126, 0x00C6, 0x00D0, 0x00AA, 0x0126, 0x0000, 0x0132, 0x013F,
	0x0141, 0x00D8, 0x0152, 0x00BA, 0x00DE, 0x0166, 0x014A, 0x0149,
	0x0138, 0x00E6, 0x0111, 0x00F0, 0x0127, 0x0131, 0x0133, 0x0140,
	0x0142, 0x00F8, 0x0153, 0x00DF, 0x00FE, 0x0167, 0x0148, block,

	/* Cyrillic G0 Alpha, Option 2 Russian */
	0x042E, 0x0410, 0x0411, 0x0426, 0x0414, 0x0415, 0x0424, 0x0413,
	0x0425, 0x0418, 0x040D, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E,
	0x041F, 0x042F, 0x0420, 0x0421, 0x0422, 0x0423, 0x0416, 0x0412,
	0x042C, 0x042A, 0x0417, 0x0428, 0x042D, 0x0429, 0x0427, 0x042B,

	0x044E, 0x0430, 0x0431, 0x0446, 0x0434, 0x0435, 0x0444, 0x0433, 
	0x0445, 0x0438, 0x045D, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E,
	0x043F, 0x044F, 0x0440, 0x0441, 0x0442, 0x0443, 0x0436, 0x0432,
	0x044C, 0x044A, 0x0437, 0x0448, 0x044D, 0x0449, 0x0447, block,

	/* Greek G0 Alpha */
	0x0390, 0x0391, 0x0392, 0x0393, 0x0394, 0x0395, 0x0396, 0x0397,
	0x0398, 0x0399, 0x039A, 0x039B, 0x039C, 0x039D, 0x039E, 0x039F,
	0x03A0, 0x03A1, 0x0374, 0x03A3, 0x03A4, 0x03A5, 0x03A6, 0x03A7,
	0x03A8, 0x03A9, 0x03AA, 0x03AB, 0x03AC, 0x03AD, 0x03AE, 0x03AF,

	0x03B0, 0x03B1, 0x03B2, 0x03B3, 0x03B4, 0x03B5, 0x03B6, 0x03B7,
	0x03B8, 0x03B9, 0x03BA, 0x03BB, 0x03BC, 0x03BD, 0x03BE, 0x03BF,
	0x03C0, 0x03C1, 0x03C2, 0x03C3, 0x03C4, 0x03C5, 0x03C6, 0x03C7,
	0x03C8, 0x03C9, 0x03CA, 0x03CB, 0x03CC, 0x03CD, 0x03CE, block,

	/* Arabic G0 0x20 ... 0x7F */
	/* can't identify 0x0000 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,

	0x0630, 0x0631, 0x0632, 0x0633, 0x0634, 0x0635, 0x0636, 0x0637,
	0x0638, 0x0639, 0x063A, 0x0000, 0x0000, 0x0000, 0x0000, 0x0023,
	0x0640, 0x0641, 0x0642, 0x0643, 0x0644, 0x0645, 0x0646, 0x0647,
	0x0648, 0x0649, 0x064A, 0x062B, 0x062C, 0x062D, 0x062E, block,

	/* Arabic G2 0x20 ... 0x3F */
	/* can't identify 0x0000 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0660, 0x0661, 0x0662, 0x0663, 0x0664, 0x0665, 0x0666, 0x0667,
	0x0668, 0x0669, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,

	/* Hebrew G0 0x60 .. 0x7F */
	0x05D0, 0x05D1, 0x05D2, 0x05D3, 0x05D4, 0x05D5, 0x05D6, 0x05D7,
	0x05D8, 0x05D9, 0x05DA, 0x05DB, 0x05DC, 0x05DD, 0x05DE, 0x05DF,
	0x05E0, 0x05E1, 0x05E2, 0x05E3, 0x05E4, 0x05E5, 0x05E6, 0x05E7,
	0x05E8, 0x05E9, 0x05EA, 0x20AA, 0x2016, 0x00BE, 0x00F7, block
};

static const unsigned short
diacritical_transcript[] = {
	0x0020, 0x0060, 0x00B4, 0x005E, 0x007E, 0x00AF, 0x0020, 0x0020,
	0x00A8, 0x0020, 0x00B0, 0x00B8, 0x005F, 0x0020, 0x0020, 0x0020,
};

/*
 *  Translates the ISO 6937 composed glyphs to Unicode
 */
static const unsigned short
unicode_reverse[] = {
	/* 0x00C0 */
	0x1001, 0x2001, 0x3001, 0x4001,	0x8001, 0x0017, 0x00E1, 0xB043,
	0x1005, 0x2005, 0x3005, 0x8005, 0x1009, 0x2009, 0x3009, 0x0010,
	0x00E2, 0x400E, 0x100F, 0x200F, 0x300F, 0x400F, 0x800F, 0x00D7,
	0x00E9, 0x1015, 0x2015, 0x3015, 0x8015, 0x2019, 0x00EC, 0x00FB,

	/* 0x00E0 */
	0x1061, 0x2061, 0x3061, 0x4061, 0x0004, 0xA061, 0x00F1, 0xB063,
	0x1065, 0x2065, 0x3065, 0x8065, 0x10F5, 0x20F5, 0x30F5, 0x000D,
	0x00F3, 0x406E, 0x106F, 0x206F, 0x306F, 0x406F, 0x806F, 0x00B8,
	0x00F9, 0x1075, 0x2075, 0x3075, 0x8075, 0x2079, 0x00FC, 0x8079,

	/* 0x0100 */
	0x5001, 0x5061, 0x6001, 0x6061,	0xE001, 0xE061, 0x2003, 0x2062,
	0x3003, 0x3063, 0x7003, 0x7063,	0xF003, 0xF063, 0xF044, 0xF064,
	0x00E2, 0x00F2, 0x5005, 0x5065, 0x6005, 0x6065, 0x7005, 0x7065,
	0xE005, 0xE065, 0xF005, 0xF065,	0x3007, 0x3067, 0x6007, 0x6067,

	/* 0x0120 */
	0x7007, 0x7067, 0x0000, 0x0000, 0x3008, 0x3068, 0x00E4, 0x00F4,
	0x4009, 0x40F5, 0x5009, 0x50F5,	0x6009, 0x60F5, 0xE009, 0xE069,
	0x7009, 0x00F5, 0x00E6, 0x00F6,	0x300A, 0x3011, 0x0000, 0x0000,
	0x00F0, 0x200C, 0x206C, 0x0000,	0x0000, 0xF00C, 0xF06C, 0x00E7,
	
	/* 0x0140 */
	0x00F7, 0x00E8, 0x00F8, 0x200E, 0x206E, 0x0000, 0x0000, 0xF00E,
	0xF06E, 0x00EF, 0x00EE, 0x00FE,	0x500F, 0x506F, 0x600F, 0x606F,
	0xD00F, 0xD06F, 0x00EA, 0x00FA,	0x2012, 0x2072, 0x0000, 0x0000,
	0xF012, 0xF072, 0x2013, 0x2073,	0x3013, 0x3073, 0xB013, 0xB073,

	/* 0x0160 */
	0xF013, 0xF073, 0xB014, 0xB074,	0xF013, 0x0016, 0x00ED, 0x00FD,
	0x4015, 0x4075, 0x5015, 0x5075,	0x6015, 0x6075, 0xA015, 0xA075,
	0xD015, 0xD075, 0xE015, 0xE075,	0x3057, 0x3077, 0x3019, 0x3079,
	0x8019, 0x201A, 0x207A, 0x701A,	0x707A, 0xF01A, 0xF07A, 0x0000,
};

/*
 *  These characters mimic a few common block mosaic, smooth mosaic
 *  or line drawing character patterns, eg. line separators
 */
static const unsigned char
gfx_transcript[] = {
/* 2 */	0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2D, 0x00, 0x00, 0x00,
/* 3 */	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x2B, 0x00, 0x00,
/* 2 */	0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2D, 0x00, 0x00, 0x00,
/* 3 */	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x2B, 0x00, 0x00,
/* 6 */	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2B, 0x00,
/* 7 */	0x5F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20,
/* 6 */	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2B, 0x00,
/* 7 */	0x5F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20,
/* 2 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 3 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 4 */ 0x2B, 0x2B, 0x2B, 0x2B, 0x3C, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2B, 0x2A, 0x2A, 0x6F,
/* 5 */ 0x7C, 0x2D, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x3E, 0x3C, 0x00, 0x00, 0x00,
/* 6 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 7 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

int
glyph2unicode(int glyph)
{
	int i;

	glyph &= 0xF3FFFF;

	if ((glyph & 0xFFFF) >= GL_DRCS)
		return 0x0020; /* dynamically redefinable character */

	if (glyph < sizeof(unicode_forward) / sizeof(unicode_forward[0]))
		return unicode_forward[glyph];

	if (glyph < GL_ITALICS)
		return 0x0020; /* block mosaic, smooth mosaic, line drawing character */

	glyph = (glyph & 0x3FF) + ((glyph >> 8) & 0xF000);

	for (i = 0; i < sizeof(unicode_reverse) / sizeof(unicode_reverse[0]); i++)
		if (glyph == unicode_reverse[i])
			return 0x00C0 + i;

	return 0x0000;
}

unsigned char
glyph2latin(int glyph)
{
	int i, c;

	glyph &= 0xF3FFFF;

	if ((glyph & 0xFFFF) >= GL_DRCS)
		return 0x20; /* dynamically redefinable character */

	if (glyph >= 0x00C0 && glyph <= 0x00CF)
		return diacritical_transcript[glyph - 0x00C0];

	if (glyph < sizeof(unicode_forward) / sizeof(unicode_forward[0])) {
		c = unicode_forward[glyph];
		return (c && c < 0x0100) ? c : 0x20;
	}

	if (glyph < GL_ITALICS)	/* block mosaic, smooth mosaic, line drawing character */
		return gfx_transcript[glyph - GL_GRAPHICS] ? : 0x20;

	glyph = (glyph & 0x3FF) + ((glyph >> 8) & 0xF000);

	for (i = 0; i < sizeof(unicode_reverse) / sizeof(unicode_reverse[0]); i++)
		if (glyph == unicode_reverse[i])
			return (i < 64) ? (0x00C0 + i) : 0x20;

	return 0x20;
}

int
glyph_iconv(iconv_t cd, int glyph, int gfx_substitute)
{
	int i, u;
	unsigned char uc[2], *up = uc;
	unsigned char c, *cp = &c;
	size_t in = 4, out = 1;

	glyph &= 0xF3FFFF;

	if ((glyph & 0xFFFF) >= GL_DRCS)
		return gfx_substitute; /* dynamically redefinable character */

	if (glyph < sizeof(unicode_forward) / sizeof(unicode_forward[0])) {
		if (!(u = unicode_forward[glyph]))
			return 0x20;
		else if (u == '@')
			return '@';

		uc[0] = u >> 8; /* network order */
		uc[1] = u;

		if (iconv(cd, (const char **) &up, &in, (char **) &cp, &out) < 1 || c == '@')
			return -u;
		else
			return c;
	}

	if (glyph < GL_ITALICS)	/* block mosaic, smooth mosaic, line drawing character */
		return gfx_transcript[glyph - GL_GRAPHICS] ? : gfx_substitute;

	glyph = (glyph & 0x3FF) + ((glyph >> 8) & 0xF000);

	for (i = 0; i < sizeof(unicode_reverse) / sizeof(unicode_reverse[0]); i++)
		if (glyph == unicode_reverse[i]) {
			u = 0x00C0 + i;

			uc[0] = u >> 8; /* network order */
			uc[1] = u;

			if (iconv(cd, (const char **) &up, &in, (char **) &cp, &out) < 1 || c == '@')
				return -u;
			else
				return c;
		}

	return 0x20;
}
