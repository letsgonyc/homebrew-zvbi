/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 2.
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

/* $Id: tables.c,v 1.1.1.1 2001-08-07 22:10:09 garetxe Exp $ */

#include "mpeg.h"

/*
 *  ISO 13818-2 Table 6.4
 */
const double
frame_rate_value[16] =
{
	0,
	24000.0 / 1001, 24.0,
	25.0, 30000.0 / 1001, 30.0,
	50.0, 60000.0 / 1001, 60.0
};

/*
 *  ISO 13818-2 6.3.11
 */
const unsigned char
default_intra_quant_matrix[8][8] =
{
	{  8, 16, 19, 22, 26, 27, 29, 34 },
	{ 16, 16, 22, 24, 27, 29, 34, 37 },
	{ 19, 22, 26, 27, 29, 34, 34, 38 },
	{ 22, 22, 26, 27, 29, 34, 37, 40 },
	{ 22, 26, 27, 29, 32, 35, 40, 48 },
	{ 26, 27, 29, 32, 35, 40, 48, 58 },
	{ 26, 27, 29, 34, 38, 46, 56, 69 },
	{ 27, 29, 35, 38, 46, 56, 69, 83 }
};

const unsigned char
default_inter_quant_matrix[8][8] =
{
        { 16, 16, 16, 16, 16, 16, 16, 16 },
        { 16, 16, 16, 16, 16, 16, 16, 16 },
        { 16, 16, 16, 16, 16, 16, 16, 16 },
        { 16, 16, 16, 16, 16, 16, 16, 16 },
        { 16, 16, 16, 16, 16, 16, 16, 16 },
        { 16, 16, 16, 16, 16, 16, 16, 16 },
        { 16, 16, 16, 16, 16, 16, 16, 16 },
        { 16, 16, 16, 16, 16, 16, 16, 16 },
};

/*
 *  ISO 13818-2 Figure 7-2, 7-3
 */
const unsigned char
scan[2][8][8] =
{
	{
		{  0,  1,  5,  6, 14, 15, 27, 28 }, 
		{  2,  4,  7, 13, 16, 26, 29, 42 },
		{  3,  8, 12, 17, 25, 30, 41, 43 },
		{  9, 11, 18, 24, 31, 40, 44, 53 },
		{ 10, 19, 23, 32, 39, 45, 52, 54 },
		{ 20, 22, 33, 38, 46, 51, 55, 60 },
		{ 21, 34, 37, 47, 50, 56, 59, 61 },
		{ 35, 36, 48, 49, 57, 58, 62, 63 }
	}, {
		{  0,  4,  6, 20, 22, 36, 38, 52 },
		{  1,  5,  7, 21, 23, 37, 39, 53 },
		{  2,  8, 19, 24, 34, 40, 50, 54 },
		{  3,  9, 18, 25, 35, 41, 51, 55 },
		{ 10, 17, 26, 30, 42, 46, 56, 60 },
		{ 11, 16, 27, 31, 43, 47, 57, 61 },
		{ 12, 15, 28, 32, 44, 48, 58, 62 },
		{ 13, 14, 29, 33, 45, 49, 59, 63 }
	}
};

/*
 *  ISO 13818-2 Table 7.6
 */
const unsigned char
quantiser_scale[2][32] =
{
	{ 0, 2, 4, 6, 8, 10, 12, 14,
	  16, 18, 20, 22, 24, 26, 28, 30,
	  32, 34, 36, 38, 40, 42, 44, 46,
	  48, 50, 52, 54, 56, 58, 60, 62 },

	{ 0, 1, 2, 3, 4, 5, 6, 7,
	  8, 10, 12, 14, 16, 18, 20, 22,
	  24, 28, 32, 36, 40, 44, 48, 52,
	  56, 64, 72, 80, 88, 96, 104, 112 }
};

/*
 *  Variable Length Codes
 */

#define VLC(bits) (((0 ## bits) << 5) | (sizeof(# bits) - 1)) // MAX. 19 BITS!

/*
 *  ISO 13818 Table B-1
 *  Variable length codes for macroblock_address_increment
 */
