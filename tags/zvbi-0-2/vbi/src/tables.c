/*
 *  libzvbi - Tables
 *
 *  PDC and VPS CNI codes rev. 5, based on
 *    TR 101 231 EBU (2001-08): www.ebu.ch
 *  Programme type tables PDC/EPG, XDS
 *
 *  Copyright (C) 1999, 2000, 2001 Michael H. Schimek
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

/* $Id: tables.c,v 1.2 2002-05-23 03:59:46 mschimek Exp $ */

#include <stdlib.h>

#include "tables.h"

/*
 *  ISO 3166-1 country codes
 */
enum {
	AT, BE, HR, CZ, DK, FI, FR, DE, GR,
	HU, IS, IE, IT, LU, NL, NO, PL, PT,
	SM, SK, SI, ES, SE, CH, TR, GB
};

const char *
vbi_country_names_en[] = {
	"Austria",
	"Belgium",
	"Croatia",
	"Czech Republic",
	"Denmark",
	"Finland",
	"France",
	"Germany",
	"Greece",
	"Hungary",
	"Iceland",
	"Ireland",
	"Italy",
	"Luxembourg",
	"Netherlands",
	"Norway",
	"Poland",
	"Portugal",
	"San Marino",
	"Slovakia",
	"Slovenia",
	"Spain",
	"Sweden",
	"Switzerland",
	"Turkey",
	"United Kingdom"
};

/*
    CNI sources:

    Packet 8/30 f1	Byte 13			Byte 14
    Bit (tx order)	0 1 2 3	4 5 6 7		0 1 2 3 4 5 6 7
    CNI			--------------- 15:8	--------------- 7:0

    Packet 8/30 f2	Byte 15		Byte 16		Byte 21		Byte 22		Byte 23
    Bit (tx order)	0 1 2 3		0 1		2 3		0 1 2 3		0 1 2 3
    VPS			Byte 5		Byte 11		Byte 13			Byte 14
    Bit (tx order)	4 5 6 7		0 1		6 7		0 1 2 3		4 5 6 7
    Country		------- 15:12 / 7:4		------------------- 11:8 / 3:0
    Network				--- 7:6			  	5:0 -------------------

    Packet X/26		Address			Mode		Data
    Bit (tx order)	0 1 2 3 4 5 6 7 8 9	A B C D E F	G H I J K L M N
    Data Word A		P P - P ----- P 1 1 0:5 (0x3n)
    Mode				        0 0 0 1 0 P 0:5 ("Country & Programme Source")
    Data Word B							------------- P 0:6
 */

/*
 *  TR 101 231 Table A.1: Register of Country and Network Identification (CNI) codes for
 *                        Teletext based systems
 *
 *  TR 101 231 Table B.1: VPS CNI Codes used in Germany, Switzerland and Austria
 *
 *  (Unified to create a unique station id where A.1 and B.1 overlap.)
 *
 *	int16_t		id;		* unique id 1++, currently the largest is 380
 *	int16_t		country;
 *	char *		name;		* (ISO 8859-1)
 *	uint16_t	cni1;		* Packet 8/30 format 1
 *	uint16_t	cni2;		* Packet 8/30 format 2
 *	uint16_t	cni3;		* Packet X/26
 *	uint16_t	cni4;		* VPS
 */
