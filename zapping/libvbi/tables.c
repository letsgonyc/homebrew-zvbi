/*
 *  Zapzilla - Teletext / PDC tables
 *
 *  PDC and VPS CNI codes rev. 3, based on TR 101 231 EBU (2000-07)
 *  Programme type tables (PDC/EPG)
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
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

/* $Id: tables.c,v 1.1 2000-12-29 00:38:30 mschimek Exp $ */

/*
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
 *  ISO 3166-1 country codes
 */
enum {
	AF, AL, DZ, AS, AD, AO, AI, AQ, AG, AR, AM, AW, AU, AT, AZ, BS,
	BH, BD, BB, BY, BE, BZ, BJ, BM, BT, BO, BA, BW, BV, BR, IO, BN,
	BG, BF, BI, KH, CM, CA, CV, KY, CF, TD, CL, CN, CX, CC, CO, KM,
	CG, CD, CK, CR, CI, HR, CU, CY, CZ, DK, DJ, DM, DO, TP, EC, EG,
	SV, GQ, ER, EE, ET, FK, FO, FJ, FI, FR, GF, PF, TF, GA, GM, GE,
	DE, GH, GI, GR, GL, GD, GP, GU, GT, GN, GW, GY, HT, HM, VA, HN,
	HK, HU, IS, IN, ID, IR, IQ, IE, IL, IT, JM, JP, JO, KZ, KE, KI,
	KP, KR, KW, KG, LA, LV, LB, LS, LR, LY, LI, LT, LU, MO, MK, MG,
	MW, MY, MV, ML, MT, MH, MQ, MR, MU, YT, MX, FM, MD, MC, MN, MS,
	MA, MZ, MM, NA, NR, NP, NL, AN, NC, NZ, NI, NE, NG, NU, NF, MP,
	NO, OM, PK, PW, PS, PA, PG, PY, PE, PH, PN, PL, PT, PR, QA, RE,
	RO, RU, RW, SH, KN, LC, PM, VC, WS, SM, ST, SA, SN, SC, SL, SG,
	SK, SI, SB, SO, ZA, GS, ES, LK, SD, SR, SJ, SZ, SE, CH, SY, TW,
	TJ, TZ, TH, TG, TK, TO, TT, TN, TR, TM, TC, TV, UG, UA, AE, GB,
	US, UM, UY, UZ, VU, VE, VN, VG, VI, WF, EH, YE, YU, ZM, ZW
};

 char *
country_names_en[] = {
	"Afghanistan",
	"Albania",
	"Algeria",
	"American Samoa",
	"Andorra",
	"Angola",
	"Anguilla",
	"Antarctica",
	"Antigua and Barbuda",
	"Argentina",
	"Armenia",
	"Aruba",
	"Australia",
	"Austria",
	"Azerbaijan",
	"Bahamas",
	"Bahrain",
	"Bangladesh",
	"Barbados",
	"Belarus",
	"Belgium",
	"Belize",
	"Benin",
	"Bermuda",
	"Bhutan",
	"Bolivia",
	"Bosnia and Herzegovina",
	"Botswana",
	"Bouvet island",
	"Brazil",
	"British Indian Ocean Territory",
	"Brunei Darussalam",
	"Bulgaria",
	"Burkina Faso",
	"Burundi",
	"Cambodia",
	"Cameroon",
	"Canada",
	"Cape Verde",
	"Cayman Islands",
	"Central African Republic",
	"Chad",
	"Chile",
	"China",
	"Christmas Island",
	"Cocos (Keeling) Islands",
	"Colombia",
	"Comoros",
	"Congo",
	"Congo, the Democratic Republic of the",
	"Cook Islands",
	"Costa Rica",
	"Cote D'Ivoire",
	"Croatia",
	"Cuba",
	"Cyprus",
	"Czech Republic",
	"Denmark",
	"Djibouti",
	"Dominica",
	"Dominican Republic",
	"East Timor",
	"Ecuador",
	"Egypt",
	"El Salvador",
	"Equatorial Guinea",
	"Eritrea",
	"Estonia",
	"Ethiopia",
	"Falkland Islands (Malvinas)",
	"Faroe Islands",
	"Fiji",
	"Finland",
	"France",
	"French Guiana",
	"French Polynesia",
	"French Southern Territories",
	"Gabon",
	"Gambia",
	"Georgia",
	"Germany",
	"Ghana",
	"Gibraltar",
	"Greece",
	"Greenland",
	"Grenada",
	"Guadeloupe",
	"Guam",
	"Guatemala",
	"Guinea",
	"Guinea-Bissau",
	"Guyana",
	"Haiti",
	"Heard Island and McDonald Islands",
	"Holy See (Vatican City State)",
	"Honduras",
	"Hong Kong",
	"Hungary",
	"Iceland",
	"India",
	"Indonesia",
	"Iran, Islamic Republic of",
	"Iraq",
	"Ireland",
	"Israel",
	"Italy",
	"Jamaica",
	"Japan",
	"Jordan",
	"Kazakstan",
	"Kenya",
	"Kiribati",
	"Korea, Democratic People's Republic of",
	"Korea, Republic of",
	"Kuwait",
	"Kyrgyzstan",
	"Lao People's Democratic Republic",
	"Latvia",
	"Lebanon",
	"Lesotho",
	"Liberia",
	"Libyan Arab Jamahiriya",
	"Liechtenstein",
	"Lithuania",
	"Luxembourg",
	"Macau",
	"Macedonia, The former Yugoslav Republic of",
	"Madagascar",
	"Malawi",
	"Malaysia",
	"Maldives",
	"Mali",
	"Malta",
	"Marshall Islands",
	"Martinique",
	"Mauritania",
	"Mauritius",
	"Mayotte",
	"Mexico",
	"Micronesia, Federated States of",
	"Moldova, Republic of",
	"Monaco",
	"Mongolia",
	"Montserrat",
	"Morocco",
	"Mozambique",
	"Myanmar",
	"Namibia",
	"Nauru",
	"Nepal",
	"Netherlands",
	"Netherlands Antilles",
	"New Caledonia",
	"New Zealand",
	"Nicaragua",
	"Niger",
	"Nigeria",
	"Niue",
	"Norfolk Island",
	"Northern Mariana Islands",
	"Norway",
	"Oman",
	"Pakistan",
	"Palau",
	"Palestinian Territory, Occupied",
	"Panama",
	"Papua new guinea",
	"Paraguay",
	"Peru",
	"Philippines",
	"Pitcairn",
	"Poland",
	"Portugal",
	"Puerto Rico",
	"Qatar",
	"Reunion",
	"Romania",
	"Russian Federation",
	"Rwanda",
	"Saint Helena",
	"Saint Kitts and Nevis",
	"Saint Lucia",
	"Saint Pierre and Miquelon",
	"Saint Vincent and the Grenadines",
	"Samoa",
	"San Marino",
	"Sao Tome and Principe",
	"Saudi Arabia",
	"Senegal",
	"Seychelles",
	"Sierra Leone",
	"Singapore",
	"Slovakia",
	"Slovenia",
	"Solomon Islands",
	"Somalia",
	"South Africa",
	"South Georgia and the South Sandwich Islands",
	"Spain",
	"Sri Lanka",
	"Sudan",
	"Suriname",
	"Svalbard and Jan Mayen",
	"Swaziland",
	"Sweden",
	"Switzerland",
	"Syrian Arab Republic",
	"Taiwan, Province of China",
	"Tajikistan",
	"Tanzania, United Republic of",
	"Thailand",
	"Togo",
	"Tokelau",
	"Tonga",
	"Trinidad and Tobago",
	"Tunisia",
	"Turkey",
	"Turkmenistan",
	"Turks and Caicos Islands",
	"Tuvalu",
	"Uganda",
	"Ukraine",
	"United Arab Emirates",
	"United Kingdom",
	"United States",
	"United States Minor Outlying Islands",
	"Uruguay",
	"Uzbekistan",
	"Vanuatu",
	"Venezuela",
	"Viet Nam",
	"Virgin Islands, British",
	"Virgin Islands, U.S.",
	"Wallis and Futuna",
	"Western Sahara",
	"Yemen",
	"Yugoslavia",
	"Zambia",
	"Zimbabwe"
};