const unsigned long long
macroblock_address_increment_vlc[33] =
{
	VLC(1),
	VLC(011),
	VLC(010),
	VLC(0011),
	VLC(0010),
	VLC(00011),
	VLC(00010),
	VLC(0000111),
	VLC(0000110),
	VLC(00001011),
	VLC(00001010),
	VLC(00001001),
	VLC(00001000),
	VLC(00000111),
	VLC(00000110),
	VLC(0000010111),
	VLC(0000010110),
	VLC(0000010101),
	VLC(0000010100),
	VLC(0000010011),
	VLC(0000010010),
	VLC(00000100011),
	VLC(00000100010),
	VLC(00000100001),
	VLC(00000100000),
	VLC(00000011111),
	VLC(00000011110),
	VLC(00000011101),
	VLC(00000011100),
	VLC(00000011011),
	VLC(00000011010),
	VLC(00000011001),
	VLC(00000011000)
	// VLC(00000001000) macroblock_escape code
};

/*
 *  ISO 13818-2 Table B-9
 *  Variable length codes for coded_block_pattern
 */
const unsigned long long
coded_block_pattern_vlc[64] =
{
	VLC(000000001), // This entry shall not be used with 4:2:0 chrominance structure
	VLC(01011),
	VLC(01001),		
	VLC(001101),		
	VLC(1101),		
	VLC(0010111),	
	VLC(0010011),	
	VLC(00011111),	
	VLC(1100),		
	VLC(0010110),	
	VLC(0010010),	
	VLC(00011110),	
	VLC(10011),		
	VLC(00011011),
	VLC(00010111),
	VLC(00010011),
	VLC(1011),		
	VLC(0010101),	
	VLC(0010001),	
	VLC(00011101),	
	VLC(10001),		
	VLC(00011001),
	VLC(00010101),
	VLC(00010001),
	VLC(001111),	
	VLC(00001111),
	VLC(00001101),
	VLC(000000011),
	VLC(01111),		
	VLC(00001011),
	VLC(00000111),
	VLC(000000111),
	VLC(1010),		
	VLC(0010100),	
	VLC(0010000),	
	VLC(00011100),
	VLC(001110),	
	VLC(00001110),
	VLC(00001100),
	VLC(000000010),
	VLC(10000),		
	VLC(00011000),
	VLC(00010100),
	VLC(00010000),
	VLC(01110),		
	VLC(00001010),
	VLC(00000110),
	VLC(000000110),
	VLC(10010),		
	VLC(00011010),
	VLC(00010110),
	VLC(00010010),
	VLC(01101),		
	VLC(00001001),
	VLC(00000101),
	VLC(000000101),
	VLC(01100),		
	VLC(00001000),
	VLC(00000100),
	VLC(000000100),
	VLC(111),		
	VLC(01010),		
	VLC(01000),		
	VLC(001100)	
};

/*
 *  ISO 13818 Table B-10
 *  Variable length codes for motion_code (not including sign bit)
 */
const unsigned long long
motion_code_vlc[17] =
{
	VLC(1),			// 0
	VLC(01),		// 1
	VLC(001),
	VLC(0001),
	VLC(000011),
	VLC(0000101),
	VLC(0000100),
	VLC(0000011),
	VLC(000001011),
	VLC(000001010),
	VLC(000001001),
	VLC(0000010001),
	VLC(0000010000),
	VLC(0000001111),
	VLC(0000001110),
	VLC(0000001101),	// 15
	VLC(0000001100)		// 16
};

/*
 *  ISO 13818-2 Table B-12
 *  Variable length codes for dct_dc_size_luminance
 */
const unsigned long long
dct_dc_size_luma_vlc[12] =
{
	VLC(100),
	VLC(00),
	VLC(01),
	VLC(101),
	VLC(110),
	VLC(1110),
	VLC(11110),
	VLC(111110),
	VLC(1111110),
	VLC(11111110),
	VLC(111111110),
	VLC(111111111)
};

/*
 *  ISO 13818-2 Table B-13
 *  Variable length codes for dct_dc_size_chrominance
 */
const unsigned long long
dct_dc_size_chroma_vlc[12] =
{
	VLC(00),
	VLC(01),
	VLC(10),
	VLC(110),
	VLC(1110),
	VLC(11110),
	VLC(111110),
	VLC(1111110),
	VLC(11111110),
	VLC(111111110),
	VLC(1111111110),
	VLC(1111111111)
};

struct dct_coeff {
	unsigned long long	code;
	char			run, level;
};