const struct vbi_cni_entry
vbi_cni_table[] = {
	{ 1,	BE, "VRT TV1",				0x3201, 0x1601, 0x3603, 0x0000 },
	{ 2,	BE, "Ka2",				0x3206, 0x1606, 0x3606, 0x0000 },
	{ 3,	BE, "RTBF 1",				0x3203, 0x0000, 0x0000, 0x0000 },
	{ 4,	BE, "RTBF 2",				0x3204, 0x0000, 0x0000, 0x0000 },
	{ 5,    BE, "CANVAS",				0x3202, 0x1602, 0x3602, 0x0000 },
	{ 6,	BE, "VT4",				0x0404, 0x1604, 0x3604, 0x0000 },
	{ 7,	BE, "VTM",				0x3205, 0x1605, 0x3605, 0x0000 },

	{ 8,	HR, "HRT",				0x0385, 0x0000, 0x0000, 0x0000 },

	{ 9,	CZ, "CT 1",				0x4201, 0x32C1, 0x3C21, 0x0000 },
	{ 10,	CZ, "CT 2",				0x4202, 0x32C2, 0x3C22, 0x0000 },
	{ 11,	CZ, "CT1 Regional",			0x4231, 0x32F1, 0x3C25, 0x0000 },
	{ 12,	CZ, "CT1 Brno",				0x4211, 0x32D1, 0x3B01, 0x0000 },
	{ 13,	CZ, "CT1 Ostravia",			0x4221, 0x32E1, 0x3B02, 0x0000 },
	{ 14,	CZ, "CT2 Regional",			0x4232, 0x32F2, 0x3B03, 0x0000 },
	{ 15,	CZ, "CT2 Brno",				0x4212, 0x32D2, 0x3B04, 0x0000 },
	{ 16,	CZ, "CT2 Ostravia",			0x4222, 0x32E2, 0x3B05, 0x0000 },
	{ 17,	CZ, "NOVA TV",				0x4203, 0x32C3, 0x3C23, 0x0000 },
	
	{ 18,	DK, "DR1",				0x7392, 0x2901, 0x3901, 0x0000 },
	{ 19,	DK, "DR2",				0x49CF, 0x2903, 0x3903, 0x0000 },
	{ 20,	DK, "TV2",				0x4502, 0x2902, 0x3902, 0x0000 },
	{ 21,	DK, "TV2 Zulu",				0x4503, 0x2904, 0x3904, 0x0000 },

	{ 22,	FI, "OWL3",				0x358F, 0x260F, 0x3614, 0x0000 },
	{ 23,	FI, "YLE1",				0x3581, 0x2601, 0x3601, 0x0000 },
	{ 24,	FI, "YLE2",				0x3582, 0x2602, 0x3607, 0x0000 },

	{ 25,	FR, "AB1",				0x33C1, 0x2FC1, 0x3F41, 0x0000 },
	{ 26,	FR, "Aqui TV",				0x3320, 0x2F20, 0x3F20, 0x0000 },
	{ 27,	FR, "Arte / La Cinqui�me",		0x330A, 0x2F0A, 0x3F0A, 0x0000 },
	{ 28,	FR, "Canal J",				0x33C2, 0x2FC2, 0x3F42, 0x0000 },
	{ 29,	FR, "Canal Jimmy",			0x33C3, 0x2FC3, 0x3F43, 0x0000 },
	{ 30,	FR, "Canal+",				0x33F4, 0x2F04, 0x3F04, 0x0000 },
	{ 31,	FR, "Euronews",				0xFE01, 0x2FE1, 0x3F61, 0x0000 },
	{ 32,	FR, "Eurosport",			0xF101, 0x2FE2, 0x3F62, 0x0000 },
	{ 33,	FR, "France 2",				0x33F2, 0x2F02, 0x3F02, 0x0000 },
	{ 34,	FR, "France 3",				0x33F3, 0x2F03, 0x3F03, 0x0000 },
	{ 35,	FR, "La Cha�ne M�t�o",			0x33C5, 0x2FC5, 0x3F45, 0x0000 },
	{ 36,	FR, "LCI",				0x33C4, 0x2FC4, 0x3F44, 0x0000 },
	{ 37,	FR, "M6",				0x33F6, 0x2F06, 0x3F06, 0x0000 },
	{ 38,	FR, "MCM",				0x33C6, 0x2FC6, 0x3F46, 0x0000 },
	{ 39,	FR, "Paris Premi�re",			0x33C8, 0x2FC8, 0x3F48, 0x0000 },
	{ 40,	FR, "Plan�te",				0x33C9, 0x2FC9, 0x3F49, 0x0000 },
	{ 41,	FR, "RFO1",				0x3311, 0x2F11, 0x3F11, 0x0000 },
	{ 42,	FR, "RFO2",				0x3312, 0x2F12, 0x3F12, 0x0000 },
	{ 43,	FR, "S�rie Club",			0x33CA, 0x2FCA, 0x3F4A, 0x0000 },
	{ 44,	FR, "T�l�toon",				0x33CB, 0x2FCB, 0x3F4B, 0x0000 },
	{ 45,	FR, "T�va",				0x33CC, 0x2FCC, 0x3F4C, 0x0000 },
	{ 46,	FR, "TF1",				0x33F1, 0x2F01, 0x3F01, 0x0000 },
	{ 47,	FR, "TLM",				0x3321, 0x2F21, 0x3F21, 0x0000 },
	{ 48,	FR, "TLT",				0x3322, 0x2F22, 0x3F22, 0x0000 },
	{ 49,	FR, "TMC Monte-Carlo",			0x33C7, 0x2FC7, 0x3F47, 0x0000 },
	{ 50,	FR, "TV5",				0xF500, 0x2FE5, 0x3F65, 0x0000 },

	/* Table B.1 */

	{ 51,	DE, "FESTIVAL",				0x4941, 0x0000, 0x0000, 0x0D41 },
	{ 52,	DE, "MUXX",				0x4942, 0x0000, 0x0000, 0x0D42 },
	{ 53,	DE, "EXTRA",				0x4943, 0x0000, 0x0000, 0x0D43 },

	{ 54,	DE, "ONYX-TV",				0x0000, 0x0000, 0x0000, 0x0D7C },
	{ 55,	DE, "QVC-Teleshopping",			0x5C49, 0x0000, 0x0000, 0x0D7D },
	{ 56,	DE, "Nickelodeon",			0x0000, 0x0000, 0x0000, 0x0D7E },
	{ 57,	DE, "Home Shopping Europe",	        0x0000, 0x0000, 0x0000, 0x0D7F },

	{ 58,	DE, "ORB-1 Regional",			0x0000, 0x0000, 0x0000, 0x0D81 },
	{ 59,	DE, "ORB-3 Landesweit",			0x4982, 0x0000, 0x0000, 0x0D82 },
	/* not used 0x0D83 */
	/* not used 0x0D84 */
	{ 60,	DE, "Arte",				0x490A, 0x0000, 0x3D05, 0x0D85 },
	/* not in TR 101 231: 0x3D05 */
	/* not used 0x0D86 */
	{ 61,	DE, "1A-Fernsehen",			0x0000, 0x0000, 0x0000, 0x0D87 },
	{ 62,	DE, "VIVA",				0x0000, 0x0000, 0x0000, 0x0D88 },
	{ 63,	DE, "VIVA 2",				0x0000, 0x0000, 0x0000, 0x0D89 },
	{ 64,	DE, "Super RTL",			0x0000, 0x0000, 0x0000, 0x0D8A },
	{ 65,	DE, "RTL Club",				0x0000, 0x0000, 0x0000, 0x0D8B },
	{ 66,	DE, "n-tv",				0x0000, 0x0000, 0x0000, 0x0D8C },
	{ 67,	DE, "Deutsches Sportfernsehen",		0x0000, 0x0000, 0x0000, 0x0D8D },
	{ 68,	DE, "VOX",		                0x490C, 0x0000, 0x0000, 0x0D8E },
	{ 69,	DE, "RTL 2",				0x0D8F, 0x0000, 0x0000, 0x0D8F },
	/* not in TR 101 231: 0x0D8F */
	{ 70,	DE, "RTL 2 Regional",			0x0000, 0x0000, 0x0000, 0x0D90 },
	{ 71,	DE, "Eurosport",			0x0000, 0x0000, 0x0000, 0x0D91 },
	{ 72,	DE, "Kabel 1",				0x0000, 0x0000, 0x0000, 0x0D92 },
	/* not used 0x0D93 */
	{ 73,	DE, "PRO 7",				0x0000, 0x0000, 0x0000, 0x0D94 },
	/* not in TR 101 231: 0x0D14, Pro 7 Austria? */
	{ 74,	DE, "PRO 7",				0x0000, 0x0000, 0x0000, 0x0D14 },
	{ 75,	DE, "SAT 1 Brandenburg",		0x0000, 0x0000, 0x0000, 0x0D95 },
	{ 76,	DE, "SAT 1 Th�ringen",			0x0000, 0x0000, 0x0000, 0x0D96 },
	{ 77,	DE, "SAT 1 Sachsen",			0x0000, 0x0000, 0x0000, 0x0D97 },
	{ 78,	DE, "SAT 1 Mecklenb.-Vorpommern",	0x0000, 0x0000, 0x0000, 0x0D98 },
	{ 79,	DE, "SAT 1 Sachsen-Anhalt",		0x0000, 0x0000, 0x0000, 0x0D99 },
	{ 80,	DE, "RTL Regional",			0x0000, 0x0000, 0x0000, 0x0D9A },
	{ 81,	DE, "RTL Schleswig-Holstein",		0x0000, 0x0000, 0x0000, 0x0D9B },
	{ 82,	DE, "RTL Hamburg",			0x0000, 0x0000, 0x0000, 0x0D9C },
	{ 83,	DE, "RTL Berlin",			0x0000, 0x0000, 0x0000, 0x0D9D },
	{ 84,	DE, "RTL Niedersachsen",		0x0000, 0x0000, 0x0000, 0x0D9E },
	{ 85,	DE, "RTL Bremen",			0x0000, 0x0000, 0x0000, 0x0D9F },
	{ 86,	DE, "RTL Nordrhein-Westfalen",		0x0000, 0x0000, 0x0000, 0x0DA0 },
	{ 87,	DE, "RTL Hessen",			0x0000, 0x0000, 0x0000, 0x0DA1 },
	{ 88,	DE, "RTL Rheinland-Pfalz",		0x0000, 0x0000, 0x0000, 0x0DA2 },
	{ 89,	DE, "RTL Baden-W�rttemberg",		0x0000, 0x0000, 0x0000, 0x0DA3 },
	{ 90,	DE, "RTL Bayern",			0x0000, 0x0000, 0x0000, 0x0DA4 },
	{ 91,	DE, "RTL Saarland",			0x0000, 0x0000, 0x0000, 0x0DA5 },
	{ 92,	DE, "RTL Sachsen-Anhalt",		0x0000, 0x0000, 0x0000, 0x0DA6 },
	{ 93,	DE, "RTL Mecklenburg-Vorpommern",	0x0000, 0x0000, 0x0000, 0x0DA7 },
	{ 94,	DE, "RTL Sachsen",			0x0000, 0x0000, 0x0000, 0x0DA8 },
	{ 95,	DE, "RTL Th�ringen",			0x0000, 0x0000, 0x0000, 0x0DA9 },
	{ 96,	DE, "RTL Brandenburg",			0x0000, 0x0000, 0x0000, 0x0DAA },
	{ 97,	DE, "RTL",		           	0x0000, 0x0000, 0x0000, 0x0DAB },
	{ 98,	DE, "Premiere",				0x0000, 0x0000, 0x0000, 0x0DAC },
	{ 99,	DE, "SAT 1 Regional",			0x0000, 0x0000, 0x0000, 0x0DAD },
	{ 100,	DE, "SAT 1 Schleswig-Holstein",		0x0000, 0x0000, 0x0000, 0x0DAE },
	{ 101,	DE, "SAT 1 Hamburg",			0x0000, 0x0000, 0x0000, 0x0DAF },
	{ 102,	DE, "SAT 1 Berlin",			0x0000, 0x0000, 0x0000, 0x0DB0 },
	{ 103,	DE, "SAT 1 Niedersachsen",		0x0000, 0x0000, 0x0000, 0x0DB1 },
	{ 104,	DE, "SAT 1 Bremen",			0x0000, 0x0000, 0x0000, 0x0DB2 },
	{ 105,	DE, "SAT 1 Nordrhein-Westfalen",	0x0000, 0x0000, 0x0000, 0x0DB3 },
	{ 106,	DE, "SAT 1 Hessen",			0x0000, 0x0000, 0x0000, 0x0DB4 },
	{ 107,	DE, "SAT 1 Rheinland-Pfalz",		0x0000, 0x0000, 0x0000, 0x0DB5 },
	{ 108,	DE, "SAT 1 Baden-W�rttemberg",		0x0000, 0x0000, 0x0000, 0x0DB6 },
	{ 109,	DE, "SAT 1 Bayern",			0x0000, 0x0000, 0x0000, 0x0DB7 },
	{ 110,	DE, "SAT 1 Saarland",			0x0000, 0x0000, 0x0000, 0x0DB8 },
	{ 111,	DE, "SAT 1",				0x0000, 0x0000, 0x0000, 0x0DB9 },
	{ 112,	DE, "NEUN LIVE",	                0x0000, 0x0000, 0x0000, 0x0DBA },
	{ 113,	DE, "Deutsche Welle TV Berlin",		0x0000, 0x0000, 0x0000, 0x0DBB },
	{ 114,	DE, "Berlin Offener Kanal",		0x0000, 0x0000, 0x0000, 0x0DBD },
	{ 115,	DE, "Berlin-Mix-Channel 2",		0x0000, 0x0000, 0x0000, 0x0DBE },
	{ 116,	DE, "Berlin-Mix-Channel 1",		0x0000, 0x0000, 0x0000, 0x0DBF },

	{ 117,	DE, "ARD",				0x4901, 0x0000, 0x3D41, 0x0DC1 },
	/* not in TR 101 231: 0x3D41 */
	{ 118,	DE, "ZDF",				0x4902, 0x0000, 0x3D42, 0x0DC2 },
	/* not in TR 101 231: 0x3D42 */
	{ 119,	DE, "ARD/ZDF Vormittagsprogramm",	0x0000, 0x0000, 0x0000, 0x0DC3 },
/*
 *  "NOTE: As this code is used for a time in two networks a distinction
 *   for automatic tuning systems is given in data line 16: [VPS]
 *   bit 3 of byte 5 = 1 for the ARD network / = 0 for the ZDF network."
 */
	{ 120,	DE, "ARD-TV-Sternpunkt",		0x0000, 0x0000, 0x0000, 0x0DC4 },
	{ 121,	DE, "ARD-TV-Sternpunkt-Fehler",		0x0000, 0x0000, 0x0000, 0x0DC5 },
	/* not used 0x0DC6 */
	{ 122,	DE, "3sat",				0x49C7, 0x0000, 0x0000, 0x0DC7 },
	{ 123,	DE, "Phoenix",				0x4908, 0x0000, 0x0000, 0x0DC8 },
	{ 124,	DE, "ARD/ZDF Kinderkanal",		0x49C9, 0x0000, 0x0000, 0x0DC9 },
	{ 125,	DE, "BR-1 Regional",			0x0000, 0x0000, 0x0000, 0x0DCA },
	{ 126,	DE, "BR-3 Landesweit",			0x49CB, 0x0000, 0x3D4B, 0x0DCB },
	/* not in TR 101 231: 0x3D4B */
	{ 377,	DE, "BR-Alpha",				0x4944, 0x0000, 0x0000, 0x0000 },
	{ 127,	DE, "BR-3 S�d",				0x0000, 0x0000, 0x0000, 0x0DCC },
	{ 128,	DE, "BR-3 Nord",			0x0000, 0x0000, 0x0000, 0x0DCD },
	{ 129,	DE, "HR-1 Regional",			0x0000, 0x0000, 0x0000, 0x0DCE },
	{ 130,	DE, "Hessen 3 Landesweit",		0x49FF, 0x0000, 0x0000, 0x0DCF },
	{ 131,	DE, "NDR-1 Dreil�nderweit",		0x0000, 0x0000, 0x0000, 0x0DD0 },
	{ 132,	DE, "NDR-1 Hamburg",			0x0000, 0x0000, 0x0000, 0x0DD1 },
	{ 133,	DE, "NDR-1 Niedersachsen",		0x0000, 0x0000, 0x0000, 0x0DD2 },
	{ 134,	DE, "NDR-1 Schleswig-Holstein",		0x0000, 0x0000, 0x0000, 0x0DD3 },
	{ 135,	DE, "Nord-3 (NDR/SFB/RB)",		0x0000, 0x0000, 0x0000, 0x0DD4 },
	{ 136,	DE, "NDR-3 Dreil�nderweit",		0x49D4, 0x0000, 0x0000, 0x0DD5 },
	{ 137,	DE, "NDR-3 Hamburg",			0x0000, 0x0000, 0x0000, 0x0DD6 },
	{ 138,	DE, "NDR-3 Niedersachsen",		0x0000, 0x0000, 0x0000, 0x0DD7 },
	{ 139,	DE, "NDR-3 Schleswig-Holstein",		0x0000, 0x0000, 0x0000, 0x0DD8 },
	{ 140,	DE, "RB-1 Regional",			0x0000, 0x0000, 0x0000, 0x0DD9 },
	{ 141,	DE, "RB-3",				0x49D9, 0x0000, 0x0000, 0x0DDA },
	{ 142,	DE, "SFB-1 Regional",			0x0000, 0x0000, 0x0000, 0x0DDB },
	{ 143,	DE, "SFB-3",				0x49DC, 0x0000, 0x0000, 0x0DDC },
	{ 144,	DE, "SDR/SWF Baden-W�rttemberg",	0x0000, 0x0000, 0x0000, 0x0DDD },
	{ 145,	DE, "SWF-1 Rheinland-Pfalz",		0x0000, 0x0000, 0x0000, 0x0DDE },
	{ 146,	DE, "SR-1 Regional",			0x49DF, 0x0000, 0x0000, 0x0DDF },
	{ 147,	DE, "S�dwest 3 (SDR/SR/SWF)",		0x0000, 0x0000, 0x0000, 0x0DE0 },
	{ 148,	DE, "SW 3 Baden-W�rttemberg",		0x49E1, 0x0000, 0x0000, 0x0DE1 },
	{ 149,	DE, "SW 3 Saarland",			0x0000, 0x0000, 0x0000, 0x0DE2 },
	{ 150,	DE, "SW 3 Baden-W�rttemb. S�d",		0x0000, 0x0000, 0x0000, 0x0DE3 },
	{ 151,	DE, "SW 3 Rheinland-Pfalz",		0x49E4, 0x0000, 0x0000, 0x0DE4 },
	{ 152,	DE, "WDR-1 Regionalprogramm",		0x0000, 0x0000, 0x0000, 0x0DE5 },
	{ 153,	DE, "WDR-3 Landesweit",			0x49E6, 0x0000, 0x0000, 0x0DE6 },
	{ 154,	DE, "WDR-3 Bielefeld",			0x0000, 0x0000, 0x0000, 0x0DE7 },
	{ 155,	DE, "WDR-3 Dortmund",			0x0000, 0x0000, 0x0000, 0x0DE8 },
	{ 156,	DE, "WDR-3 D�sseldorf",			0x0000, 0x0000, 0x0000, 0x0DE9 },
	{ 157,	DE, "WDR-3 K�ln",			0x0000, 0x0000, 0x0000, 0x0DEA },
	{ 158,	DE, "WDR-3 M�nster",			0x0000, 0x0000, 0x0000, 0x0DEB },
	{ 159,	DE, "SDR-1 Regional",			0x0000, 0x0000, 0x0000, 0x0DEC },
	{ 160,	DE, "SW 3 Baden-W�rttemberg Nord",	0x0000, 0x0000, 0x0000, 0x0DED },
	{ 161,	DE, "SW 3 Mannheim",			0x0000, 0x0000, 0x0000, 0x0DEE },
	{ 162,	DE, "SDR/SWF BW und Rhld-Pfalz",	0x0000, 0x0000, 0x0000, 0x0DEF },
	{ 163,	DE, "SWF-1 / Regionalprogramm",		0x0000, 0x0000, 0x0000, 0x0DF0 },
	{ 164,	DE, "NDR-1 Mecklenb.-Vorpommern",	0x0000, 0x0000, 0x0000, 0x0DF1 },
	{ 165,	DE, "NDR-3 Mecklenb.-Vorpommern",	0x0000, 0x0000, 0x0000, 0x0DF2 },
	{ 166,	DE, "MDR-1 Sachsen",			0x0000, 0x0000, 0x0000, 0x0DF3 },
	{ 167,	DE, "MDR-3 Sachsen",			0x0000, 0x0000, 0x0000, 0x0DF4 },
	{ 168,	DE, "MDR Dresden",			0x0000, 0x0000, 0x0000, 0x0DF5 },
	{ 169,	DE, "MDR-1 Sachsen-Anhalt",		0x0000, 0x0000, 0x0000, 0x0DF6 },
	{ 170,	DE, "WDR Dortmund",			0x0000, 0x0000, 0x0000, 0x0DF7 },
	{ 171,	DE, "MDR-3 Sachsen-Anhalt",		0x0000, 0x0000, 0x0000, 0x0DF8 },
	{ 172,	DE, "MDR Magdeburg",			0x0000, 0x0000, 0x0000, 0x0DF9 },
	{ 173,	DE, "MDR-1 Th�ringen",			0x0000, 0x0000, 0x0000, 0x0DFA },
	{ 174,	DE, "MDR-3 Th�ringen",			0x0000, 0x0000, 0x0000, 0x0DFB },
	{ 175,	DE, "MDR Erfurt",			0x0000, 0x0000, 0x0000, 0x0DFC },
	{ 176,	DE, "MDR-1 Regional",			0x0000, 0x0000, 0x0000, 0x0DFD },
	{ 177,	DE, "MDR-3 Landesweit",			0x49FE, 0x0000, 0x0000, 0x0DFE },

	{ 178,	CH, "TeleZ�ri",				0x0000, 0x0000, 0x0000, 0x0481 },
	{ 179,	CH, "Teleclub Abo-Fernsehen",		0x0000, 0x0000, 0x0000, 0x0482 },
	{ 180,	CH, "Z�rich 1",				0x0000, 0x0000, 0x0000, 0x0483 },
	{ 181,	CH, "TeleBern",				0x0000, 0x0000, 0x0000, 0x0484 },
	{ 182,	CH, "Tele M1",				0x0000, 0x0000, 0x0000, 0x0485 },
	{ 183,	CH, "Star TV",				0x0000, 0x0000, 0x0000, 0x0486 },
	{ 184,	CH, "Pro 7",				0x0000, 0x0000, 0x0000, 0x0487 },
	{ 185,	CH, "TopTV",				0x0000, 0x0000, 0x0000, 0x0488 },

	{ 186,	CH, "SRG Schweizer Fernsehen SF 1",	0x4101, 0x24C1, 0x3441, 0x04C1 },
	{ 187,	CH, "SSR T�l�vis. Suisse TSR 1",	0x4102, 0x24C2, 0x3442, 0x04C2 },
	{ 188,	CH, "SSR Televis. svizzera TSI 1",	0x4103, 0x24C3, 0x3443, 0x04C3 },
	/* not used 0x04C4 */
	/* not used 0x04C5 */
	/* not used 0x04C6 */
	{ 189,	CH, "SRG Schweizer Fernsehen SF 2",	0x4107, 0x24C7, 0x3447, 0x04C7 },
	{ 190,	CH, "SSR T�l�vis. Suisse TSR 2",	0x4108, 0x24C8, 0x3448, 0x04C8 },
	{ 191,	CH, "SSR Televis. svizzera TSI 2",	0x4109, 0x24C9, 0x3449, 0x04C9 },
	{ 192,	CH, "SRG SSR Sat Access",		0x410A, 0x24CA, 0x344A, 0x04CA },

	{ 193,	AT, "ORF 1",				0x4301, 0x0000, 0x0000, 0x0AC1 },
	{ 194,	AT, "ORF 2",				0x4302, 0x0000, 0x0000, 0x0AC2 },
	{ 195,	AT, "ORF 3",				0x0000, 0x0000, 0x0000, 0x0AC3 },
	{ 196,	AT, "ORF Burgenland",			0x0000, 0x0000, 0x0000, 0x0ACB },
	{ 197,	AT, "ORF K�rnten",			0x0000, 0x0000, 0x0000, 0x0ACC },
	{ 198,	AT, "ORF Nieder�sterreich",		0x0000, 0x0000, 0x0000, 0x0ACD },
	{ 199,	AT, "ORF Ober�sterreich",		0x0000, 0x0000, 0x0000, 0x0ACE },
	{ 200,	AT, "ORF Salzburg",			0x0000, 0x0000, 0x0000, 0x0ACF },
	{ 201,	AT, "ORF Steiermark",			0x0000, 0x0000, 0x0000, 0x0AD0 },
	{ 202,	AT, "ORF Tirol",			0x0000, 0x0000, 0x0000, 0x0AD1 },
	{ 203,	AT, "ORF Vorarlberg",			0x0000, 0x0000, 0x0000, 0x0AD2 },
	{ 204,	AT, "ORF Wien",				0x0000, 0x0000, 0x0000, 0x0AD3 },
	{ 205,	AT, "ATV",				0x0000, 0x0000, 0x0000, 0x0ADE },
	/* not in TR 101 231: 0x0ADE */

	/* Table A.1 continued */

	{ 206,	GR, "ET-1",				0x3001, 0x2101, 0x3101, 0x0000 },
	{ 207,	GR, "NET",				0x3002, 0x2102, 0x3102, 0x0000 },
	{ 208,	GR, "ET-3",				0x3003, 0x2103, 0x3103, 0x0000 },

	{ 209,	HU, "Duna Televizio",			0x3636, 0x0000, 0x0000, 0x0000 },
	{ 210,	HU, "MTV1",				0x3601, 0x0000, 0x0000, 0x0000 },
	{ 211,	HU, "MTV1 Budapest",			0x3611, 0x0000, 0x0000, 0x0000 },
	{ 212,	HU, "MTV1 Debrecen",			0x3651, 0x0000, 0x0000, 0x0000 },
	{ 213,	HU, "MTV1 Miskolc",			0x3661, 0x0000, 0x0000, 0x0000 },
	{ 214,	HU, "MTV1 P�cs",			0x3621, 0x0000, 0x0000, 0x0000 },
	{ 215,	HU, "MTV1 Szeged",			0x3631, 0x0000, 0x0000, 0x0000 },
	{ 216,	HU, "MTV1 Szombathely",			0x3641, 0x0000, 0x0000, 0x0000 },
	{ 217,	HU, "MTV2",				0x3602, 0x0000, 0x0000, 0x0000 },
	{ 218,	HU, "tv2",				0x3622, 0x0000, 0x0000, 0x0000 },

	{ 219,	IS, "Rikisutvarpid-Sjonvarp",		0x3541, 0x0000, 0x0000, 0x0000 },

	{ 220,	IE, "Network 2",			0x3532, 0x4202, 0x3202, 0x0000 },
	{ 221,	IE, "RTE1",				0x3531, 0x4201, 0x3201, 0x0000 },
	{ 222,	IE, "Teilifis na Gaeilge",		0x3533, 0x4203, 0x3203, 0x0000 },
	{ 223,	IE, "TV3",				0x3333, 0x0000, 0x0000, 0x0000 },

	{ 224,	IT, "Arte",				0x390A, 0x0000, 0x0000, 0x0000 },
	{ 225,	IT, "Canale 5",				0xFA05, 0x0000, 0x0000, 0x0000 },
	{ 226,	IT, "Italia 1",				0xFA06, 0x0000, 0x0000, 0x0000 },
	{ 227,	IT, "RAI 1",				0x3901, 0x0000, 0x0000, 0x0000 },
	{ 228,	IT, "RAI 2",				0x3902, 0x0000, 0x0000, 0x0000 },
	{ 229,	IT, "RAI 3",				0x3903, 0x0000, 0x0000, 0x0000 },
	{ 230,	IT, "Rete 4",				0xFA04, 0x0000, 0x0000, 0x0000 },
	{ 231,	IT, "Rete A",				0x3904, 0x0000, 0x0000, 0x0000 },
	{ 232,	IT, "RTV38",				0x3938, 0x0000, 0x0000, 0x0000 },
	{ 233,	IT, "Tele+1",				0x3997, 0x0000, 0x0000, 0x0000 },
	{ 234,	IT, "Tele+2",				0x3998, 0x0000, 0x0000, 0x0000 },
	{ 235,	IT, "Tele+3",				0x3999, 0x0000, 0x0000, 0x0000 },
	{ 236,	IT, "TMC",				0xFA08, 0x0000, 0x0000, 0x0000 },
	{ 237,	IT, "TRS TV",				0x3910, 0x0000, 0x0000, 0x0000 },

	{ 238,	LU, "RTL T�l� Letzebuerg",		0x4000, 0x0000, 0x0000, 0x0000 },

	{ 239,	NL, "Nederland 1",			0x3101, 0x4801, 0x3801, 0x0000 },
	{ 240,	NL, "Nederland 2",			0x3102, 0x4802, 0x3802, 0x0000 },
	{ 241,	NL, "Nederland 3",			0x3103, 0x4803, 0x3803, 0x0000 },
	{ 242,	NL, "RTL 4",				0x3104, 0x4804, 0x3804, 0x0000 },
	{ 243,	NL, "RTL 5",				0x3105, 0x4805, 0x3805, 0x0000 },
	{ 244,	NL, "Veronica",				0x3106, 0x4806, 0x3806, 0x0000 },
	{ 379,  NL, "The BOX",				0x3120, 0x4820, 0x3820, 0x0000 },
	{ 245,	NL, "NRK1",				0x4701, 0x0000, 0x0000, 0x0000 },
	{ 246,	NL, "NRK2",				0x4703, 0x0000, 0x0000, 0x0000 },
	{ 247,	NL, "TV 2",				0x4702, 0x0000, 0x0000, 0x0000 },
	{ 248,	NL, "TV Norge",				0x4704, 0x0000, 0x0000, 0x0000 },

	{ 249,	PL, "TV Polonia",			0x4810, 0x0000, 0x0000, 0x0000 },
	{ 250,	PL, "TVP1",				0x4801, 0x0000, 0x0000, 0x0000 },
	{ 251,	PL, "TVP2",				0x4802, 0x0000, 0x0000, 0x0000 },

	{ 252,	PT, "RTP1",				0x3510, 0x0000, 0x0000, 0x0000 },
	{ 253,	PT, "RTP2",				0x3511, 0x0000, 0x0000, 0x0000 },
    	{ 254,	PT, "RTPAF",				0x3512, 0x0000, 0x0000, 0x0000 },
	{ 255,	PT, "RTPAZ",				0x3514, 0x0000, 0x0000, 0x0000 },
	{ 256,	PT, "RTPI",				0x3513, 0x0000, 0x0000, 0x0000 },
	{ 257,	PT, "RTPM",				0x3515, 0x0000, 0x0000, 0x0000 },

	{ 258,	SM, "RTV",				0x3781, 0x0000, 0x0000, 0x0000 },

	{ 259,	SK, "STV1",				0x42A1, 0x35A1, 0x3521, 0x0000 },
	{ 260,	SK, "STV2",				0x42A2, 0x35A2, 0x3522, 0x0000 },
	{ 261,	SK, "STV1 Kosice",			0x42A3, 0x35A3, 0x3523, 0x0000 },
	{ 262,	SK, "STV2 Kosice",			0x42A4, 0x35A4, 0x3524, 0x0000 },
	{ 263,	SK, "STV1 B. Bystrica",			0x42A5, 0x35A5, 0x3525, 0x0000 },
	{ 264,	SK, "STV2 B. Bystrica",			0x42A6, 0x35A6, 0x3526, 0x0000 },

	{ 265,	SI, "SLO1",				0xAAE1, 0x0000, 0x0000, 0x0000 },
	{ 266,	SI, "SLO2",				0xAAE2, 0x0000, 0x0000, 0x0000 },
	{ 267,	SI, "KC",				0xAAE3, 0x0000, 0x0000, 0x0000 },
	{ 268,	SI, "TLM",				0xAAE4, 0x0000, 0x0000, 0x0000 },
	{ 269,	SI, "SLO3",				0xAAF1, 0x0000, 0x0000, 0x0000 },

        { 270,	ES, "Arte",				0x340A, 0x0000, 0x0000, 0x0000 },
	{ 271,	ES, "C33",				0xCA33, 0x0000, 0x0000, 0x0000 },
	{ 272,	ES, "ETB 1",				0xBA01, 0x0000, 0x0000, 0x0000 },
	{ 273,	ES, "ETB 2",				0x3402, 0x0000, 0x0000, 0x0000 },
	{ 274,	ES, "TV3",				0xCA03, 0x0000, 0x0000, 0x0000 },
	{ 275,	ES, "TVE1",				0x3E00, 0x0000, 0x0000, 0x0000 },
	{ 276,	ES, "TVE2",				0xE100, 0x0000, 0x0000, 0x0000 },
	{ 277,	ES, "Canal+",				0xA55A, 0x0000, 0x0000, 0x0000 },
	/* not in TR 101 231: 0xA55A (valid?) */

	{ 278,	SE, "SVT 1",				0x4601, 0x4E01, 0x3E01, 0x0000 },
	{ 279,	SE, "SVT 2",				0x4602, 0x4E02, 0x3E02, 0x0000 },
	{ 280,	SE, "SVT Test Transmissions",		0x4600, 0x4E00, 0x3E00, 0x0000 },
	{ 281,	SE, "TV 4",				0x4640, 0x4E40, 0x3E40, 0x0000 },

	{ 289,	TR, "ATV",				0x900A, 0x0000, 0x0000, 0x0000 },
	{ 290,	TR, "AVRASYA",				0x9006, 0x4306, 0x3306, 0x0000 },
	{ 291,	TR, "BRAVO TV",				0x900E, 0x0000, 0x0000, 0x0000 },
	{ 292,	TR, "Cine 5",				0x9008, 0x0000, 0x0000, 0x0000 },
	{ 293,	TR, "EKO TV",				0x900D, 0x0000, 0x0000, 0x0000 },
	{ 294,	TR, "EURO D",				0x900C, 0x0000, 0x0000, 0x0000 },
	{ 295,	TR, "FUN TV",				0x9010, 0x0000, 0x0000, 0x0000 },
	{ 296,	TR, "GALAKSI TV",			0x900F, 0x0000, 0x0000, 0x0000 },
	{ 297,	TR, "KANAL D",				0x900B, 0x0000, 0x0000, 0x0000 },
	{ 298,	TR, "Show TV",				0x9007, 0x0000, 0x0000, 0x0000 },
	{ 299,	TR, "Super Sport",			0x9009, 0x0000, 0x0000, 0x0000 },
	{ 300,	TR, "TEMPO TV",				0x9011, 0x0000, 0x0000, 0x0000 },
	{ 301,	TR, "TGRT",				0x9014, 0x0000, 0x0000, 0x0000 },
	{ 302,	TR, "TRT-1",				0x9001, 0x4301, 0x3301, 0x0000 },
	{ 303,	TR, "TRT-2",				0x9002, 0x4302, 0x3302, 0x0000 },
	{ 304,	TR, "TRT-3",				0x9003, 0x4303, 0x3303, 0x0000 },
	{ 305,	TR, "TRT-4",				0x9004, 0x4304, 0x3304, 0x0000 },
	{ 306,	TR, "TRT-INT",				0x9005, 0x4305, 0x3305, 0x0000 },
	/* ?? TRT-INT transmits 0x9001 */

	{ 307,	GB, "ANGLIA TV",			0xFB9C, 0x2C1C, 0x3C1C, 0x0000 },
	{ 308,	GB, "BBC News 24",			0x4469, 0x2C69, 0x3C69, 0x0000 },
	{ 309,	GB, "BBC Prime",			0x4468, 0x2C68, 0x3C68, 0x0000 },
	{ 310,	GB, "BBC World",			0x4457, 0x2C57, 0x3C57, 0x0000 },
	{ 311,	GB, "BBC1",				0x447F, 0x2C7F, 0x3C7F, 0x0000 },
	{ 312,	GB, "BBC1 NI",				0x4441, 0x2C41, 0x3C41, 0x0000 },
	{ 313,	GB, "BBC1 Scotland",			0x447B, 0x2C7B, 0x3C7B, 0x0000 },
	{ 314,	GB, "BBC1 Wales",			0x447D, 0x2C7D, 0x3C7D, 0x0000 },
	{ 315,	GB, "BBC2",				0x4440, 0x2C40, 0x3C40, 0x0000 },
	{ 316,	GB, "BBC2 NI",				0x447E, 0x2C7E, 0x3C7E, 0x0000 },
	{ 317,	GB, "BBC2 Scotland",			0x4444, 0x2C44, 0x3C44, 0x0000 },
	{ 318,	GB, "BBC2 Wales",			0x4442, 0x2C42, 0x3C42, 0x0000 },
	{ 319,	GB, "BORDER TV",			0xB7F7, 0x2C27, 0x3C27, 0x0000 },
	{ 320,	GB, "BRAVO",				0x4405, 0x5BEF, 0x3B6F, 0x0000 },
	{ 321,	GB, "CARLTON SELECT",			0x82E1, 0x2C05, 0x3C05, 0x0000 },
	{ 322,	GB, "CARLTON TV",			0x82DD, 0x2C1D, 0x3C1D, 0x0000 },
	{ 323,	GB, "CENTRAL TV",			0x2F27, 0x2C37, 0x3C37, 0x0000 },
	{ 324,	GB, "CHANNEL 4",			0xFCD1, 0x2C11, 0x3C11, 0x0000 },
	{ 325,	GB, "CHANNEL 5 (1)",			0x9602, 0x2C02, 0x3C02, 0x0000 },
	{ 326,	GB, "CHANNEL 5 (2)",			0x1609, 0x2C09, 0x3C09, 0x0000 },
	{ 327,	GB, "CHANNEL 5 (3)",			0x28EB, 0x2C2B, 0x3C2B, 0x0000 },
	{ 328,	GB, "CHANNEL 5 (4)",			0xC47B, 0x2C3B, 0x3C3B, 0x0000 },
	{ 329,	GB, "CHANNEL TV",			0xFCE4, 0x2C24, 0x3C24, 0x0000 },
	{ 330,	GB, "CHILDREN'S CHANNEL",		0x4404, 0x5BF0, 0x3B70, 0x0000 },
	{ 331,	GB, "CNN International",		0x01F2, 0x5BF1, 0x3B71, 0x0000 },
	{ 332,	GB, "DISCOVERY",			0x4407, 0x5BF2, 0x3B72, 0x0000 },
	{ 333,	GB, "DISNEY CHANNEL UK",		0x44D1, 0x5BCC, 0x3B4C, 0x0000 },
	{ 334,	GB, "FAMILY CHANNEL",			0x4408, 0x5BF3, 0x3B73, 0x0000 },
	{ 335,	GB, "GMTV",				0xADDC, 0x5BD2, 0x3B52, 0x0000 },
	{ 336,	GB, "GRAMPIAN TV",			0xF33A, 0x2C3A, 0x3C3A, 0x0000 },
	{ 337,	GB, "GRANADA PLUS",			0x4D5A, 0x5BF4, 0x3B74, 0x0000 },
	{ 338,	GB, "GRANADA Timeshare",		0x4D5B, 0x5BF5, 0x3B75, 0x0000 },
	{ 339,	GB, "GRANADA TV",			0xADD8, 0x2C18, 0x3C18, 0x0000 },
	{ 340,	GB, "HISTORY CHANNEL",			0xFCF4, 0x5BF6, 0x3B76, 0x0000 },
	{ 341,	GB, "HTV",				0x5AAF, 0x2C3F, 0x3C3F, 0x0000 },
	{ 342,	GB, "ITV NETWORK",			0xC8DE, 0x2C1E, 0x3C1E, 0x0000 },
	{ 343,	GB, "LEARNING CHANNEL",			0x4406, 0x5BF7, 0x3B77, 0x0000 },
	{ 344,	GB, "Live TV",				0x4409, 0x5BF8, 0x3B78, 0x0000 },
	{ 345,	GB, "LWT",				0x884B, 0x2C0B, 0x3C0B, 0x0000 },
	{ 346,	GB, "MERIDIAN",				0x10E4, 0x2C34, 0x3C34, 0x0000 },
	{ 347,	GB, "MOVIE CHANNEL",			0xFCFB, 0x2C1B, 0x3C1B, 0x0000 },
	{ 348,	GB, "MTV",				0x4D54, 0x2C14, 0x3C14, 0x0000 },
	{ 380,  GB, "National Geographic Channel",	0x320B, 0x0000, 0x0000, 0x0000 },
	{ 349,	GB, "NBC Europe",			0x8E71, 0x2C31, 0x3C31, 0x0E86 },
	/* not in TR 101 231: 0x0E86 */
	{ 350,	GB, "Nickelodeon UK",			0xA460, 0x0000, 0x0000, 0x0000 },
	{ 351,	GB, "Paramount Comedy Channel UK",	0xA465, 0x0000, 0x0000, 0x0000 },
	{ 352,	GB, "QVC UK",				0x5C44, 0x0000, 0x0000, 0x0000 },
	{ 353,	GB, "RACING CHANNEL",			0xFCF3, 0x2C13, 0x3C13, 0x0000 },
	{ 354,	GB, "Sianel Pedwar Cymru",		0xB4C7, 0x2C07, 0x3C07, 0x0000 },
	{ 355,	GB, "SCI FI CHANNEL",			0xFCF5, 0x2C15, 0x3C15, 0x0000 },
	{ 356,	GB, "SCOTTISH TV",			0xF9D2, 0x2C12, 0x3C12, 0x0000 },
	{ 357,	GB, "SKY GOLD",				0xFCF9, 0x2C19, 0x3C19, 0x0000 },
	{ 358,	GB, "SKY MOVIES PLUS",			0xFCFC, 0x2C0C, 0x3C0C, 0x0000 },
	{ 359,	GB, "SKY NEWS",				0xFCFD, 0x2C0D, 0x3C0D, 0x0000 },
	{ 360,	GB, "SKY ONE",				0xFCFE, 0x2C0E, 0x3C0E, 0x0000 },
	{ 361,	GB, "SKY SOAPS",			0xFCF7, 0x2C17, 0x3C17, 0x0000 },
	{ 362,	GB, "SKY SPORTS",			0xFCFA, 0x2C1A, 0x3C1A, 0x0000 },
	{ 363,	GB, "SKY SPORTS 2",			0xFCF8, 0x2C08, 0x3C08, 0x0000 },
	{ 364,	GB, "SKY TRAVEL",			0xFCF6, 0x5BF9, 0x3B79, 0x0000 },
	{ 365,	GB, "SKY TWO",				0xFCFF, 0x2C0F, 0x3C0F, 0x0000 },
	{ 366,	GB, "SSVC",				0x37E5, 0x2C25, 0x3C25, 0x0000 },
	{ 367,	GB, "TNT / Cartoon Network",		0x44C1, 0x0000, 0x0000, 0x0000 },
	{ 368,	GB, "TYNE TEES TV",			0xA82C, 0x2C2C, 0x3C2C, 0x0000 },
	{ 369,	GB, "UK GOLD",				0x4401, 0x5BFA, 0x3B7A, 0x0000 },
	{ 370,	GB, "UK LIVING",			0x4402, 0x2C01, 0x3C01, 0x0000 },
	{ 371,	GB, "ULSTER TV",			0x833B, 0x2C3D, 0x3C3D, 0x0000 },
	{ 372,	GB, "VH-1",				0x4D58, 0x2C20, 0x3C20, 0x0000 },
	{ 373,	GB, "VH-1 German",			0x4D59, 0x2C21, 0x3C21, 0x0000 },
	{ 374,	GB, "WESTCOUNTRY TV",			0x25D0, 0x2C30, 0x3C30, 0x0000 },
	{ 375,	GB, "WIRE TV",				0x4403, 0x2C3C, 0x3C3C, 0x0000 },
	{ 376,	GB, "YORKSHIRE TV",			0xFA2C, 0x2C2D, 0x3C2D, 0x0000 },

	{ 0, 0,  0,  0, 0, 0, 0 }
};

