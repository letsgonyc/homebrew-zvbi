/*
 *  libzvbi - Teletext and Closed Caption character set
 *
 *  Copyright (C) 2000-2003 Michael H. Schimek
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

/* $Id: lang.c,v 1.4.2.3 2004-04-05 04:42:27 mschimek Exp $ */

#include "../config.h"

#include <assert.h>
#include <inttypes.h>		/* uint8_t, uint16_t */
#include <stdio.h>		/* FILE */
#include <stdlib.h>		/* exit() */
#include "misc.h"		/* N_ELEMENTS() */
#include "lang.h"

#define UNUSED(n) { n, 0, 0, 0, { NULL, } }

/* Teletext character set designation defined in
   ETS 300 706 Table 32, 33, 34 */
static const vbi_character_set
character_set_table [88] = {
	{  0, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_ENGLISH,			{ "en", } },
	{  1, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_GERMAN,			{ "de", } },
	{  2, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_SWEDISH_FINNISH_HUNGARIAN,	{ "sv", "fi", "hu", } },
	{  3, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_ITALIAN,			{ "it", } },
	{  4, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_FRENCH,			{ "fr", } },
	{  5, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_PORTUGUESE_SPANISH,	{ "es", "pt", } },
	{  6, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_CZECH_SLOVAK,		{ "cs", "sk", } },
	{  7, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_NONE,			{ "en", } },

	{  8, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_POLISH,			{ "pl", } },
	{  9, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_GERMAN,			{ "de", } },
	{ 10, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_SWEDISH_FINNISH_HUNGARIAN,	{ "sv", "fi", "hu", } },
	{ 11, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_ITALIAN,			{ "it", } },
	{ 12, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_FRENCH,			{ "fr", } },
	{ 13, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_NONE,			{ "en", } },
	{ 14, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_CZECH_SLOVAK,		{ "cs", "sk", } },
	{ 15, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_NONE,			{ "en", } },

	{ 16, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_ENGLISH,			{ "en", } },
	{ 17, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_GERMAN,			{ "de", } },
	{ 18, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_SWEDISH_FINNISH_HUNGARIAN,	{ "sv", "fi", "hu", } },
	{ 19, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_ITALIAN,			{ "it", } },
	{ 20, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_FRENCH,			{ "fr", } },
	{ 21, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_PORTUGUESE_SPANISH,	{ "es", "pt", } },
	{ 22, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_TURKISH,			{ "tr", } },
	{ 23, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_NONE,			{ "en", } },

	{ 24, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_NONE,			{ "en", } },
	{ 25, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_NONE,			{ "en", } },
	{ 26, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_NONE,			{ "en", } },
	{ 27, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_NONE,			{ "en", } },
	{ 28, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_NONE,			{ "en", } },
	{ 29, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_SERBIAN_CROATIAN_SLOVENIAN, { "sr", "hr", "sl", } },
	{ 30, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_NONE,			{ "en", } },
	{ 31, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_RUMANIAN,			{ "ro", } },

	{ 32, VBI_CHARSET_CYRILLIC1_G0, VBI_CHARSET_CYRILLIC_G2, VBI_SUBSET_NONE, 		{ "sr", "hr", "sl", } },
	{ 33, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_GERMAN,			{ "de", } },
	{ 34, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_ESTONIAN,			{ "et", } },
	{ 35, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_LETTISH_LITHUANIAN,	{ "lv", "lt", } },
	{ 36, VBI_CHARSET_CYRILLIC2_G0, VBI_CHARSET_CYRILLIC_G2, VBI_SUBSET_NONE,		{ "ru", "bg", } },
	{ 37, VBI_CHARSET_CYRILLIC3_G0, VBI_CHARSET_CYRILLIC_G2, VBI_SUBSET_NONE,		{ "uk", } },
	{ 38, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_CZECH_SLOVAK,		{ "cs", "sk", } },
	UNUSED (39),

	UNUSED (40),
	UNUSED (41),
	UNUSED (42),
	UNUSED (43),
	UNUSED (44),
	UNUSED (45),
	UNUSED (46),
	UNUSED (47),

	UNUSED (48),
	UNUSED (49),
	UNUSED (50),
	UNUSED (51),
	UNUSED (52),
	UNUSED (53),
	{ 54, VBI_CHARSET_LATIN_G0, VBI_CHARSET_LATIN_G2, VBI_SUBSET_TURKISH,			{ "tr", } },
	{ 55, VBI_CHARSET_GREEK_G0, VBI_CHARSET_GREEK_G2, VBI_SUBSET_NONE,			{ "el", } },

	UNUSED (56),
	UNUSED (57),
	UNUSED (58),
	UNUSED (59),
	UNUSED (60),
	UNUSED (61),
	UNUSED (62),
	UNUSED (63),

	{ 64, VBI_CHARSET_LATIN_G0, VBI_CHARSET_ARABIC_G2, VBI_SUBSET_ENGLISH,			{ "ar", "en", } },
	UNUSED (65),
	UNUSED (66),
	UNUSED (67),
	{ 68, VBI_CHARSET_LATIN_G0, VBI_CHARSET_ARABIC_G2, VBI_SUBSET_FRENCH,			{ "ar", "fr", } },
	UNUSED (69),
	UNUSED (70),
	{ 71, VBI_CHARSET_ARABIC_G0, VBI_CHARSET_ARABIC_G2, VBI_SUBSET_NONE,			{ "ar", } },

	UNUSED (72),
	UNUSED (73),
	UNUSED (74),
	UNUSED (75),
	UNUSED (76),
	UNUSED (77),
	UNUSED (78),
	UNUSED (79),

	UNUSED (80),
	UNUSED (81),
	UNUSED (82),
	UNUSED (83),
	UNUSED (84),
	{ 85, VBI_CHARSET_HEBREW_G0, VBI_CHARSET_ARABIC_G2, VBI_SUBSET_NONE,			{ "he", } },
	UNUSED (86),
	{ 87, VBI_CHARSET_ARABIC_G0, VBI_CHARSET_ARABIC_G2, VBI_SUBSET_NONE,			{ "ar", } },
};