/*
 *  ISO 13818-2 Table B-14
 *  DCT coefficients table zero (not including sign bit)
 */
static const struct dct_coeff
dct_coeff_zero_vlc[] =
{
	// VLC(10) End of Block
	// { VLC(1), 0, 1 } This code shall be used
	// for the first (DC) coefficient of a non-intra block
	{ VLC(11), 0, 1 },
	{ VLC(011), 1, 1 },
	{ VLC(0100), 0, 2 },
	{ VLC(0101), 2, 1 },
	{ VLC(00101), 0, 3 },
	{ VLC(00111), 3, 1 },
	{ VLC(00110), 4, 1 },
	{ VLC(000110), 1, 2 },
	{ VLC(000111), 5, 1 },
	{ VLC(000101), 6, 1 },
	{ VLC(000100), 7, 1 },
	{ VLC(0000110), 0, 4 },
	{ VLC(0000100), 2, 2 },
	{ VLC(0000111), 8, 1 },
	{ VLC(0000101), 9, 1 },
	// VLC(000001) Escape code
	{ VLC(00100110), 0, 5 },
	{ VLC(00100001), 0, 6 },
	{ VLC(00100101), 1, 3 },
	{ VLC(00100100), 3, 2 },
	{ VLC(00100111), 10, 1 },
	{ VLC(00100011), 11, 1 },
	{ VLC(00100010), 12, 1 },
	{ VLC(00100000), 13, 1 },
	{ VLC(0000001010), 0, 7 },
	{ VLC(0000001100), 1, 4 },
	{ VLC(0000001011), 2, 3 },
	{ VLC(0000001111), 4, 2 },
	{ VLC(0000001001), 5, 2 },
	{ VLC(0000001110), 14, 1 },
	{ VLC(0000001101), 15, 1 },
	{ VLC(0000001000), 16, 1 },
	{ VLC(000000011101), 0, 8 },
	{ VLC(000000011000), 0, 9 },
	{ VLC(000000010011), 0, 10 },
	{ VLC(000000010000), 0, 11 },
	{ VLC(000000011011), 1, 5 },
	{ VLC(000000010100), 2, 4 },
	{ VLC(000000011100), 3, 3 },
	{ VLC(000000010010), 4, 3 },
	{ VLC(000000011110), 6, 2 },
	{ VLC(000000010101), 7, 2 },
	{ VLC(000000010001), 8, 2 },
	{ VLC(000000011111), 17, 1 },
	{ VLC(000000011010), 18, 1 },
	{ VLC(000000011001), 19, 1 },
	{ VLC(000000010111), 20, 1 },
	{ VLC(000000010110), 21, 1 },
	{ VLC(0000000011010), 0, 12 },
	{ VLC(0000000011001), 0, 13 },
	{ VLC(0000000011000), 0, 14 },
	{ VLC(0000000010111), 0, 15 },
	{ VLC(0000000010110), 1, 6 },
	{ VLC(0000000010101), 1, 7 },
	{ VLC(0000000010100), 2, 5 },
	{ VLC(0000000010011), 3, 4 },
	{ VLC(0000000010010), 5, 3 },
	{ VLC(0000000010001), 9, 2 },
	{ VLC(0000000010000), 10, 2 },
	{ VLC(0000000011111), 22, 1 },
	{ VLC(0000000011110), 23, 1 },
	{ VLC(0000000011101), 24, 1 },
	{ VLC(0000000011100), 25, 1 },
	{ VLC(0000000011011), 26, 1 },
	{ VLC(00000000011111), 0, 16 },
	{ VLC(00000000011110), 0, 17 },
	{ VLC(00000000011101), 0, 18 },
	{ VLC(00000000011100), 0, 19 },
	{ VLC(00000000011011), 0, 20 },
	{ VLC(00000000011010), 0, 21 },
	{ VLC(00000000011001), 0, 22 },
	{ VLC(00000000011000), 0, 23 },
	{ VLC(00000000010111), 0, 24 },
	{ VLC(00000000010110), 0, 25 },
	{ VLC(00000000010101), 0, 26 },
	{ VLC(00000000010100), 0, 27 },
	{ VLC(00000000010011), 0, 28 },
	{ VLC(00000000010010), 0, 29 },
	{ VLC(00000000010001), 0, 30 },
	{ VLC(00000000010000), 0, 31 },
	{ VLC(000000000011000), 0, 32 },
	{ VLC(000000000010111), 0, 33 },
	{ VLC(000000000010110), 0, 34 },
	{ VLC(000000000010101), 0, 35 },
	{ VLC(000000000010100), 0, 36 },
	{ VLC(000000000010011), 0, 37 },
	{ VLC(000000000010010), 0, 38 },
	{ VLC(000000000010001), 0, 39 },
	{ VLC(000000000010000), 0, 40 },
	{ VLC(000000000011111), 1, 8 },
	{ VLC(000000000011110), 1, 9 },
	{ VLC(000000000011101), 1, 10 },
	{ VLC(000000000011100), 1, 11 },
	{ VLC(000000000011011), 1, 12 },
	{ VLC(000000000011010), 1, 13 },
	{ VLC(000000000011001), 1, 14 },
	{ VLC(0000000000010011), 1, 15 },
	{ VLC(0000000000010010), 1, 16 },
	{ VLC(0000000000010001), 1, 17 },
	{ VLC(0000000000010000), 1, 18 },
	{ VLC(0000000000010100), 6, 3 },
	{ VLC(0000000000011010), 11, 2 },
	{ VLC(0000000000011001), 12, 2 },
	{ VLC(0000000000011000), 13, 2 },
	{ VLC(0000000000010111), 14, 2 },
	{ VLC(0000000000010110), 15, 2 },
	{ VLC(0000000000010101), 16, 2 },
	{ VLC(0000000000011111), 27, 1 },
	{ VLC(0000000000011110), 28, 1 },
	{ VLC(0000000000011101), 29, 1 },
	{ VLC(0000000000011100), 30, 1 },
	{ VLC(0000000000011011), 31, 1 },
	{ VLC(0), -1, -1 }
};