/*
 *  TR 101 231 Table A.1: Register of Country and Network Identification (CNI) codes for
 *                        Teletext based systems
 */
struct {
	int		country;
	char *		short_name;	/* 8 chars */
	char *		long_name;
	unsigned short	cni1;		/* Packet 8/30 format 1 */
	unsigned short	cni2;		/* Packet 8/30 format 2 */
	unsigned short	cni3;		/* Packet X/26 */
} PDC_CNI[] = {
	{ BE, "BRTN TV1", "BRTN TV1",				0x3201, 0x1601, 0x3603 },
	{ BE, "Ka2",	  "Ka2",				0x3206, 0x1606, 0x3606 },
	{ BE, "RTBF 1",	  "RTBF 1",				0x3203, 0x0000, 0x0000 },
	{ BE, "RTBF 2",	  "RTBF 2",				0x3204, 0x0000, 0x0000 },
	{ BE, "TV2",	  "TV2",				0x3202, 0x1602, 0x3602 },
	{ BE, "VT4",	  "VT4",				0x0404, 0x1604, 0x3604 },
	{ BE, "VTM",	  "VTM",				0x3205, 0x1605, 0x3605 },

	{ HR, "HRT",	  "HRT",				0x0385, 0x0000, 0x0000 },

	{ CZ, "CT 1",	  "CT 1",				0x4201, 0x32C1, 0x3C21 },
	{ CZ, "CT 2",	  "CT 2",				0x4202, 0x32C2, 0x3C22 },
	{ CZ, "CT 1",	  "CT1 Regional",			0x4231, 0x32F1, 0x3C25 },
	{ CZ, "CT 1",	  "CT1 Brno",				0x4211, 0x32D1, 0x3B01 },
	{ CZ, "CT 1",	  "CT1 Ostravia",			0x4221, 0x32E1, 0x3B02 },
	{ CZ, "CT 2",	  "CT2 Regional",			0x4232, 0x32F2, 0x3B03 },
	{ CZ, "CT 2",	  "CT2 Brno",				0x4212, 0x32D2, 0x3B04 },
	{ CZ, "CT 2",	  "CT2 Ostravia",			0x4222, 0x32E2, 0x3B05 },
	{ CZ, "NOVA TV",  "NOVA TV",				0x4203, 0x32C3, 0x3C23 },
	
	{ DK, "DR1",	  "DR1",				0x7392, 0x2901, 0x3901 },
	{ DK, "DR2",	  "DR2",				0x49CF, 0x2903, 0x3903 },
	{ DK, "TV2",	  "TV2",				0x4502, 0x2902, 0x3902 },
	{ DK, "TV2 Zulu", "TV2 Zulu",				0x4503, 0x2904, 0x3904 },

	{ FI, "OWL3",	  "OWL3",				0x358F, 0x260F, 0x3614 },
	{ FI, "YLE1",	  "YLE1",				0x3581, 0x2601, 0x3601 },
	{ FI, "YLE2",	  "YLE2",				0x3582, 0x2602, 0x3607 },

	{ FR, "AB1",	  "AB1",				0x33C1, 0x2FC1, 0x3F41 },
	{ FR, "Aqui TV",  "Aqui TV",				0x3320, 0x2F20, 0x3F20 },
	{ FR, "Arte",	  "Arte / La Cinqui�me",		0x330A, 0x2F0A, 0x3F0A },
	{ FR, "Canal J",  "Canal J",				0x33C2, 0x2FC2, 0x3F42 },
	{ FR, "CJ",	  "Canal Jimmy",			0x33C3, 0x2FC3, 0x3F43 },
	{ FR, "Canal+",	  "Canal+",				0x33F4, 0x2F04, 0x3F04 },
	{ FR, "Euronews", "Euronews",				0xFE01, 0x2FE1, 0x3F61 },
	{ FR, "Sport",    "Eurosport",				0xF101, 0x2FE2, 0x3F62 },
	{ FR, "France 2", "France 2",				0x33F2, 0x2F02, 0x3F02 },
	{ FR, "France 3", "France 3",				0x33F3, 0x2F03, 0x3F03 },
	{ FR, "LCM",	  "La Cha�ne M�t�o",			0x33C5, 0x2FC5, 0x3F45 },
	{ FR, "LCI",	  "LCI",				0x33C4, 0x2FC4, 0x3F44 },
	{ FR, "M6",	  "M6",					0x33F6, 0x2F06, 0x3F06 },
	{ FR, "MCM",	  "MCM",				0x33C6, 0x2FC6, 0x3F46 },
	{ FR, "PP",	  "Paris Premi�re",			0x33C8, 0x2FC8, 0x3F48 },
	{ FR, "Plan�te",  "Plan�te",				0x33C9, 0x2FC9, 0x3F49 },
	{ FR, "RFO1",	  "RFO1",				0x3311, 0x2F11, 0x3F11 },
	{ FR, "RFO2",	  "RFO2",				0x3312, 0x2F12, 0x3F12 },
	{ FR, "SC",	  "S�rie Club",				0x33CA, 0x2FCA, 0x3F4A },
	{ FR, "T�l�toon", "T�l�toon",				0x33CB, 0x2FCB, 0x3F4B },
	{ FR, "T�va",	  "T�va",				0x33CC, 0x2FCC, 0x3F4C },
	{ FR, "TF1",	  "TF1",				0x33F1, 0x2F01, 0x3F01 },
	{ FR, "TLM",	  "TLM",				0x3321, 0x2F21, 0x3F21 },
	{ FR, "TLT",	  "TLT",				0x3322, 0x2F22, 0x3F22 },
	{ FR, "TMC",	  "TMC Monte-Carlo",			0x33C7, 0x2FC7, 0x3F47 },
	{ FR, "TV5",	  "TV5",				0xF500, 0x2FE5, 0x3F65 },

	/* not in TR 101 231: 0x4901, 0x3d41 */
	{ DE, "ARD",	  "Erstes Deutsches Fernsehen",		0x4901, 0x0000, 0x3D41 },
	/* not in TR 101 231: 0x4902, 0x3d42 */
	{ DE, "ZDF",	  "Zweites Deutsches Fernsehen",	0x4902, 0x0000, 0x3D42 },
	/* not in TR 101 231: 0x3d4b */
	{ DE, "BR-3",	  "BR-3 Landesweit",			0x0000, 0x0000, 0x3D4B },
	/* not in TR 101 231: 0x49e1 */
	{ DE, "SW 3",	  "SW 3 Baden-W�rttemberg",		0x49E1, 0x0000, 0x0000 },
	/* not in TR 101 231: 0x3d05 */
	{ DE, "Arte",	  "Arte",				0x490A, 0x0000, 0x3D05 },
	{ DE, "Phoenix",  "Phoenix",				0x4908, 0x0000, 0x0000 },
	{ DE, "QVC D",	  "QVC D",				0x5C49, 0x0000, 0x0000 },
	{ DE, "VOX",	  "VOX Television",			0x490C, 0x0000, 0x0000 },
	/* not in TR 101 231: 0x0D8F */
	{ DE, "RTL 2",	  "RTL 2",				0x0D8F, 0x0000, 0x0000 },

	{ GR, "ET-1",	  "ET-1",				0x3001, 0x2101, 0x3101 },
	{ GR, "NET",	  "NET",				0x3002, 0x2102, 0x3102 },
	{ GR, "ET-3",	  "ET-3",				0x3003, 0x2103, 0x3103 },

	{ HU, "Duna TV",  "Duna Televizio",			0x3636, 0x0000, 0x0000 },
	{ HU, "MTV1",	  "MTV1",				0x3601, 0x0000, 0x0000 },
	{ HU, "MTV1",	  "MTV1 Budapest",			0x3611, 0x0000, 0x0000 },
	{ HU, "MTV1",	  "MTV1 Debrecen",			0x3651, 0x0000, 0x0000 },
	{ HU, "MTV1",	  "MTV1 Miskolc",			0x3661, 0x0000, 0x0000 },
	{ HU, "MTV1",	  "MTV1 P�cs",				0x3621, 0x0000, 0x0000 },
	{ HU, "MTV1",	  "MTV1 Szeged",			0x3631, 0x0000, 0x0000 },
	{ HU, "MTV1",	  "MTV1 Szombathely",			0x3641, 0x0000, 0x0000 },
	{ HU, "MTV2",	  "MTV2",				0x3602, 0x0000, 0x0000 },
	{ HU, "tv2",	  "tv2",				0x3622, 0x0000, 0x0000 },

	{ IS, "RS",	  "Rikisutvarpid-Sjonvarp",		0x3541, 0x0000, 0x0000 },

	{ IE, "Netwk 2",  "Network 2",				0x3532, 0x4202, 0x3202 },
	{ IE, "RTE1",	  "RTE1",				0x3531, 0x4201, 0x3201 },
	{ IE, "TNG",	  "Teilifis na Gaeilge",		0x3533, 0x4203, 0x3203 },
	{ IE, "TV3",	  "TV3",				0x3333, 0x0000, 0x0000 },

	{ IT, "Arte",	  "Arte",				0x390A, 0x0000, 0x0000 },
	{ IT, "Canale 5", "Canale 5",				0xFA05, 0x0000, 0x0000 },
	{ IT, "Italia 1", "Italia 1",				0xFA06, 0x0000, 0x0000 },
	{ IT, "RAI 1",	  "RAI 1",				0x3901, 0x0000, 0x0000 },
	{ IT, "RAI 2",	  "RAI 2",				0x3902, 0x0000, 0x0000 },
	{ IT, "RAI 3",	  "RAI 3",				0x3903, 0x0000, 0x0000 },
	{ IT, "Rete 4",	  "Rete 4",				0xFA04, 0x0000, 0x0000 },
	{ IT, "Rete A",	  "Rete A",				0x3904, 0x0000, 0x0000 },
	{ IT, "RTV38",	  "RTV38",				0x3938, 0x0000, 0x0000 },
	{ IT, "Tele+1",	  "Tele+1",				0x3997, 0x0000, 0x0000 },
	{ IT, "Tele+2",	  "Tele+2",				0x3998, 0x0000, 0x0000 },
	{ IT, "Tele+3",	  "Tele+3",				0x3999, 0x0000, 0x0000 },
	{ IT, "TMC",	  "TMC",				0xFA08, 0x0000, 0x0000 },
	{ IT, "TRS TV",	  "TRS TV",				0x3910, 0x0000, 0x0000 },

	{ LU, "RTL",	  "RTL T�l� Letzebuerg",		0x4000, 0x0000, 0x0000 },

	{ NL, "NL 1",	  "Nederland 1",			0x3101, 0x4801, 0x3801 },
	{ NL, "NL 2",	  "Nederland 2",			0x3102, 0x4802, 0x3802 },
	{ NL, "NL 3",	  "Nederland 3",			0x3103, 0x4803, 0x3803 },
	{ NL, "RTL 4",	  "RTL 4",				0x3104, 0x4804, 0x3804 },
	{ NL, "RTL 5",	  "RTL 5",				0x3105, 0x4805, 0x3805 },
	{ NL, "Veronica", "Veronica",				0x3106, 0x4806, 0x3806 },
	{ NL, "NRK1",	  "NRK1",				0x4701, 0x0000, 0x0000 },
	{ NL, "NRK2",	  "NRK2",				0x4703, 0x0000, 0x0000 },
	{ NL, "TV 2",	  "TV 2",				0x4702, 0x0000, 0x0000 },
	{ NL, "TV Norge", "TV Norge",				0x4704, 0x0000, 0x0000 },

	{ PL, "TV Pol.",  "TV Polonia",				0x4810, 0x0000, 0x0000 },
	{ PL, "TVP1",	  "TVP1",				0x4801, 0x0000, 0x0000 },
	{ PL, "TVP2",	  "TVP2",				0x4802, 0x0000, 0x0000 },

	{ PT, "RTP1",	  "RTP1",				0x3510, 0x0000, 0x0000 },
	{ PT, "RTP2",	  "RTP2",				0x3511, 0x0000, 0x0000 },
    	{ PT, "RTPAF",	  "RTPAF",				0x3512, 0x0000, 0x0000 },
	{ PT, "RTPAZ",	  "RTPAZ",				0x3514, 0x0000, 0x0000 },
	{ PT, "RTPI",	  "RTPI",				0x3513, 0x0000, 0x0000 },
	{ PT, "RTPM",	  "RTPM",				0x3515, 0x0000, 0x0000 },

	{ SM, "RTV",	  "RTV",				0x3781, 0x0000, 0x0000 },

	{ SK, "STV1",	  "STV1",				0x42A1, 0x35A1, 0x3521 },
	{ SK, "STV2",	  "STV2",				0x42A2, 0x35A2, 0x3522 },
	{ SK, "STV1",	  "STV1 Kosice",			0x42A3, 0x35A3, 0x3523 },
	{ SK, "STV2",	  "STV2 Kosice",			0x42A4, 0x35A4, 0x3524 },
	{ SK, "STV1",	  "STV1 B. Bystrica",			0x42A5, 0x35A5, 0x3525 },
	{ SK, "STV2",	  "STV2 B. Bystrica",			0x42A6, 0x35A6, 0x3526 },

	{ SI, "SLO1",	  "SLO1",				0xAAE1, 0x0000, 0x0000 },
	{ SI, "SLO2",	  "SLO2",				0xAAE2, 0x0000, 0x0000 },
	{ SI, "KC",	  "KC",					0xAAE3, 0x0000, 0x0000 },
	{ SI, "TLM",	  "TLM",				0xAAE4, 0x0000, 0x0000 },
	{ SI, "SLO3",	  "SLO3",				0xAAF1, 0x0000, 0x0000 },

        { ES, "Arte",	  "Arte",				0x340A, 0x0000, 0x0000 },
	{ ES, "C33",	  "C33",				0xCA33, 0x0000, 0x0000 },
	{ ES, "ETB 1",	  "ETB 1",				0xBA01, 0x0000, 0x0000 },
	{ ES, "ETB 2",	  "ETB 2",				0x3402, 0x0000, 0x0000 },
	{ ES, "TV3",	  "TV3",				0xCA03, 0x0000, 0x0000 },
	{ ES, "TVE1",	  "TVE1",				0x3E00, 0x0000, 0x0000 },
	{ ES, "TVE2",	  "TVE2",				0xE100, 0x0000, 0x0000 },
	/* not in TR 101 231: 0xA55A (valid?) */
	{ ES, "Canal+",	  "Canal+",                             0xA55A, 0x0000, 0x0000 },

	{ SE, "SVT 1",	  "SVT 1",				0x4601, 0x4E01, 0x3E01 },
	{ SE, "SVT 2",	  "SVT 2",				0x4602, 0x4E02, 0x3E02 },
	{ SE, "SVT Test", "SVT Test Transmissions",		0x4600, 0x4E00, 0x3E00 },
	{ SE, "TV 4",	  "TV 4",				0x4640, 0x4E40, 0x3E40 },

	{ CH, "SAT ACC.", "SAT ACCESS",				0x410A, 0x24CA, 0x344A },
	{ CH, "SF 1",	  "SF 1",				0x4101, 0x24C1, 0x3441 },
	{ CH, "SF 2",	  "SF 2",				0x4107, 0x24C7, 0x3447 },
	{ CH, "TSI 1",	  "TSI 1",				0x4103, 0x24C3, 0x3443 },
	{ CH, "TSI 2",	  "TSI 2",				0x4109, 0x24C9, 0x3449 },
	{ CH, "TSR 1",	  "TSR 1",				0x4102, 0x24C2, 0x3442 },
	{ CH, "TSR 2",	  "TSR 2",				0x4108, 0x24C8, 0x3448 },

	{ TR, "ATV",	  "ATV",				0x900A, 0x0000, 0x0000 },
	{ TR, "AVRASYA",  "AVRASYA",				0x9006, 0x4306, 0x3306 },
	{ TR, "BRAVO TV", "BRAVO TV",				0x900E, 0x0000, 0x0000 },
	{ TR, "Cine 5",	  "Cine 5",				0x9008, 0x0000, 0x0000 },
	{ TR, "EKO TV",	  "EKO TV",				0x900D, 0x0000, 0x0000 },
	{ TR, "EURO D",	  "EURO D",				0x900C, 0x0000, 0x0000 },
	{ TR, "FUN TV",	  "FUN TV",				0x9010, 0x0000, 0x0000 },
	{ TR, "GAL. TV",  "GALAKSI TV",				0x900F, 0x0000, 0x0000 },
	{ TR, "KANAL D",  "KANAL D",				0x900B, 0x0000, 0x0000 },
	{ TR, "Show TV",  "Show TV",				0x9007, 0x0000, 0x0000 },
	{ TR, "Sport",	  "Super Sport",			0x9009, 0x0000, 0x0000 },
	{ TR, "TEMPO TV", "TEMPO TV",				0x9011, 0x0000, 0x0000 },
	{ TR, "TGRT",	  "TGRT",				0x9014, 0x0000, 0x0000 },
	{ TR, "TRT-1",	  "TRT-1",				0x9001, 0x4301, 0x3301 },
	{ TR, "TRT-2",	  "TRT-2",				0x9002, 0x4302, 0x3302 },
	{ TR, "TRT-3",	  "TRT-3",				0x9003, 0x4303, 0x3303 },
	{ TR, "TRT-4",	  "TRT-4",				0x9004, 0x4304, 0x3304 },
	{ TR, "TRT-INT",  "TRT-INT",				0x9005, 0x4305, 0x3305 },

	{ GB, "ANGLIA",   "ANGLIA TV",				0xFB9C, 0x2C1C, 0x3C1C },
	{ GB, "BBC News", "BBC News 24",			0x4469, 0x2C69, 0x3C69 },
	{ GB, "BBC Prme", "BBC Prime",				0x4468, 0x2C68, 0x3C68 },
	{ GB, "BBC Wrld", "BBC World",				0x4457, 0x2C57, 0x3C57 },
	{ GB, "BBC1",	  "BBC1",				0x447F, 0x2C7F, 0x3C7F },
	{ GB, "BBC1",	  "BBC1 NI",				0x4441, 0x2C41, 0x3C41 },
	{ GB, "BBC1",	  "BBC1 Scotland",			0x447B, 0x2C7B, 0x3C7B },
	{ GB, "BBC1",	  "BBC1 Wales",				0x447D, 0x2C7D, 0x3C7D },
	{ GB, "BBC2",	  "BBC2",				0x4440, 0x2C40, 0x3C40 },
	{ GB, "BBC2",	  "BBC2 NI",				0x447E, 0x2C7E, 0x3C7E },
	{ GB, "BBC2",	  "BBC2 Scotland",			0x4444, 0x2C44, 0x3C44 },
	{ GB, "BBC2",	  "BBC2 Wales",				0x4442, 0x2C42, 0x3C42 },
	{ GB, "BORDER",   "BORDER TV",				0xB7F7, 0x2C27, 0x3C27 },
	{ GB, "BRAVO",	  "BRAVO",				0x4405, 0x5BEF, 0x3B6F },
	{ GB, "CAR. S.",  "CARLTON SELECT",			0x82E1, 0x2C05, 0x3C05 },
	{ GB, "CARLTON",  "CARLTON TV",				0x82DD, 0x2C1D, 0x3C1D },
	{ GB, "CENTRAL",  "CENTRAL TV",				0x2F27, 0x2C37, 0x3C37 },
	{ GB, "CHAN. 4",  "CHANNEL 4",				0xFCD1, 0x2C11, 0x3C11 },
	{ GB, "CHAN. 5",  "CHANNEL 5 (1)",			0x9602, 0x2C02, 0x3C02 },
	{ GB, "CHAN. 5",  "CHANNEL 5 (2)",			0x1609, 0x2C09, 0x3C09 },
	{ GB, "CHAN. 5",  "CHANNEL 5 (3)",			0x28EB, 0x2C2B, 0x3C2B },
	{ GB, "CHAN. 5",  "CHANNEL 5 (4)",			0xC47B, 0x2C3B, 0x3C3B },
	{ GB, "CH. TV",	  "CHANNEL TV",				0xFCE4, 0x2C24, 0x3C24 },
	{ GB, "CHILDREN", "CHILDREN'S CHANNEL",			0x4404, 0x5BF0, 0x3B70 },
	{ GB, "CNNI",	  "CNN International",			0x01F2, 0x5BF1, 0x3B71 },
	{ GB, "DISCVERY", "DISCOVERY",				0x4407, 0x5BF2, 0x3B72 },
	{ GB, "DISNEY",	  "DISNEY CHANNEL UK",			0x44D1, 0x5BCC, 0x3B4C },
	{ GB, "FAMILY",	  "FAMILY CHANNEL",			0x4408, 0x5BF3, 0x3B73 },
	{ GB, "GMTV",	  "GMTV",				0xADDC, 0x5BD2, 0x3B52 },
	{ GB, "GRAMPIAN", "GRAMPIAN TV",			0xF33A, 0x2C3A, 0x3C3A },
	{ GB, "GRAN. P.", "GRANADA PLUS",			0x4D5A, 0x5BF4, 0x3B74 },
	{ GB, "GRAN. T.", "GRANADA Timeshare",			0x4D5B, 0x5BF5, 0x3B75 },
	{ GB, "GRANADA",  "GRANADA TV",				0xADD8, 0x2C18, 0x3C18 },
	{ GB, "HISTORY",  "HISTORY CHANNEL",			0xFCF4, 0x5BF6, 0x3B76 },
	{ GB, "HTV",	  "HTV",				0x5AAF, 0x2C3F, 0x3C3F },
	{ GB, "ITV",	  "ITV NETWORK",			0xC8DE, 0x2C1E, 0x3C1E },
	{ GB, "LEARNING", "LEARNING CHANNEL",			0x4406, 0x5BF7, 0x3B77 },
	{ GB, "Live TV",  "Live TV",				0x4409, 0x5BF8, 0x3B78 },
	{ GB, "LWT",	  "LWT",				0x884B, 0x2C0B, 0x3C0B },
	{ GB, "MERIDIAN", "MERIDIAN",				0x10E4, 0x2C34, 0x3C34 },
	{ GB, "MOVIE CH", "MOVIE CHANNEL",			0xFCFB, 0x2C1B, 0x3C1B },
	{ GB, "MTV",	  "MTV",				0x4D54, 0x2C14, 0x3C14 },
	{ GB, "NBC",	  "NBC Europe",				0x8E71, 0x2C31, 0x3C31 },
	{ GB, "Nick.",	  "Nickelodeon UK",			0xA460, 0x0000, 0x0000 },
	{ GB, "Paramnt",  "Paramount Comedy Channel UK",	0xA465, 0x0000, 0x0000 },
	{ GB, "QVC UK",	  "QVC UK",				0x5C44, 0x0000, 0x0000 },
	{ GB, "RACING",	  "RACING Ch.",				0xFCF3, 0x2C13, 0x3C13 },
	{ GB, "S4C",	  "Sianel Pedwar Cymru",		0xB4C7, 0x2C07, 0x3C07 },
	{ GB, "SCI FI",	  "SCI FI CHANNEL",			0xFCF5, 0x2C15, 0x3C15 },
	{ GB, "SCOTTISH", "SCOTTISH TV",			0xF9D2, 0x2C12, 0x3C12 },
	{ GB, "SKY GOLD", "SKY GOLD",				0xFCF9, 0x2C19, 0x3C19 },
	{ GB, "SKY MOV.", "SKY MOVIES PLUS",			0xFCFC, 0x2C0C, 0x3C0C },
	{ GB, "SKY NEWS", "SKY NEWS",				0xFCFD, 0x2C0D, 0x3C0D },
	{ GB, "SKY ONE",  "SKY ONE",				0xFCFE, 0x2C0E, 0x3C0E },
	{ GB, "SKY SOAP", "SKY SOAPS",				0xFCF7, 0x2C17, 0x3C17 },
	{ GB, "SKY SP.",  "SKY SPORTS",				0xFCFA, 0x2C1A, 0x3C1A },
	{ GB, "SKY SP.2", "SKY SPORTS 2",			0xFCF8, 0x2C08, 0x3C08 },
	{ GB, "SKY TR.",  "SKY TRAVEL",				0xFCF6, 0x5BF9, 0x3B79 },
	{ GB, "SKY TWO",  "SKY TWO",				0xFCFF, 0x2C0F, 0x3C0F },
	{ GB, "SSVC",	  "SSVC",				0x37E5, 0x2C25, 0x3C25 },
	{ GB, "TNT",	  "TNT / Cartoon Network",		0x44C1, 0x0000, 0x0000 },
	{ GB, "TYNE T.",  "TYNE TEES TV",			0xA82C, 0x2C2C, 0x3C2C },
	{ GB, "UK GOLD",  "UK GOLD",				0x4401, 0x5BFA, 0x3B7A },
	{ GB, "UK LIVNG", "UK LIVING",				0x4402, 0x2C01, 0x3C01 },
	{ GB, "ULSTER",  "ULSTER TV",				0x833B, 0x2C3D, 0x3C3D },
	{ GB, "VH-1",	  "VH-1",				0x4D58, 0x2C20, 0x3C20 },
	{ GB, "VH-1 Ger", "VH-1 German",			0x4D59, 0x2C21, 0x3C21 },
	{ GB, "WSTCNTRY", "WESTCOUNTRY TV",			0x25D0, 0x2C30, 0x3C30 },
	{ GB, "WIRE TV",  "WIRE TV",				0x4403, 0x2C3C, 0x3C3C },
	{ GB, "YORKSHRE", "YORKSHIRE TV",			0xFA2C, 0x2C2D, 0x3C2D },

	{ 0,  0, 0,  0, 0, 0 }
};