const vbi_character_set *
vbi_character_set_from_code	(vbi_character_set_code code)
{
	if (code >= N_ELEMENTS (character_set_table))
		return NULL;

	return character_set_table + code;
}

/*
 *  Teletext character set
 */

/* These characters mimic block mosaic, smooth mosaic
   or line drawing character patterns. */
static const uint8_t
ascii_art [] = {
/* 2 */	' ', '\'','`','\"','-', '|', '/', '/', '-', '\\','|', '\\','-', '\\','/', '#',
/* 3 */	'.', ':', ':', ':', '|', '|', '/', '/', '/', '>', '/', '>', '/', '+', '\\','#',
/* 2 */	' ', '\'','`','\"','-', '|', '/', '/', '-', '\\','|', '\\','-', '\\','/', '#',
/* 3 */	'.', ':', ':', ':', '|', '|', '/', '/', '/', '>', '/', '>', '/', '+', '\\','#',
/* 6 */	'.', ':', ':', ':', '\\','\\','<', '/', '|', '\\','|', '\\','\\','/', '+', '#',
/* 7 */	'_', ':', ':', '=', '\\','\\','<', '[', '/', '>', '/', ']', '#', '#', '#', '#',
/* 6 */	'.', ':', ':', ':', '\\','\\','<', '/', '|', '\\','|', '\\','\\','/', '+', '#',
/* 7 */	'_', ':', ':', '=', '\\','\\','<', '[', '/', '>', '/', ']', '#', '#', '#', '#',
/* 2 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 3 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 4 */ 0x2B, 0x2B, 0x2B, 0x2B, 0x3C, 0x3E, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x2B, 0x2A, 0x2A, 0x6F,
/* 5 */ 0x7C, 0x2D, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B,
        0x2B, 0x2B, 0x2B, 0x3E, 0x3C, 0x00, 0x00, 0x00,
/* 6 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 7 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

unsigned int
_vbi_teletext_ascii_art		(unsigned int		c)
{
	if (c >= 0xEE00 && c < (0xEE00 + N_ELEMENTS (ascii_art)))
		if (ascii_art[c - 0xEE00])
			return ascii_art[c - 0xEE00];

	return c;
}

/* ETS 300 706 Table 36: Latin National Option Sub-sets
   Latin G0 character code to Unicode mapping per national subset,
   unmodified codes (VBI_SUBSET_NONE) in row zero. */
static const uint16_t
national_subset[14][13] = {
	{ 0x0023, 0x0024, 0x0040, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F, 0x0060, 0x007B, 0x007C, 0x007D, 0x007E },

	/* Replaced by: */
	{ 0x0023, 0x016F, 0x010D, 0x0165, 0x017E, 0x00FD, 0x00ED, 0x0159, 0x00E9, 0x00E1, 0x011B, 0x00FA, 0x0161 },
	{ 0x00A3, 0x0024, 0x0040, 0x2190, 0x00BD, 0x2192, 0x2191, 0x0023, 0x2014, 0x00BC, 0x2016, 0x00BE, 0x00F7 },
	{ 0x0023, 0x00F5, 0x0160, 0x00C4, 0x00D6, 0x017D, 0x00DC, 0x00D5, 0x0161, 0x00E4, 0x00F6, 0x017E, 0x00FC },
	{ 0x00E9, 0x00EF, 0x00E0, 0x00EB, 0x00EA, 0x00F9, 0x00EE, 0x0023, 0x00E8, 0x00E2, 0x00F4, 0x00FB, 0x00E7 },
	{ 0x0023, 0x0024, 0x00A7, 0x00C4, 0x00D6, 0x00DC, 0x005E, 0x005F, 0x00B0, 0x00E4, 0x00F6, 0x00FC, 0x00DF },
	{ 0x00A3, 0x0024, 0x00E9, 0x00B0, 0x00E7, 0x2192, 0x2191, 0x0023, 0x00F9, 0x00E0, 0x00F2, 0x00E8, 0x00EC },
	{ 0x0023, 0x0024, 0x0160, 0x0117, 0x0229, 0x017D, 0x010D, 0x016B, 0x0161, 0x0105, 0x0173, 0x017E, 0x012F },
	{ 0x0023, 0x0144, 0x0105, 0x01B5, 0x015A, 0x0141, 0x0107, 0x00F3, 0x0119, 0x017C, 0x015B, 0x0142, 0x017A },
	{ 0x00E7, 0x0024, 0x00A1, 0x00E1, 0x00E9, 0x00ED, 0x00F3, 0x00FA, 0x00BF, 0x00FC, 0x00F1, 0x00E8, 0x00E0 },
	{ 0x0023, 0x00A4, 0x0162, 0x00C2, 0x015E, 0x01CD, 0x00CD, 0x0131, 0x0163, 0x00E2, 0x015F, 0X01CE, 0x00EE },
	{ 0x0023, 0x00CB, 0x010C, 0x0106, 0x017D, 0x00D0, 0x0160, 0x00EB, 0x010D, 0x0107, 0x017E, 0x00F0, 0x0161 },
	{ 0x0023, 0x00A4, 0x00C9, 0x00C4, 0x00D6, 0x00C5, 0x00DC, 0x005F, 0x00E9, 0x00E4, 0x00F6, 0x00E5, 0x00FC },
	{ 0x20A4, 0x011F, 0x0130, 0x015E, 0x00D6, 0x00C7, 0x00DC, 0x011E, 0x0131, 0x015F, 0x00F6, 0x00E7, 0x00FC }
};

/* ETS 300 706 Table 37: Latin G2 Supplementary Set
   0x49 seems to be dot below; not in Unicode (except combining),
   use 0x002E instead. */
static const uint16_t
latin_g2 [96] = {
	0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x0024, 0x00A5, 0x0023, 0x00A7,
	0x00A4, 0x2018, 0x201C, 0x00AB, 0x2190, 0x2191, 0x2192, 0x2193,
	0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00D7, 0x00B5, 0x00B6, 0x00B7,
	0x00F7, 0x2019, 0x201D, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF,
	0x0020, 0x02CB, 0x02CA, 0x02C6, 0x02DC, 0x02C9, 0x02D8, 0x02D9,
	0x00A8, 0x002E, 0x02DA, 0x02CF, 0x02CD, 0x02DD, 0x02DB, 0x02C7,
	0x2014, 0x00B9, 0x00AE, 0x00A9, 0x2122, 0x266A, 0x20A0, 0x2030,
	0x0251, 0x0020, 0x0020, 0x0020, 0x215B, 0x215C, 0x215D, 0x215E,
	0x2126, 0x00C6, 0x00D0, 0x00AA, 0x0126, 0x0020, 0x0132, 0x013F,
	0x0141, 0x00D8, 0x0152, 0x00BA, 0x00DE, 0x0166, 0x014A, 0x0149,
	0x0138, 0x00E6, 0x0111, 0x00F0, 0x0127, 0x0131, 0x0133, 0x0140,
	0x0142, 0x00F8, 0x0153, 0x00DF, 0x00FE, 0x0167, 0x014B, 0x25A0
};

/* ETS 300 706 Table 38: Cyrillic G0 Primary Set -
   Option 1 - Serbian/Croatian */
static const uint16_t
cyrillic1_g0 [64] = {
	0x0427, 0x0410, 0x0411, 0x0426, 0x0414, 0x0415, 0x0424, 0x0413,
	0x0425, 0x0418, 0x0408, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E,
	0x041F, 0x040C, 0x0420, 0x0421, 0x0422, 0x0423, 0x0412, 0x0403,
	0x0409, 0x040A, 0x0417, 0x040B, 0x0416, 0x0402, 0x0428, 0x040F,
	0x0447, 0x0430, 0x0431, 0x0446, 0x0434, 0x0435, 0x0444, 0x0433,
	0x0445, 0x0438, 0x0458, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E,
	0x043F, 0x045C, 0x0440, 0x0441, 0x0442, 0x0443, 0x0432, 0x0453,
	0x0459, 0x045A, 0x0437, 0x045B, 0x0436, 0x0452, 0x0448, 0x25A0
};

/* ETS 300 706 Table 39: Cyrillic G0 Primary Set -
   Option 2 - Russian/Bulgarian */
static const uint16_t
cyrillic2_g0 [64] = {
	0x042E, 0x0410, 0x0411, 0x0426, 0x0414, 0x0415, 0x0424, 0x0413,
	0x0425, 0x0418, 0x040D, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E,
	0x041F, 0x042F, 0x0420, 0x0421, 0x0422, 0x0423, 0x0416, 0x0412,
	0x042C, 0x042A, 0x0417, 0x0428, 0x042D, 0x0429, 0x0427, 0x042B,
	0x044E, 0x0430, 0x0431, 0x0446, 0x0434, 0x0435, 0x0444, 0x0433,
	0x0445, 0x0438, 0x045D, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E,
	0x043F, 0x044F, 0x0440, 0x0441, 0x0442, 0x0443, 0x0436, 0x0432,
	0x044C, 0x044A, 0x0437, 0x0448, 0x044D, 0x0449, 0x0447, 0x25A0
};

/* ETS 300 706 Table 40: Cyrillic G0 Primary Set - Option 3 - Ukrainian  */
static const uint16_t
cyrillic3_g0 [64] = {
	0x042E, 0x0410, 0x0411, 0x0426, 0x0414, 0x0415, 0x0424, 0x0413,
	0x0425, 0x0418, 0x040D, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E,
	0x041F, 0x042F, 0x0420, 0x0421, 0x0422, 0x0423, 0x0416, 0x0412,
	0x042C, 0x0406, 0x0417, 0x0428, 0x0404, 0x0429, 0x0427, 0x0407,
	0x044E, 0x0430, 0x0431, 0x0446, 0x0434, 0x0435, 0x0444, 0x0433,
	0x0445, 0x0438, 0x045D, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E,
	0x043F, 0x044F, 0x0440, 0x0441, 0x0442, 0x0443, 0x0436, 0x0432,
	0x044C, 0x0456, 0x0437, 0x0448, 0x0454, 0x0449, 0x0447, 0x25A0
};

/* ETS 300 706 Table 41: Cyrillic G2 Supplementary Set */
static const uint16_t
cyrillic_g2 [96] = {
	0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x0020, 0x00A5, 0x0023, 0x00A7,
	0x0020, 0x2018, 0x201C, 0x00AB, 0x2190, 0x2191, 0x2192, 0x2193,
	0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00D7, 0x00B5, 0x00B6, 0x00B7,
	0x00F7, 0x2019, 0x201D, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF,
	0x0020, 0x02CB, 0x02CA, 0x02C6, 0x02DC, 0x02C9, 0x02D8, 0x02D9,
	0x00A8, 0x002E, 0x02DA, 0x02CF, 0x02CD, 0x02DD, 0x02DB, 0x02C7,
	0x2014, 0x00B9, 0x00AE, 0x00A9, 0x2122, 0x266A, 0x20A0, 0x2030,
	0x0251, 0x0141, 0x0142, 0x00DF, 0x215B, 0x215C, 0x215D, 0x215E,
	0x0044, 0x0045, 0x0046, 0x0047, 0x0049, 0x004A, 0x004B, 0x004C,
	0x004E, 0x0051, 0x0052, 0x0053, 0x0055, 0x0056, 0x0057, 0x005A,
	0x0064, 0x0065, 0x0066, 0x0067, 0x0069, 0x006A, 0x006B, 0x006C,
	0x006E, 0x0071, 0x0072, 0x0073, 0x0075, 0x0076, 0x0077, 0x007A
};

/* ETS 300 706 Table 42: Greek G0 Primary Set */
static const uint16_t
greek_g0 [64] = {
	0x0390, 0x0391, 0x0392, 0x0393, 0x0394, 0x0395, 0x0396, 0x0397,
	0x0398, 0x0399, 0x039A, 0x039B, 0x039C, 0x039D, 0x039E, 0x039F,
	0x03A0, 0x03A1, 0x0374, 0x03A3, 0x03A4, 0x03A5, 0x03A6, 0x03A7,
	0x03A8, 0x03A9, 0x03AA, 0x03AB, 0x03AC, 0x03AD, 0x03AE, 0x03AF,
	0x03B0, 0x03B1, 0x03B2, 0x03B3, 0x03B4, 0x03B5, 0x03B6, 0x03B7,
	0x03B8, 0x03B9, 0x03BA, 0x03BB, 0x03BC, 0x03BD, 0x03BE, 0x03BF,
	0x03C0, 0x03C1, 0x03C2, 0x03C3, 0x03C4, 0x03C5, 0x03C6, 0x03C7,
	0x03C8, 0x03C9, 0x03CA, 0x03CB, 0x03CC, 0x03CD, 0x03CE, 0x25A0
};

/* ETS 300 706 Table 43: Greek G2 Supplementary Set */
static const uint16_t
greek_g2 [96] = {
	0x00A0, 0x0061, 0x0062, 0x00A3, 0x0065, 0x0068, 0x0069, 0x00A7,
	0x003A, 0x2018, 0x201C, 0x006B, 0x2190, 0x2191, 0x2192, 0x2193,
	0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x0078, 0x006D, 0x006E, 0x0070,
	0x00F7, 0x2019, 0x201D, 0x0074, 0x00BC, 0x00BD, 0x00BE, 0x0078,
	0x0020, 0x02CB, 0x02CA, 0x02C6, 0x02DC, 0x02C9, 0x02D8, 0x02D9,
	0x00A8, 0x002E, 0x02DA, 0x02CF, 0x02CD, 0x02DD, 0x02DB, 0x02C7,
	0x003F, 0x00B9, 0x00AE, 0x00A9, 0x2122, 0x266A, 0x20A0, 0x2030,
	0x0251, 0x038A, 0x038E, 0x038F, 0x215B, 0x215C, 0x215D, 0x215E,
	0x0043, 0x0044, 0x0046, 0x0047, 0x004A, 0x004C, 0x0051, 0x0052,
	0x0053, 0x0055, 0x0056, 0x0057, 0x0059, 0x005A, 0x0386, 0x0389,
	0x0063, 0x0064, 0x0066, 0x0067, 0x006A, 0x006C, 0x0071, 0x0072,
	0x0073, 0x0075, 0x0076, 0x0077, 0x0079, 0x007A, 0x0388, 0x25A0
};

/* ETS 300 706 Table 44: Arabic G0 Primary Set
   XXX 0X0000 is what? Until these tables are finished use private codes. */
static const uint16_t
arabic_g0 [96] = {
/*
	0x0020, 0x0021, 0x0022, 0x00A3, 0x0024, 0x0025, 0x0000, 0x0000,
	0x0029, 0x0028, 0x002A, 0x002B, 0x060C, 0x002D, 0x002E, 0x002F,
	0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
	0x0038, 0x0039, 0x003A, 0x061B, 0x003E, 0x003D, 0x003C, 0x061F,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0630, 0x0631, 0x0632, 0x0633, 0x0634, 0x0635, 0x0636, 0x0637,
	0x0638, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0023,
	0x0640, 0x0641, 0x0642, 0x0643, 0x0644, 0x0645, 0x0646, 0x0647,
	0x0648, 0x0649, 0x064A, 0x062B, 0x062D, 0x062C, 0x062E, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x25A0
*/
	0x0020, 0x0021, 0x0022, 0x00A3, 0x0024, 0x0025, 0xE606, 0xE607,
	0x0029, 0x0028, 0x002A, 0x002B, 0x060C, 0x002D, 0x002E, 0x002F,
	0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
	0x0038, 0x0039, 0x003A, 0x061B, 0x003E, 0x003D, 0x003C, 0x061F,
	0xE620, 0xE621, 0xE622, 0xE623, 0xE624, 0xE625, 0xE626, 0xE627,
	0xE628, 0xE629, 0xE62A, 0xE62B, 0xE62C, 0xE62D, 0xE62E, 0xE62F,
	0xE630, 0xE631, 0xE632, 0xE633, 0xE634, 0xE635, 0xE636, 0xE637,
	0xE638, 0xE639, 0xE63A, 0xE63B, 0xE63C, 0xE63D, 0xE63E, 0x0023,
	0xE640, 0xE641, 0xE642, 0xE643, 0xE644, 0xE645, 0xE646, 0xE647,
	0xE648, 0xE649, 0xE64A, 0xE64B, 0xE64C, 0xE64D, 0xE64E, 0xE64F,
	0xE650, 0xE651, 0xE652, 0xE653, 0xE654, 0xE655, 0xE656, 0xE657,
	0xE658, 0xE659, 0xE65A, 0xE65B, 0xE65C, 0xE65D, 0xE65E, 0x25A0
};

/* ETS 300 706 Table 45: Arabic G2 Supplementary Set
   XXX 0X0000 is what? Until these tables are finished use private codes. */
static const uint16_t
arabic_g2 [96] = {
/*
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0660, 0x0661, 0x0662, 0x0663, 0x0664, 0x0665, 0x0666, 0x0667,
	0x0668, 0x0669, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
*/
	0xE660, 0xE661, 0xE662, 0xE663, 0xE664, 0xE665, 0xE666, 0xE667,
	0xE668, 0xE669, 0xE66A, 0xE66B, 0xE66C, 0xE66D, 0xE66E, 0xE66F,
	0xE670, 0xE671, 0xE672, 0xE673, 0xE674, 0xE675, 0xE676, 0xE677,
	0xE678, 0xE679, 0xE67A, 0xE67B, 0xE67C, 0xE67D, 0xE67E, 0xE67F,
	0x00E0, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
	0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
	0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
	0x0058, 0x0059, 0x005A, 0x00EB, 0x00EA, 0x00F9, 0x00EE, 0xE75F,
	0x00E9, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
	0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,
	0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
	0x0078, 0x0079, 0x007A, 0x00E2, 0x00F4, 0x00FB, 0x00E7, 0x0020
};

/* ETS 300 706 Table 46: Hebrew G0 Primary Set */
static const uint16_t
hebrew_g0 [37] = {
	0x2190, 0x00BD, 0x2192, 0x2191, 0x0023,
	0x05D0, 0x05D1, 0x05D2, 0x05D3, 0x05D4, 0x05D5, 0x05D6, 0x05D7,
	0x05D8, 0x05D9, 0x05DA, 0x05DB, 0x05DC, 0x05DD, 0x05DE, 0x05DF,
	0x05E0, 0x05E1, 0x05E2, 0x05E3, 0x05E4, 0x05E5, 0x05E6, 0x05E7,
	0x05E8, 0x05E9, 0x05EA, 0x20AA, 0x2016, 0x00BE, 0x00F7, 0x25A0
};

/**
 * @param charset Teletext character set as defined in ETS 300 706 section 15.
 * @param subset National character subset as defined in section 15, only
 *   applicable to character set LATIN_G0, otherwise ignored.
 * @param c Character code in range 0x20 ... 0x7F.
 * 
 * Translates Teletext character code to Unicode.
 * 
 * Exceptions:
 * ETS 300 706 Table 36 Latin National Subset Turkish character
 * 0x23 Turkish currency symbol is not representable in Unicode,
 * translated to private code U+E800. Was unable to identify all
 * Arabic glyphs in Table 44 and 45 Arabic G0 and G2, these are
 * mapped to private code U+E620 ... U+E67F and U+E720 ... U+E77F
 * respectively. Table 47 G1 Block Mosaic is not representable
 * in Unicode, translated to private code U+EE00 ... U+EE7F.
 * (contiguous form has bit 5 set, separate form cleared).
 * Table 48 G3 Smooth Mosaics and Line Drawing Set is not
 * representable in Unicode, translated to private code U+EF20
 * ... U+EF7F.
 *
 * Note that some Teletext character sets contain complementary
 * Latin characters. For example the Greek capital letters Alpha
 * and Beta are reused as Latin capital letter A and B, while a
 * separate code exists for Latin capital letter C. This function
 * is unable to distinguish between uses, so it will always translate
 * Greek A and B to Alpha and Beta, C to Latin C.
 * 
 * Private codes U+F000 ... U+F7FF are reserved for DRCS.
 * 
 * @return
 * Unicode value.
 */
unsigned int
vbi_teletext_unicode		(vbi_charset		charset,
				 vbi_subset		subset,
				 unsigned int		c)
{
	assert (c >= 0x20 && c <= 0x7F);

	switch (charset) {
	case VBI_CHARSET_LATIN_G0:
		/* Shortcut. */
		if (0xF8000019UL & (1 << (c & 31))) {
			if (subset > 0) {
				unsigned int i;

				assert (subset < 14);

				for (i = 0; i < 13; ++i)
					if (c == national_subset[0][i])
					    return national_subset[subset][i];
			}

			if (c == 0x24)
				return 0x00A4;
			else if (c == 0x7C)
				return 0x00A6;
			else if (c == 0x7F)
				return 0x25A0;
		}

		return c;

	case VBI_CHARSET_LATIN_G2:
		return latin_g2[c - 0x20];

	case VBI_CHARSET_CYRILLIC1_G0:
		if (c < 0x40)
			return c;
		else
			return cyrillic1_g0[c - 0x40];

	case VBI_CHARSET_CYRILLIC2_G0:
		if (c == 0x26)
			return 0x044B;
		else if (c < 0x40)
			return c;
		else
			return cyrillic2_g0[c - 0x40];

	case VBI_CHARSET_CYRILLIC3_G0:
		if (c == 0x26)
			return 0x00EF;
		else if (c < 0x40)
			return c;
		else
			return cyrillic3_g0[c - 0x40];

	case VBI_CHARSET_CYRILLIC_G2:
		return cyrillic_g2[c - 0x20];

	case VBI_CHARSET_GREEK_G0:
		if (c == 0x3C)
			return 0x00AB;
		else if (c == 0x3E)
			return 0x00BB;
		else if (c < 0x40)
			return c;
		else
			return greek_g0[c - 0x40];

	case VBI_CHARSET_GREEK_G2:
		return greek_g2[c - 0x20];

	case VBI_CHARSET_ARABIC_G0:
		return arabic_g0[c - 0x20];

	case VBI_CHARSET_ARABIC_G2:
		return arabic_g2[c - 0x20];

	case VBI_CHARSET_HEBREW_G0:
		if (c < 0x5B)
			return c;
		else
			return hebrew_g0[c - 0x5B];

	case VBI_CHARSET_BLOCK_MOSAIC_G1:
		/* 0x20 ... 0x3F -> 0xEE00 ... 0xEE1F separated */
		/*                  0xEE20 ... 0xEE3F contiguous */
		/* 0x60 ... 0x7F -> 0xEE40 ... 0xEE5F separated */
		/*                  0xEE60 ... 0xEE7F contiguous */
		assert (c < 0x40 || c >= 0x60);
		return 0xEE00u + c;

	case VBI_CHARSET_SMOOTH_MOSAIC_G3:
		return 0xEF00u + c;

	default:
		fprintf (stderr, "%s: unknown char set %d\n",
			 __PRETTY_FUNCTION__, charset);
		exit (EXIT_FAILURE);
	}
}

/* Unicode 0x00C0 ... 0x017F to
   Teletext combining diacritical mark
   (((Latin G2 0x40 ... 0x4F) - 0x40) << 12) + Latin G0 */
static const uint16_t
composed [12 * 16] = {
	0x1041, 0x2041, 0x3041, 0x4041, 0x8041, 0xA041, 0x0000, 0xB043, 0x1045, 0x2045, 0x3045, 0x8045, 0x1049, 0x2049, 0x3049, 0x8049,
	0x0000, 0x404E, 0x104F, 0x204F, 0x304F, 0x404F, 0x804F, 0x0000, 0x0000, 0x1055, 0x2055, 0x3055, 0x8055, 0x2059, 0x0000, 0x0000,
	0x1061, 0x2061, 0x3061, 0x4061, 0x8061, 0xA061, 0x0000, 0xB063, 0x1065, 0x2065, 0x3065, 0x8065, 0x1069, 0x2069, 0x3069, 0x8069,
	0x0000, 0x406E, 0x106F, 0x206F, 0x306F, 0x406F, 0x806F, 0x0000, 0x00F9, 0x1075, 0x2075, 0x3075, 0x8075, 0x2079, 0x0000, 0x8079,
	0x5041, 0x5061, 0x6041, 0x6061, 0xE041, 0xE061, 0x2043, 0x2063, 0x3043, 0x3063, 0x7043, 0x7063, 0xF043, 0xF063, 0xF044, 0xF064,
	0x0000, 0x0000, 0x5045, 0x5065, 0x6045, 0x6065, 0x7045, 0x7065, 0xE045, 0xE065, 0xF045, 0xF065, 0x3047, 0x3067, 0x6047, 0x6067,
	0x7047, 0x7067, 0x0000, 0x0000, 0x3048, 0x3068, 0x0000, 0x0000, 0x4049, 0x4069, 0x5049, 0x5069, 0x6049, 0x6069, 0xE049, 0xE069,
	0x7049, 0x0000, 0x0000, 0x0000, 0x304A, 0x306A, 0x0000, 0x0000, 0x0000, 0x204C, 0x2049, 0x0000, 0x0000, 0xF04C, 0xF06C, 0x0000,
	0x0000, 0x0000, 0x0000, 0x204E, 0x206E, 0x0000, 0x0000, 0xF04E, 0xF06E, 0x0000, 0x0000, 0x0000, 0x504F, 0x506F, 0x604F, 0x606F,
	0xD04F, 0xD06F, 0x0000, 0x0000, 0x2052, 0x2072, 0x0000, 0x0000, 0xF052, 0xF072, 0x2053, 0x2073, 0x3053, 0x3073, 0xB053, 0xB073,
	0xF053, 0xF073, 0xB054, 0xB074, 0xF053, 0xF073, 0x0000, 0x0000, 0x4055, 0x4075, 0x5055, 0x5075, 0x6055, 0x6075, 0xA055, 0xA075,
	0xD055, 0xD075, 0xE055, 0xE075, 0x3057, 0x3077, 0x3059, 0x3079, 0x8059, 0x205A, 0x207A, 0x705A, 0x707A, 0xF05A, 0xF07A, 0x0000
};

/**
 * @internal
 * @param a Diacritical mark 0 ... 15.
 * @param c Character code in range 0x20 ... 0x7F.
 * 
 * Translate Teletext Latin G1 character 0x20 ... 0x7F combined
 * with accent code from Latin G2 0x40 ... 0x4F to Unicode. Not all
 * combinations are representable in Unicode.
 *
 * @return
 * Unicode value or 0.
 */
unsigned int
vbi_teletext_composed_unicode	(unsigned int		a,
				 unsigned int		c)
{
	unsigned int i;

	assert (a <= 15);
	assert (c >= 0x20 && c <= 0x7F);

	if (0 == a)
		return vbi_teletext_unicode (VBI_CHARSET_LATIN_G0,
					     VBI_SUBSET_NONE, c);

	c += a << 12;

	for (i = 0; i < N_ELEMENTS (composed); ++i)
		if (composed[i] == c)
			return 0x00C0 + i;

	return 0;
}

/*
 *  Closed Caption character set
 */

/* Closed Caption character set
   Defined in EIA 608. Standard not available, this information comes
   from Video Demystified, Table 8.31. Closed Captioning Basic Character
   Set) */
static const uint16_t
caption [96] = {
	0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
	0x0028, 0x0029, 0x00E1, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
	0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
	0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
	0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
	0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
	0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
	0x0058, 0x0059, 0x005A, 0x005B, 0x00E9, 0x005D, 0x00ED, 0x00F3,
	0x00FA, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
	0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,
	0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
	0x0078, 0x0079, 0x007A, 0x00E7, 0x00F7, 0x00D1, 0x00F1, 0x25A0
};

/* Closed Caption special characters. */
static const uint16_t
caption_special [] = {
	0x00AE, 0x00B0, 0x00BD, 0x00BF, 0x2122, 0x00A2, 0x00A3, 0x266A,
	0x00E0, 0x0020, 0x00E8, 0x00E2, 0x00EA, 0x00EE, 0x00F4, 0x00FB
};

/**
 * @param c Character code in range 0x00 ... 0x0F and 0x20 ... 0x7F.
 * 
 * Translates Closed Caption character code to Unicode. Codes
 * in range 0x00 ... 0x0F are Special Characters (Closed
 * Caption commands 0x22/0x3x, 0x32/0x3x).
 *
 * @return
 * Unicode value.
 */
unsigned int
vbi_caption_unicode		(unsigned int		c)
{
	assert (c <= 0x0F || (c >= 0x20 && c <= 0x7F));

	if (c < 0x10)
		return caption_special[c];
	else
		return caption[c - 0x20];
}