/*
 *  ISO 13818-2 Table B-15
 *  DCT coefficients table one (not including sign bit)
 */
static const struct dct_coeff
dct_coeff_one_vlc[] =
{
	// VLC(0110) End of Block
	{ VLC(10), 0, 1 },
	{ VLC(010), 1, 1 },
	{ VLC(110), 0, 2 },
	{ VLC(00101), 2, 1 },
	{ VLC(0111), 0, 3 },
	{ VLC(00111), 3, 1 },
	{ VLC(000110), 4, 1 },
	{ VLC(00110), 1, 2 },
	{ VLC(000111), 5, 1 },
	{ VLC(0000110), 6, 1 },
	{ VLC(0000100), 7, 1 },
	{ VLC(11100), 0, 4 },
	{ VLC(0000111), 2, 2 },
	{ VLC(0000101), 8, 1 },
	{ VLC(1111000), 9, 1 },
	// VLC(000001) Escape code
	{ VLC(11101), 0, 5 },
	{ VLC(000101), 0, 6 },
	{ VLC(1111001), 1, 3 },
	{ VLC(00100110), 3, 2 },
	{ VLC(1111010), 10, 1 },
	{ VLC(00100001), 11, 1 },
	{ VLC(00100101), 12, 1 },
	{ VLC(00100100), 13, 1 },
	{ VLC(000100), 0, 7 },
	{ VLC(00100111), 1, 4 },
	{ VLC(11111100), 2, 3 },
	{ VLC(11111101), 4, 2 },
	{ VLC(000000100), 5, 2 },
	{ VLC(000000101), 14, 1 },
	{ VLC(000000111), 15, 1 },
	{ VLC(0000001101), 16, 1 },
	{ VLC(1111011), 0, 8 },
	{ VLC(1111100), 0, 9 },
	{ VLC(00100011), 0, 10 },
	{ VLC(00100010), 0, 11 },
	{ VLC(00100000), 1, 5 },
	{ VLC(0000001100), 2, 4 },
	{ VLC(000000011100), 3, 3 },
	{ VLC(000000010010), 4, 3 },
	{ VLC(000000011110), 6, 2 },
	{ VLC(000000010101), 7, 2 },
	{ VLC(000000010001), 8, 2 },
	{ VLC(000000011111), 17, 1 },
	{ VLC(000000011010), 18, 1 },
	{ VLC(000000011001), 19, 1 },
	{ VLC(000000010111), 20, 1 },
	{ VLC(000000010110), 21, 1 },
	{ VLC(11111010), 0, 12 },
	{ VLC(11111011), 0, 13 },
	{ VLC(11111110), 0, 14 },
	{ VLC(11111111), 0, 15 },
	{ VLC(0000000010110), 1, 6 },
	{ VLC(0000000010101), 1, 7 },
	{ VLC(0000000010100), 2, 5 },
	{ VLC(0000000010011), 3, 4 },
	{ VLC(0000000010010), 5, 3 },
	{ VLC(0000000010001), 9, 2 },
	{ VLC(0000000010000), 10, 2 },
	{ VLC(0000000011111), 22, 1 },
	{ VLC(0000000011110), 23, 1 },
	{ VLC(0000000011101), 24, 1 },
	{ VLC(0000000011100), 25, 1 },
	{ VLC(0000000011011), 26, 1 },
	{ VLC(00000000011111), 0, 16 },
	{ VLC(00000000011110), 0, 17 },
	{ VLC(00000000011101), 0, 18 },
	{ VLC(00000000011100), 0, 19 },
	{ VLC(00000000011011), 0, 20 },
	{ VLC(00000000011010), 0, 21 },
	{ VLC(00000000011001), 0, 22 },
	{ VLC(00000000011000), 0, 23 },
	{ VLC(00000000010111), 0, 24 },
	{ VLC(00000000010110), 0, 25 },
	{ VLC(00000000010101), 0, 26 },
	{ VLC(00000000010100), 0, 27 },
	{ VLC(00000000010011), 0, 28 },
	{ VLC(00000000010010), 0, 29 },
	{ VLC(00000000010001), 0, 30 },
	{ VLC(00000000010000), 0, 31 },
	{ VLC(000000000011000), 0, 32 },
	{ VLC(000000000010111), 0, 33 },
	{ VLC(000000000010110), 0, 34 },
	{ VLC(000000000010101), 0, 35 },
	{ VLC(000000000010100), 0, 36 },
	{ VLC(000000000010011), 0, 37 },
	{ VLC(000000000010010), 0, 38 },
	{ VLC(000000000010001), 0, 39 },
	{ VLC(000000000010000), 0, 40 },
	{ VLC(000000000011111), 1, 8 },
	{ VLC(000000000011110), 1, 9 },
	{ VLC(000000000011101), 1, 10 },
	{ VLC(000000000011100), 1, 11 },
	{ VLC(000000000011011), 1, 12 },
	{ VLC(000000000011010), 1, 13 },
	{ VLC(000000000011001), 1, 14 },
	{ VLC(0000000000010011), 1, 15 },
	{ VLC(0000000000010010), 1, 16 },
	{ VLC(0000000000010001), 1, 17 },
	{ VLC(0000000000010000), 1, 18 },
	{ VLC(0000000000010100), 6, 3 },
	{ VLC(0000000000011010), 11, 2 },
	{ VLC(0000000000011001), 12, 2 },
	{ VLC(0000000000011000), 13, 2 },
	{ VLC(0000000000010111), 14, 2 },
	{ VLC(0000000000010110), 15, 2 },
	{ VLC(0000000000010101), 16, 2 },
	{ VLC(0000000000011111), 27, 1 },
	{ VLC(0000000000011110), 28, 1 },
	{ VLC(0000000000011101), 29, 1 },
	{ VLC(0000000000011100), 30, 1 },
	{ VLC(0000000000011011), 31, 1 },
	{ VLC(0), -1, -1 }
};

/*
 *  Translate VLC(), returns bit length
 */
int
vlc(unsigned long long vlc_octet, unsigned int *code)
{
	int i;

	*code = 0;

	for (i = 0; i < 19; i++)
		if (vlc_octet & (1ULL << (i * 3 + 5)))
			*code |= 1 << i;

	return vlc_octet & 0x1F;
}

/*
 *  Find dct_vlc, not including sign bit
 *  (append 0 for positive level, 1 for negative level)
 */
int
dct_coeff_vlc(int table, int run, int level, unsigned int *vlcp)
{
	const struct dct_coeff *dcp;

	for (dcp = table ? dct_coeff_one_vlc : dct_coeff_zero_vlc; dcp->run >= 0; dcp++)
		if (dcp->run == run && dcp->level == level)
			return vlc(dcp->code, vlcp);

	return -1; // No vlc for this run/length combination
}