#if 1

/*
 *  ETS 300 231 Table 3: Codes for programme type (PTY) Principle of classification
 */
const char *
ets_program_class[16] =
{
	"undefined content",
	"drama & films",
	"news/current affairs/social",
	"show/game show/leisure hobbies",
	"sports",
	"children/youth/education/science",
	"music/ballet/Dance",
	"arts/culture (without music)",
	"series code",
	"series code",
	"series code",
	"series code",
	"series code",
	"series code",
	"series code",
	"series code",
};

#endif

/*
 *  ETS 300 231 Table 3: Codes for programme type (PTY) Principle of classification
 */
const char *
ets_program_type[8][16] =
{
	{
		0
	}, {
		"movie (general)",
		"detective/thriller",
		"adventure/western/war",
		"science fiction/fantasy/horror",
		"comedy",
		"soap/melodrama/folklore",
		"romance",
		"serious/classical/religious/historical drama",
		"adult movie"
	}, {
		"news/current affairs (general)",
		"news/weather report",
		"news magazine",
		"documentary",
		"discussion/interview/debate",
		"social/political issues/economics (general)",
		"magazines/reports/documentary",
		"economics/social advisory",
		"remarkable people"
	}, {
		"show/game show (general)",
		"game show/quiz/contest",
		"variety show",
		"talk show",
		"leisure hobbies (general)",
		"tourism/travel",
		"handicraft",
		"motoring",
		"fitness & health",
		"cooking",
		"advertisement/shopping",
		0,
		0,
		0,
		0,
		"alarm/emergency identification"
	}, {
		"sports (general)"
		"special event (Olympic Games, World Cup etc.)",
		"sports magazine",
		"football/soccer",
		"tennish/squash",
		"team sports (excluding football)",
		"athletics",
		"motor sport",
		"water sport",
		"winter sports",
		"equestrian",
		"martial sports",
		"local sports"
	}, {
		"children's/youth programmes (general)",
		"pre-school children's programmes",
		"entertainment programmes for 6 to 14",
		"entertainment programmes for 10 to 16",
		"informational/educational/school programmes",
		"cartoons/puppets",
		"education/science/factual topics (general)",
		"nature/animals/environement",
		"technology/natural sciences",
		"medicine/physiology/psychology",
		"foreign countries/expeditions",
		"social/spiritual sciences",
		"further education",
		"languages"
	}, {
		"music/ballet/dance (general)",
		"rock/Pop",
		"serious music/classical Music",
		"folk/traditional music",
		"jazz",
		"musical/opera",
		"ballet"
	}, {
		"arts/culture (general)",
		"performing arts",
		"fine arts",
		"religion",
		"popular culture/traditional arts",
		"literature",
		"film/cinema",
		"experimental film/video",
		"broadcasting/press",
		"new media",
		"arts/culture magazines",
		"fashion"
	}
};