/*
 *  TR 101 231 Table B.1: VPS CNI Codes used in Germany, Switzerland and Austria
 */
struct {
	int		country;
	char *		short_name;
	char *		long_name;
	unsigned int	cni;
} VPS_CNI[] = {
	{ DE, "FESTIVAL", "FESTIVAL",				0x0D41 },
	{ DE, "MUXX",	  "MUXX",				0x0D42 },
	{ DE, "EXTRA",	  "EXTRA",				0x0D43 },

	{ DE, "ONYX-TV",  "ONYX-TV",				0x0D7C },
	{ DE, "QVC",	  "QVC-Teleshopping",			0x0D7D },
	{ DE, "Nick.",	  "Nickelodeon",			0x0D7E },
	{ DE, "HOT",	  "Home Order Television",		0x0D7F },

	{ DE, "ORB-1",	  "ORB-1 Regional",			0x0D81 },
	{ DE, "ORB-3",	  "ORB-3 Landesweit",			0x0D82 },
	/* not used 0x0D83 */
	/* not used 0x0D84 */
	{ DE, "Arte",	  "Arte",				0x0D85 },
	/* not used 0x0D86 */
	{ DE, "1A-TV",	  "1A-Fernsehen",			0x0D87 },
	{ DE, "VIVA",	  "VIVA",				0x0D88 },
	{ DE, "VIVA 2",	  "VIVA 2",				0x0D89 },
	{ DE, "SuperRTL", "Super RTL",				0x0D8A },
	{ DE, "RTL Club", "RTL Club",				0x0D8B },
	{ DE, "n-tv",	  "n-tv",				0x0D8C },
	{ DE, "DSF",	  "Deutsches Sportfernsehen",		0x0D8D },
	{ DE, "VOX",	  "VOX Fernsehen",			0x0D8E },
	{ DE, "RTL 2",	  "RTL 2",				0x0D8F },
	{ DE, "RTL 2",	  "RTL 2 Regional",			0x0D90 },
	{ DE, "Eurosp.",  "Eurosport",				0x0D91 },
	{ DE, "Kabel 1",  "Kabel 1",				0x0D92 },
	/* not used 0x0D93 */
	{ DE, "PRO 7",	  "PRO 7",				0x0D94 },
	/* not in TR 101 231: 0x0D14, Pro 7 Austria? */
	{ DE, "PRO 7",	  "PRO 7",				0x0D14 },
	{ DE, "SAT 1",	  "SAT 1 Brandenburg",			0x0D95 },
	{ DE, "SAT 1",	  "SAT 1 Th�ringen",			0x0D96 },
	{ DE, "SAT 1",	  "SAT 1 Sachsen",			0x0D97 },
	{ DE, "SAT 1",	  "SAT 1 Mecklenb.-Vorpommern",		0x0D98 },
	{ DE, "SAT 1",	  "SAT 1 Sachsen-Anhalt",		0x0D99 },
	{ DE, "RTL",	  "RTL Regional",			0x0D9A },
	{ DE, "RTL",	  "RTL Schleswig-Holstein",		0x0D9B },
	{ DE, "RTL",	  "RTL Hamburg",			0x0D9C },
	{ DE, "RTL",	  "RTL Berlin",				0x0D9D },
	{ DE, "RTL",	  "RTL Niedersachsen",			0x0D9E },
	{ DE, "RTL",	  "RTL Bremen",				0x0D9F },
	{ DE, "RTL",	  "RTL Nordrhein-Westfalen",		0x0DA0 },
	{ DE, "RTL",	  "RTL Hessen",				0x0DA1 },
	{ DE, "RTL",	  "RTL Rheinland-Pfalz",		0x0DA2 },
	{ DE, "RTL",	  "RTL Baden-W�rttemberg",		0x0DA3 },
	{ DE, "RTL",	  "RTL Bayern",				0x0DA4 },
	{ DE, "RTL",	  "RTL Saarland",			0x0DA5 },
	{ DE, "RTL",	  "RTL Sachsen-Anhalt",			0x0DA6 },
	{ DE, "RTL",	  "RTL Mecklenburg-Vorpommern",		0x0DA7 },
	{ DE, "RTL",	  "RTL Sachsen",			0x0DA8 },
	{ DE, "RTL",	  "RTL Th�ringen",			0x0DA9 },
	{ DE, "RTL",	  "RTL Brandenburg",			0x0DAA },
	{ DE, "RTL Plus", "RTL Plus",				0x0DAB },
	{ DE, "Premiere", "Premiere",				0x0DAC },
	{ DE, "SAT 1",	  "SAT 1 Regional",			0x0DAD },
	{ DE, "SAT 1",	  "SAT 1 Schleswig-Holstein",		0x0DAE },
	{ DE, "SAT 1",	  "SAT 1 Hamburg",			0x0DAF },
	{ DE, "SAT 1",	  "SAT 1 Berlin",			0x0DB0 },
	{ DE, "SAT 1",	  "SAT 1 Niedersachsen",		0x0DB1 },
	{ DE, "SAT 1",	  "SAT 1 Bremen",			0x0DB2 },
	{ DE, "SAT 1",	  "SAT 1 Nordrhein-Westfalen",		0x0DB3 },
	{ DE, "SAT 1",	  "SAT 1 Hessen",			0x0DB4 },
	{ DE, "SAT 1",	  "SAT 1 Rheinland-Pfalz",		0x0DB5 },
	{ DE, "SAT 1",	  "SAT 1 Baden-W�rttemberg",		0x0DB6 },
	{ DE, "SAT 1",	  "SAT 1 Bayern",			0x0DB7 },
	{ DE, "SAT 1",	  "SAT 1 Saarland",			0x0DB8 },
	{ DE, "SAT 1",	  "SAT 1",				0x0DB9 },
	{ DE, "TM3",	  "TM3 Fernsehen",			0x0DBA },
	{ DE, "DW Bln",	  "Deutsche Welle TV Berlin",		0x0DBB },
	{ DE, "OK Bln",	  "Berlin Offener Kanal",		0x0DBD },
	{ DE, "Bln-Mix2", "Berlin-Mix-Channel 2",		0x0DBE },
	{ DE, "Bln-Mix1", "Berlin-Mix-Channel 1",		0x0DBF },

	{ DE, "ARD",	  "Erstes Deutsches Fernsehen",		0x0DC1 },
	{ DE, "ZDF",	  "Zweites Deutsches Fernsehen",	0x0DC2 },
	{ DE, "ARD/ZDF",  "ARD/ZDF Vormittagsprogramm",		0x0DC3 },
/*
 *  NOTE: As this code is used for a time in two networks a distinction
 *  for automatic tuning systems is given in data line 16: [VPS]
 *  bit 3 of byte 5 = 1 for the ARD network / = 0 for the ZDF network.
 */
	{ DE, "ARD *",	  "ARD-TV-Sternpunkt",			0x0DC4 },
	{ DE, "ARD **",	  "ARD-TV-Sternpunkt-Fehler",		0x0DC5 },
	/* not used 0x0DC6 */
	{ DE, "3sat",	  "3sat",				0x0DC7 },
	{ DE, "Phoenix",  "Phoenix",				0x0DC8 },
	{ DE, "KiKa",	  "ARD/ZDF Kinderkanal",		0x0DC9 },
	{ DE, "BR-1",	  "BR-1 Regional",			0x0DCA },
	{ DE, "BR-3",	  "BR-3 Landesweit",			0x0DCB },
	{ DE, "BR-3",	  "BR-3 S�d",				0x0DCC },
	{ DE, "BR-3",	  "BR-3 Nord",				0x0DCD },
	{ DE, "HR-1",	  "HR-1 Regional",			0x0DCE },
	{ DE, "HR-3",	  "Hessen 3 Landesweit",		0x0DCF },
	{ DE, "NDR-1",	  "NDR-1 Dreil�nderweit",		0x0DD0 },
	{ DE, "NDR-1",	  "NDR-1 Hamburg",			0x0DD1 },
	{ DE, "NDR-1",	  "NDR-1 Niedersachsen",		0x0DD2 },
	{ DE, "NDR-1",	  "NDR-1 Schleswig-Holstein",		0x0DD3 },
	{ DE, "N3",	  "Nord-3 (NDR/SFB/RB)",		0x0DD4 },
	{ DE, "NDR-3",	  "NDR-3 Dreil�nderweit",		0x0DD5 },
	{ DE, "NDR-3",	  "NDR-3 Hamburg",			0x0DD6 },
	{ DE, "NDR-3",	  "NDR-3 Niedersachsen",		0x0DD7 },
	{ DE, "NDR-3",	  "NDR-3 Schleswig-Holstein",		0x0DD8 },
	{ DE, "RB-1",	  "RB-1 Regional",			0x0DD9 },
	{ DE, "RB-3",	  "RB-3",				0x0DDA },
	{ DE, "SFB-1",	  "SFB-1 Regional",			0x0DDB },
	{ DE, "SFB-3",	  "SFB-3",				0x0DDC },
	{ DE, "SDR/SWF",  "SDR/SWF Baden-W�rttemb.",		0x0DDD },
	{ DE, "SWF-1",	  "SWF-1 Rheinland-Pfalz",		0x0DDE },
	{ DE, "SR-1",	  "SR-1 Regional",			0x0DDF },
	{ DE, "SW 3",	  "S�dwest 3 (SDR/SR/SWF)",		0x0DE0 },
	{ DE, "SW 3",	  "SW 3 Baden-W�rttemberg",		0x0DE1 },
	{ DE, "SW 3",	  "SW 3 Saarland",			0x0DE2 },
	{ DE, "SW 3",	  "SW 3 Baden-W�rttemb. S�d",		0x0DE3 },
	{ DE, "SW 3",	  "SW 3 Rheinland-Pfalz",		0x0DE4 },
	{ DE, "WDR-1",	  "WDR-1 Regionalprogramm",		0x0DE5 },
	{ DE, "WDR-3",	  "WDR-3 Landesweit",			0x0DE6 },
	{ DE, "WDR-3",	  "WDR-3 Bielefeld",			0x0DE7 },
	{ DE, "WDR-3",	  "WDR-3 Dortmund",			0x0DE8 },
	{ DE, "WDR-3",	  "WDR-3 D�sseldorf",			0x0DE9 },
	{ DE, "WDR-3",	  "WDR-3 K�ln",				0x0DEA },
	{ DE, "WDR-3",	  "WDR-3 M�nster",			0x0DEB },
	{ DE, "SDR-1",	  "SDR-1 Regional",			0x0DEC },
	{ DE, "SW 3",	  "SW 3 Baden-W�rttemb. Nord",		0x0DED },
	{ DE, "SW 3",	  "SW 3 Mannheim",			0x0DEE },
	{ DE, "SDR/SWF",  "SDR/SWF BW und Rhld-Pfalz",		0x0DEF },
	{ DE, "SWF-1",	  "SWF-1 / Regionalprogramm",		0x0DF0 },
	{ DE, "NDR-1",	  "NDR-1 Mecklenb.-Vorpommern",		0x0DF1 },
	{ DE, "NDR-3",	  "NDR-3 Mecklenb.-Vorpommern",		0x0DF2 },
	{ DE, "MDR-1",	  "MDR-1 Sachsen",			0x0DF3 },
	{ DE, "MDR-3",	  "MDR-3 Sachsen",			0x0DF4 },
	{ DE, "MDR",	  "MDR Dresden",			0x0DF5 },
	{ DE, "MDR-1",	  "MDR-1 Sachsen-Anhalt",		0x0DF6 },
	{ DE, "WDR",	  "WDR Dortmund",			0x0DF7 },
	{ DE, "MDR-3",	  "MDR-3 Sachsen-Anhalt",		0x0DF8 },
	{ DE, "MDR",	  "MDR Magdeburg",			0x0DF9 },
	{ DE, "MDR-1",	  "MDR-1 Th�ringen",			0x0DFA },
	{ DE, "MDR-3",	  "MDR-3 Th�ringen",			0x0DFB },
	{ DE, "MDR",	  "MDR Erfurt",				0x0DFC },
	{ DE, "MDR-1",	  "MDR-1 Regional",			0x0DFD },
	{ DE, "MDR-3",	  "MDR-3 Landesweit",			0x0DFE },

	{ CH, "TeleZ�ri", "TeleZ�ri",				0x0481 },
	{ CH, "Teleclub", "Teleclub Abo-Fernsehen",		0x0482 },
	{ CH, "Z�rich 1", "Z�rich 1",				0x0483 },
	{ CH, "TeleBern", "TeleBern",				0x0484 },
	{ CH, "Tele M1",  "Tele M1",				0x0485 },
	{ CH, "Star TV",  "Star TV",				0x0486 },
	{ CH, "Pro 7",	  "Pro 7",				0x0487 },
	{ CH, "TopTV",	  "TopTV",				0x0488 },

	{ CH, "SF 1",	  "SRG Schweizer Fernsehen SF 1",	0x04C1 },
	{ CH, "TSR 1",	  "SSR T�l�vis. Suisse TSR 1",		0x04C2 },
	{ CH, "TSI 1",	  "SSR Televis. svizzera TSI 1",	0x04C3 },
	/* not used 0x04C4 */
	/* not used 0x04C5 */
	/* not used 0x04C6 */
	{ CH, "SF 2",	  "SRG Schweizer Fernsehen SF 2",	0x04C7 },
	{ CH, "TSR 2",	  "SSR T�l�vis. Suisse TSR 2",		0x04C8 },
	{ CH, "TSI 2",	  "SSR Televis. svizzera TSI 2",	0x04C9 },
	{ CH, "SRG-SAT",  "SRG SSR Sat Access",			0x04CA },

	{ AT, "ORF 1",	  "ORF 1",				0x0AC1 },
	{ AT, "ORF 2",	  "ORF 2",				0x0AC2 },
	{ AT, "ORF 3",	  "ORF 3",				0x0AC3 },
	{ AT, "ORF B",	  "ORF Burgenland",			0x0ACB },
	{ AT, "ORF K",	  "ORF K�rnten",			0x0ACC },
	{ AT, "ORF N�",	  "ORF Nieder�sterreich",		0x0ACD },
	{ AT, "ORF O�",	  "ORF Ober�sterreich",			0x0ACE },
	{ AT, "ORF S",	  "ORF Salzburg",			0x0ACF },
	{ AT, "ORF St",	  "ORF Steiermark",			0x0AD0 },
	{ AT, "ORF T",	  "ORF Tirol",				0x0AD1 },
	{ AT, "ORF V",	  "ORF Vorarlberg",			0x0AD2 },
	{ AT, "ORF W",	  "ORF Wien",				0x0AD3 },
	/* not in TR 101 231: 0x0ADE */
	{ AT, "ATV",	  "ATV",				0x0ADE },

	{ 0,  0, 0,  0 }
};

/*
 *  ETS 300 231 Table 3: Codes for programme type (PTY) Principle of classification
 */
char *
program_class[16] =
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

/*
 *  ETS 300 231 Table 3: Codes for programme type (PTY) Principle of classification
 */
char *
program_type[8][16] =
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