static const char *
eia608_program_type[96] =
{
	"education",
	"entertainment",
	"movie",
	"news",
	"religious",
	"sports",
	"other",
	"action",
	"advertisement",
	"animated",
	"anthology",
	"automobile",
	"awards",
	"baseball",
	"basketball",
	"bulletin",
	"business",
	"classical",
	"college",
	"combat",
	"comedy",
	"commentary",
	"concert",
	"consumer",
	"contemporary",
	"crime",
	"dance",
	"documentary",
	"drama",
	"elementary",
	"erotica",
	"exercise",
	"fantasy",
	"farm",
	"fashion",
	"fiction",
	"food",
	"football",
	"foreign",
	"fund raiser",
	"game/quiz",
	"garden",
	"golf",
	"government",
	"health",
	"high school",
	"history",
	"hobby",
	"hockey",
	"home",
	"horror",
	"information",
	"instruction",
	"international",
	"interview",
	"language",
	"legal",
	"live",
	"local",
	"math",
	"medical",
	"meeting",
	"military",
	"miniseries",
	"music",
	"mystery",
	"national",
	"nature",
	"police",
	"politics",
	"premiere",
	"prerecorded",
	"product",
	"professional",
	"public",
	"racing",
	"reading",
	"repair",
	"repeat",
	"review",
	"romance",
	"science",
	"series",
	"service",
	"shopping",
	"soap opera",
	"special",
	"suspense",
	"talk",
	"technical",
	"tennis",
	"travel",
	"variety",
	"video",
	"weather",
	"western"
};

/**
 * vbi_rating_str_by_id:
 * @auth: From #vbi_program_info.rating_auth.
 * @id: From #vbi_program_info.rating_id.
 * 
 * Translate a #vbi_program_info program rating code into a
 * Latin-1 string, native language.
 * 
 * Return value: 
 * Static pointer to the string (don't free()), or %NULL if
 * this code is undefined.
 **/
char *
vbi_rating_string(vbi_rating_auth auth, int id)
{
	static const char *ratings[4][8] = {
		{ NULL, "G", "PG", "PG-13", "R", "NC-17", "X", "Not rated" },
		{ "Not rated", "TV-Y", "TV-Y7", "TV-G", "TV-PG", "TV-14", "TV-MA", "Not rated" },
		{ "Exempt", "C", "C8+", "G", "PG", "14+", "18+", NULL },
		{ "Exempt", "G", "8 ans +", "13 ans +", "16 ans +", "18 ans +", NULL, NULL },
	};

	if (id < 0 || id > 7)
		return NULL;

	switch (auth) {
	case VBI_RATING_AUTH_MPAA:
		return (char *) ratings[0][id];

	case VBI_RATING_AUTH_TV_US:
		return (char *) ratings[1][id];

	case VBI_RATING_AUTH_TV_CA_EN:
		return (char *) ratings[2][id];

	case VBI_RATING_AUTH_TV_CA_FR:
		return (char *) ratings[3][id];

	default:
		return NULL;
	}
}

/**
 * vbi_prog_type_str_by_id:
 * @classf: From #vbi_program_info.type_classf.
 * @id: From #vbi_program_info.type_id.
 * 
 * Translate a #vbi_program_info program type code into a
 * Latin-1 string, currently English only.
 * 
 * Return value: 
 * Static pointer to the string (don't free()), or %NULL if
 * this code is undefined.
 **/
char *
vbi_prog_type_string(vbi_prog_classf classf, int id)
{
	switch (classf) {
	case VBI_PROG_CLASSF_EIA_608:
		if (id < 0x20 || id > 0x7F)
			return NULL;
		return (char *) eia608_program_type[id - 0x20];

	case VBI_PROG_CLASSF_ETS_300231:
		if (id < 0x00 || id > 0x7F)
			return NULL;
		return (char *) ets_program_type[0][id];

	default:
		return NULL;
	}
}
