
#include "site_def.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "vbi.h"
#include "hamm.h"
#include "lang.h"
#include "export.h"
#include "tables.h"
#include "libvbi.h"
#include "trigger.h"

#define printable(c) ((((c) & 0x7F) < 0x20 || ((c) & 0x7F) > 0x7E) ? '.' : ((c) & 0x7F))

static bool convert_drcs(struct vt_page *vtp, unsigned char *raw);

static inline void
dump_pagenum(pagenum page)
{
	printf("T%x %3x/%04x\n", page.type, page.pgno, page.subno);
}

static void
dump_raw(struct vt_page *vtp, bool unham)
{
	int i, j;

	printf("Page %03x.%04x\n", vtp->pgno, vtp->subno);

	for (j = 0; j < 25; j++) {
		if (unham)
			for (i = 0; i < 40; i++)
				printf("%01x ", hamm8(vtp->data.lop.raw[j][i]) & 0xF);
		else
			for (i = 0; i < 40; i++)
				printf("%02x ", vtp->data.lop.raw[j][i]);
		for (i = 0; i < 40; i++)
			putchar(printable(vtp->data.lop.raw[j][i]));
		putchar('\n');
	}
}

static void
dump_extension(extension *ext)
{
	int i;

	printf("Extension:\ndesignations %08x\n", ext->designations);
	printf("char set primary %d secondary %d\n", ext->char_set[0], ext->char_set[1]);
	printf("default screen col %d row col %d\n", ext->def_screen_colour, ext->def_row_colour);
	printf("bbg subst %d colour table remapping %d, %d\n",
		ext->fallback.black_bg_substitution, ext->foreground_clut, ext->background_clut);
	printf("panel left %d right %d left columns %d\n",
		ext->fallback.left_side_panel, ext->fallback.right_side_panel,
		ext->fallback.left_panel_columns);
	printf("colour map (bgr):\n");
	for (i = 0; i < 40; i++) {
		printf("%08x, ", ext->colour_map[i]);
		if ((i % 8) == 7) printf("\n");
	}
	printf("dclut4 global: ");
	for (i = 0; i <= 3; i++)
		printf("%2d ", ext->drcs_clut[i + 2]);
	printf("\ndclut4 normal: ");
	for (i = 0; i <= 3; i++)
		printf("%2d ", ext->drcs_clut[i + 6]);
	printf("\ndclut16 global: ");
	for (i = 0; i <= 15; i++)
		printf("%2d ", ext->drcs_clut[i + 10]);
	printf("\ndclut16 normal: ");
	for (i = 0; i <= 15; i++)
		printf("%2d ", ext->drcs_clut[i + 26]);
	printf("\n\n");
}

static void
dump_drcs(struct vt_page *vtp)
{
	int i, j, k;
	unsigned char *p = vtp->data.drcs.bits[0];

	printf("\nDRCS page %03x/%04x\n", vtp->pgno, vtp->subno);

	for (i = 0; i < 48; i++) {
		printf("DRC #%d mode %02x\n", i, vtp->data.drcs.mode[i]);

		for (j = 0; j < 10; p += 6, j++) {
			for (k = 0; k < 6; k++)
				printf("%x%x", p[k] & 15, p[k] >> 4);
			putchar('\n');
		}
	}
}

static void
dump_page_info(struct teletext *vt)
{
	int i, j;

	for (i = 0; i < 0x800; i += 16) {
		printf("%03x: ", i + 0x100);

		for (j = 0; j < 16; j++)
			printf("%02x:%02x:%04x ",
			       vt->page_info[i + j].code & 0xFF,
			       vt->page_info[i + j].language & 0xFF, 
			       vt->page_info[i + j].subcode & 0xFFFF);

		putchar('\n');
	}

	putchar('\n');
}

static inline bool
hamm8_page_number(pagenum *p, unsigned char *raw, int magazine)
{
	int b1, b2, b3, err, m;

	err = b1 = hamm16(raw + 0);
	err |= b2 = hamm16(raw + 2);
	err |= b3 = hamm16(raw + 4);

	if (err < 0)
		return FALSE;

	m = ((b3 >> 5) & 6) + (b2 >> 7);

	p->pgno = ((magazine ^ m) ? : 8) * 256 + b1;
	p->subno = (b3 * 256 + b2) & 0x3f7f;

	return TRUE;
}

static inline bool
parse_mot(magazine *mag, unsigned char *raw, int packet)
{
	int err, i, j;

	switch (packet) {
	case 1 ... 8:
	{
		int index = (packet - 1) << 5;
		char n0, n1;

		for (i = 0; i < 20; index++, i++) {
			if (i == 10)
				index += 6;

			n0 = hamm8(*raw++);
			n1 = hamm8(*raw++);

			if ((n0 | n1) < 0)
				continue;

			mag->pop_lut[index] = n0 & 7;
			mag->drcs_lut[index] = n1 & 7;
		}

		return TRUE;
	}

	case 9 ... 14:
	{
		int index = (packet - 9) * 0x30 + 10;

		for (i = 0; i < 20; index++, i++) {
			char n0, n1;

			if (i == 6 || i == 12) {
				if (index == 0x100)
					break;
				else
					index += 10;
			}

			n0 = hamm8(*raw++);
			n1 = hamm8(*raw++);

			if ((n0 | n1) < 0)
				continue;

			mag->pop_lut[index] = n0 & 7;
			mag->drcs_lut[index] = n1 & 7;
		}

		return TRUE;
	}

	case 15 ... 18: /* not used */
		return TRUE;

	case 22 ... 23:	/* level 3.5 pops */
		packet--;

	case 19 ... 20: /* level 2.5 pops */
	{
		pop_link *pop = mag->pop_link + (packet - 19) * 4;

		for (i = 0; i < 4; raw += 10, pop++, i++) {
			char n[10];

			for (err = j = 0; j < 10; j++)
				err |= n[j] = hamm8(raw[j]);

			if (err < 0) /* XXX unused bytes poss. not hammed (^ N3) */
				continue;

			pop->pgno = (((n[0] & 7) ? : 8) << 8) + (n[1] << 4) + n[2];

			/* n[3] number of subpages ignored */

			if (n[4] & 1)
				memset(&pop->fallback, 0, sizeof(pop->fallback));
			else {
				int x = (n[4] >> 1) & 3;

				pop->fallback.black_bg_substitution = n[4] >> 3;
				pop->fallback.left_side_panel = x & 1;
				pop->fallback.right_side_panel = x >> 1;
				pop->fallback.left_panel_columns = "\00\20\20\10"[x];
			}

			pop->default_obj[0].type = n[5] & 3;
			pop->default_obj[0].address = (n[7] << 4) + n[6];
			pop->default_obj[1].type = n[5] >> 2;
			pop->default_obj[1].address = (n[9] << 4) + n[8];
		}

		return TRUE;
	}

	case 21:	/* level 2.5 drcs */
	case 24:	/* level 3.5 drcs */
	    {
		int index = (packet == 21) ? 0 : 8;
		char n[4];

		for (i = 0; i < 8; raw += 4, index++, i++) {
			for (err = j = 0; j < 4; j++)
				err |= n[j] = hamm8(raw[j]);

			if (err < 0)
				continue;

			mag->drcs_link[index] = (((n[0] & 7) ? : 8) << 8) + (n[1] << 4) + n[2];

			/* n[3] number of subpages ignored */
		}

		return TRUE;
	    }
	}

	return TRUE;
}

static bool
parse_pop(struct vt_page *vtp, unsigned char *raw, int packet)
{
	int designation, triplet[13];
	vt_triplet *trip;
	int i;

	if ((designation = hamm8(raw[0])) < 0)
		return FALSE;

	for (raw++, i = 0; i < 13; raw += 3, i++)
		triplet[i] = hamm24(raw);

	if (packet == 26)
		packet += designation;

	switch (packet) {
	case 1 ... 2:
		if (!(designation & 1))
			return FALSE; /* fixed usage */

	case 3 ... 4:
		if (designation & 1) {
			int index = (packet - 1) * 26;

			for (index += 2, i = 1; i < 13; index += 2, i++)
				if (triplet[i] >= 0) {
					vtp->data.pop.pointer[index + 0] = triplet[i] & 0x1FF;
					vtp->data.pop.pointer[index + 1] = triplet[i] >> 9;
				}

			return TRUE;
		}

		/* fall through */

	case 5 ... 42:
		trip = vtp->data.pop.triplet + (packet - 3) * 13;

		for (i = 0; i < 13; trip++, i++)
			if (triplet[i] >= 0) {
				trip->address	= (triplet[i] >> 0) & 0x3F;
				trip->mode	= (triplet[i] >> 6) & 0x1F;
				trip->data	= (triplet[i] >> 11);
			}

		return TRUE;
	}

	return FALSE;
}

static unsigned int expand[64];

static void init_expand(void) __attribute__ ((constructor));

static void
init_expand(void)
{
	int i, j, n;

	for (i = 0; i < 64; i++) {
		for (n = j = 0; j < 6; j++)
			if (i & (0x20 >> j))
				n |= 1 << (j * 4);
		expand[i] = n;
	}
}

static bool
convert_drcs(struct vt_page *vtp, unsigned char *raw)
{
	unsigned char *p, *d;
	int i, j, q;

	p = raw;
	vtp->data.drcs.invalid = 0;

	for (i = 0; i < 24; p += 40, i++)
		if (vtp->lop_lines & (2 << i)) {
			for (j = 0; j < 20; j++)
				if (parity(p[j]) < 0x40) {
					vtp->data.drcs.invalid |= 1ULL << (i * 2);
					break;
				}
			for (j = 20; j < 40; j++)
				if (parity(p[j]) < 0x40) {
					vtp->data.drcs.invalid |= 1ULL << (i * 2 + 1);
					break;
				}
		} else {
			vtp->data.drcs.invalid |= 3ULL << (i * 2);
		}

	p = raw;
	d = vtp->data.drcs.bits[0];

	for (i = 0; i < 48; i++) {
		switch (vtp->data.drcs.mode[i]) {
		case DRCS_MODE_12_10_1:
			for (j = 0; j < 20; d += 3, j++) {
				d[0] = q = expand[p[j] & 0x3F];
				d[1] = q >> 8;
				d[2] = q >> 16;
			}
			p += 20;
			break;

		case DRCS_MODE_12_10_2:
			if (vtp->data.drcs.invalid & (3ULL << i)) {
				vtp->data.drcs.invalid |= (3ULL << i);
				d += 60;
			} else
				for (j = 0; j < 20; d += 3, j++) {
					q = expand[p[j +  0] & 0x3F]
					  + expand[p[j + 20] & 0x3F] * 2;
					d[0] = q;
					d[1] = q >> 8;
					d[2] = q >> 16;
				}
			p += 40;
			d += 60;
			i += 1;
			break;

		case DRCS_MODE_12_10_4:
			if (vtp->data.drcs.invalid & (15ULL << i)) {
				vtp->data.drcs.invalid |= (15ULL << i);
				d += 60;
			} else
				for (j = 0; j < 20; d += 3, j++) {
					q = expand[p[j +  0] & 0x3F]
					  + expand[p[j + 20] & 0x3F] * 2
					  + expand[p[j + 40] & 0x3F] * 4
					  + expand[p[j + 60] & 0x3F] * 8;
					d[0] = q;
					d[1] = q >> 8;
					d[2] = q >> 16;
				}
			p += 80;
			d += 180;
			i += 3;
			break;

		case DRCS_MODE_6_5_4:
			for (j = 0; j < 20; p += 4, d += 6, j++) {
				q = expand[p[0] & 0x3F]
				  + expand[p[1] & 0x3F] * 2
				  + expand[p[2] & 0x3F] * 4
				  + expand[p[3] & 0x3F] * 8;
				d[0] = (q & 15) * 0x11;
				d[1] = ((q >> 4) & 15) * 0x11;
				d[2] = ((q >> 8) & 15) * 0x11;
				d[3] = ((q >> 12) & 15) * 0x11;
				d[4] = ((q >> 16) & 15) * 0x11;
				d[5] = (q >> 20) * 0x11;
			}
			break;

		default:
			vtp->data.drcs.invalid |= (1ULL << i);
			p += 20;
			d += 60;
			break;
		}
	}

	if (0)
		dump_drcs(vtp);

	return TRUE;
}

static int
page_language(struct teletext *vt, struct vt_page *vtp, int pgno, int national)
{
	magazine *mag;
	extension *ext;
	int char_set;
	int lang = -1; /***/

	if (vtp) {
		if (vtp->function != PAGE_FUNCTION_LOP)
			return lang;

		pgno = vtp->pgno;
		national = vtp->national;
	}

	mag = (vt->max_level <= VBI_LEVEL_1p5) ?
		vt->magazine : vt->magazine + (pgno >> 8);

	ext = (vtp && vtp->data.lop.ext) ?
		&vtp->data.ext_lop.ext : &mag->extension;

	char_set = ext->char_set[0];

	if (VALID_CHARACTER_SET(char_set))
		lang = char_set;

	char_set = (char_set & ~7) + national;

	if (VALID_CHARACTER_SET(char_set))
		lang = char_set;

	return lang;
}

static bool
parse_mip_page(struct vbi *vbi, struct vt_page *vtp,
	int pgno, int code, int *subp_index)
{
	unsigned char *raw;
	int subc, old_code, old_subc;

	if (code < 0)
		return FALSE;

	switch (code) {
	case 0x52 ... 0x6F: /* reserved */
	case 0xD2 ... 0xDF: /* reserved */
	case 0xFA ... 0xFC: /* reserved */
	case 0xFF: 	    /* reserved, we use it as 'unknown' flag */
		return TRUE;

	case 0x02 ... 0x4F:
	case 0x82 ... 0xCF:
		subc = code & 0x7F;
		code = (code >= 0x80) ? VBI_PROGR_SCHEDULE :
					VBI_NORMAL_PAGE;
		break;

	case 0x70 ... 0x77:
		code = VBI_SUBTITLE_PAGE;
		subc = 0;
		vbi->vt.page_info[pgno - 0x100].language =
			page_language(&vbi->vt,
				vbi_cache_get(vbi, pgno, 0, 0),
				pgno, code & 7);
		break;

	case 0x50 ... 0x51: /* normal */
	case 0xD0 ... 0xD1: /* program */
	case 0xE0 ... 0xE1: /* data */
	case 0x7B: /* current program */
	case 0xF8: /* keyword search list */
		if (*subp_index > 10 * 13)
			return FALSE;

		raw = &vtp->data.unknown.raw[*subp_index / 13 + 15]
				    [(*subp_index % 13) * 3 + 1];
		(*subp_index)++;

		if ((subc = hamm16(raw) | (hamm8(raw[2]) << 8)) < 0)
			return FALSE;

		if ((code & 15) == 1)
			subc += 1 << 12;
		else if (subc < 2)
			return FALSE;

		code =	(code == 0xF8) ? VBI_KEYWORD_SEARCH_LIST :
			(code == 0x7B) ? VBI_CURRENT_PROGR :
			(code >= 0xE0) ? VBI_CA_DATA_BROADCAST :
			(code >= 0xD0) ? VBI_PROGR_SCHEDULE :
					 VBI_NORMAL_PAGE;
		break;

	default:
		code = code;
		subc = 0;
		break;
	}

	old_code = vbi->vt.page_info[pgno - 0x100].code;
	old_subc = vbi->vt.page_info[pgno - 0x100].subcode;

	/*
	 *  When we got incorrect numbers and proved otherwise by
	 *  actually receiving the page...
	 */
	if (old_code == VBI_UNKNOWN_PAGE || old_code == VBI_SUBTITLE_PAGE
	    || code != VBI_NO_PAGE || code == VBI_SUBTITLE_PAGE)
		vbi->vt.page_info[pgno - 0x100].code = code;

	if (old_code == VBI_UNKNOWN_PAGE || subc > old_subc)
		vbi->vt.page_info[pgno - 0x100].subcode = subc;

	return TRUE;
}

static bool
parse_mip(struct vbi *vbi, struct vt_page *vtp)
{
	int packet, pgno, i, spi = 0;

	if (0)
		dump_raw(vtp, TRUE);

	for (packet = 1, pgno = vtp->pgno & 0xF00; packet <= 8; packet++, pgno += 0x20)
		if (vtp->lop_lines & (1 << packet)) {
			unsigned char *raw = vtp->data.unknown.raw[packet];

			for (i = 0x00; i <= 0x09; raw += 2, i++)
				if (!parse_mip_page(vbi, vtp, pgno + i, hamm16(raw), &spi))
					return FALSE;
			for (i = 0x10; i <= 0x19; raw += 2, i++)
				if (!parse_mip_page(vbi, vtp, pgno + i, hamm16(raw), &spi))
					return FALSE;
		}

	for (packet = 9, pgno = vtp->pgno & 0xF00; packet <= 14; packet++, pgno += 0x30)
		if (vtp->lop_lines & (1 << packet)) {
			unsigned char *raw = vtp->data.unknown.raw[packet];

			for (i = 0x0A; i <= 0x0F; raw += 2, i++)
				if (!parse_mip_page(vbi, vtp, pgno + i,
						    hamm16(raw), &spi))
					return FALSE;
			if (packet == 14) /* 0xFA ... 0xFF */
				break;
			for (i = 0x1A; i <= 0x1F; raw += 2, i++)
				if (!parse_mip_page(vbi, vtp, pgno + i,
						    hamm16(raw), &spi))
					return FALSE;
			for (i = 0x2A; i <= 0x2F; raw += 2, i++)
				if (!parse_mip_page(vbi, vtp, pgno + i,
						    hamm16(raw), &spi))
					return FALSE;
		}

	if (0 && packet == 1)
		dump_page_info(&vbi->vt);

	return TRUE;
}

static void
eacem_trigger(struct vbi *vbi, struct vt_page *vtp)
{
	struct fmt_page pg;
	unsigned char *s;
	int i, j;

#ifdef LIBVBI_TRIGGER_VERBOSE
	dump_raw(vtp, FALSE);
#endif

	if (!(vbi->event_mask & VBI_EVENT_TRIGGER))
		return;

	if (!vbi_format_page(vbi, &pg, vtp, VBI_LEVEL_1p5, 24, 0))
		return;

	s = (unsigned char *) pg.text;

	for (i = 1; i < 25; i++)
		for (j = 0; j < 40; j++)
			*s++ = glyph2latin(pg.text[i * 41 + j].glyph);
	*s = 0;

	vbi_eacem_trigger(vbi, (unsigned char *) pg.text);
}

/*
 *  Table Of Pages navigation
 */

static const int dec2bcdp[20] = {
	0x000, 0x040, 0x080, 0x120, 0x160, 0x200, 0x240, 0x280, 0x320, 0x360,
	0x400, 0x440, 0x480, 0x520, 0x560, 0x600, 0x640, 0x680, 0x720, 0x760
};

static bool
top_page_number(pagenum *p, unsigned char *raw)
{
	char n[8];
	int pgno, err, i;

	for (err = i = 0; i < 8; i++)
		err |= n[i] = hamm8(raw[i]);

	pgno = n[0] * 256 + n[1] * 16 + n[2];

	if (err < 0 || pgno > 0x8FF)
		return FALSE;

	p->pgno = pgno;
	p->subno = ((n[3] << 12) | (n[4] << 8) | (n[5] << 4) | n[6]) & 0x3f7f; // ?
	p->type = n[7]; // ?

	return TRUE;
}

static inline bool
parse_btt(struct vbi *vbi, unsigned char *raw, int packet)
{
	struct vt_page *vtp;

	switch (packet) {
	case 1 ... 20:
	{
		int i, j, code, index = dec2bcdp[packet - 1];

		for (i = 0; i < 4; i++) {
			for (j = 0; j < 10; index++, j++) {
				struct page_info *pi = vbi->vt.page_info + index;

				if ((code = hamm8(*raw++)) < 0)
					break;

				switch (code) {
				case BTT_SUBTITLE:
					pi->code = VBI_SUBTITLE_PAGE;
					if ((vtp = vbi_cache_get(vbi, index + 0x100, 0, 0)))
						pi->language = page_language(&vbi->vt, vtp, 0, 0);
					break;

				case BTT_PROGR_INDEX_S:
				case BTT_PROGR_INDEX_M:
					/* Usually schedule, not index (likely BTT_GROUP) */
					pi->code = VBI_PROGR_SCHEDULE;
					break;

				case BTT_BLOCK_S:
				case BTT_BLOCK_M:
					pi->code = VBI_TOP_BLOCK;
					break;

				case BTT_GROUP_S:
				case BTT_GROUP_M:
					pi->code = VBI_TOP_GROUP;
					break;

				case 8 ... 11:
					pi->code = VBI_NORMAL_PAGE;
					break;

				default:
					pi->code = VBI_NO_PAGE;
					continue;
				}

				switch (code) {
				case BTT_PROGR_INDEX_M:
				case BTT_BLOCK_M:
				case BTT_GROUP_M:
				case BTT_NORMAL_M:
					/* -> mpt, mpt_ex */
					break;

				default:
					pi->subcode = 0;
					break;
				}
			}

			index += ((index & 0xFF) == 0x9A) ? 0x66 : 0x06;
		}

		break;
	}

	case 21 ... 23:
	    {
		pagenum *p = vbi->vt.btt_link + (packet - 21) * 5;
		int i;

		vbi->vt.top = TRUE;

		for (i = 0; i < 5; raw += 8, p++, i++) {
			if (!top_page_number(p, raw))
				continue;

			if (0) {
				printf("BTT #%d: ", (packet - 21) * 5);
				dump_pagenum(*p);
			}

			switch (p->type) {
			case 1: /* MPT */
			case 2: /* AIT */
			case 3: /* MPT-EX */
				vbi->vt.page_info[p->pgno - 0x100].code = VBI_TOP_PAGE;
				vbi->vt.page_info[p->pgno - 0x100].subcode = 0;
				break;
			}
		}

		break;
	    }
	}

	if (0 && packet == 1)
		dump_page_info(&vbi->vt);

	return TRUE;
}

static bool
parse_ait(struct vt_page *vtp, unsigned char *raw, int packet)
{
	int i, n;
	ait_entry *ait;

	if (packet < 1 || packet > 23)
		return TRUE;

	ait = vtp->data.ait + (packet - 1) * 2;

	if (top_page_number(&ait[0].page, raw + 0)) {
		for (i = 0; i < 12; i++)
			if ((n = parity(raw[i + 8])) >= 0)
				ait[0].text[i] = n;
	}

	if (top_page_number(&ait[1].page, raw + 20)) {
		for (i = 0; i < 12; i++)
			if ((n = parity(raw[i + 28])) >= 0)
				ait[1].text[i] = n;
	}

	return TRUE;
}

static inline bool
parse_mpt(struct teletext *vt, unsigned char *raw, int packet)
{
	int i, j, index;
	int n;

	switch (packet) {
	case 1 ... 20:
		index = dec2bcdp[packet - 1];

		for (i = 0; i < 4; i++) {
			for (j = 0; j < 10; index++, j++)
				if ((n = hamm8(*raw++)) >= 0) {
					int code = vt->page_info[index].code;
					int subc = vt->page_info[index].subcode;

					if (n > 9)
						n = 0xFFFEL; /* mpt_ex? not transm?? */

					if (code != VBI_NO_PAGE && code != VBI_UNKNOWN_PAGE
					    && (subc >= 0xFFFF || n > subc))
						vt->page_info[index].subcode = n;
				}

			index += ((index & 0xFF) == 0x9A) ? 0x66 : 0x06;
		}
	}

	return TRUE;
}

static inline bool
parse_mpt_ex(struct teletext *vt, unsigned char *raw, int packet)
{
	int i, code, subc;
	pagenum p;

	switch (packet) {
	case 1 ... 23:
		for (i = 0; i < 5; raw += 8, i++) {
			if (!top_page_number(&p, raw))
				continue;

			if (0) {
				printf("MPT-EX #%d: ", (packet - 1) * 5);
				dump_pagenum(p);
			}

			if (p.pgno < 0x100)
				break;
			else if (p.pgno > 0x8FF || p.subno < 1)
				continue;

			code = vt->page_info[p.pgno - 0x100].code;
			subc = vt->page_info[p.pgno - 0x100].subcode;

			if (code != VBI_NO_PAGE && code != VBI_UNKNOWN_PAGE
			    && (p.subno > subc /* evidence */
				/* || subc >= 0xFFFF unknown */
				|| subc >= 0xFFFE /* mpt > 9 */))
				vt->page_info[p.pgno - 0x100].subcode = p.subno;
		}

		break;
	}

	return TRUE;
}

/*
 *  Since MOT, MIP and X/28 are optional, the function of a system page
 *  may not be clear until we format a LOP and find a link of certain type.
 */
struct vt_page *
vbi_convert_page(struct vbi *vbi, struct vt_page *vtp, bool cached, page_function new_function)
{
	struct vt_page page;
	int i;

	if (vtp->function != PAGE_FUNCTION_UNKNOWN)
		return NULL;

	memcpy(&page, vtp, sizeof(*vtp)	- sizeof(vtp->data) + sizeof(vtp->data.unknown));

	switch (new_function) {
	case PAGE_FUNCTION_LOP:
		vtp->function = new_function;
		return vtp;

	case PAGE_FUNCTION_GPOP:
	case PAGE_FUNCTION_POP:
		memset(page.data.pop.pointer, 0xFF, sizeof(page.data.pop.pointer));
		memset(page.data.pop.triplet, 0xFF, sizeof(page.data.pop.triplet));

		for (i = 1; i <= 25; i++)
			if (vtp->lop_lines & (1 << i))
				if (!parse_pop(&page, vtp->data.unknown.raw[i], i))
					return FALSE;

		if (vtp->enh_lines)
			memcpy(&page.data.pop.triplet[23 * 13],	vtp->data.enh_lop.enh,
				16 * 13 * sizeof(vt_triplet));
		break;

	case PAGE_FUNCTION_GDRCS:
	case PAGE_FUNCTION_DRCS:
		memmove(page.data.drcs.raw, vtp->data.unknown.raw, sizeof(page.data.drcs.raw));
		memset(page.data.drcs.mode, 0, sizeof(page.data.drcs.mode));
		page.lop_lines = vtp->lop_lines;

		if (!convert_drcs(&page, vtp->data.unknown.raw[1]))
			return FALSE;

		break;

	case PAGE_FUNCTION_AIT:
		memset(page.data.ait, 0, sizeof(page.data.ait));

		for (i = 1; i <= 23; i++)
			if (vtp->lop_lines & (1 << i))
				if (!parse_ait(&page, vtp->data.unknown.raw[i], i))
					return FALSE;
		break;

	case PAGE_FUNCTION_MPT:
		for (i = 1; i <= 20; i++)
			if (vtp->lop_lines & (1 << i))
				if (!parse_mpt(&vbi->vt, vtp->data.unknown.raw[i], i))
					return FALSE;
		break;

	case PAGE_FUNCTION_MPT_EX:
		for (i = 1; i <= 20; i++)
			if (vtp->lop_lines & (1 << i))
				if (!parse_mpt_ex(&vbi->vt, vtp->data.unknown.raw[i], i))
					return FALSE;
		break;

	default:
		return NULL;
	}

	page.function = new_function;

	if (cached) {
		return vbi_cache_put(vbi, &page);
	} else {
		memcpy(vtp, &page, vtp_size(&page));
		return vtp;
	}
}

typedef enum {
	CNI_NONE,
	CNI_VPS,	/* VPS format */
	CNI_8301,	/* Teletext packet 8/30 format 1 */
	CNI_8302,	/* Teletext packet 8/30 format 2 */
	CNI_X26		/* Teletext packet X/26 local enhancement */
} vbi_cni_type;

static unsigned int
station_lookup(vbi_cni_type type, int cni,
	char **country, char **short_name, char **long_name)
{
	struct pdc_vps_entry *p;

	if (!cni)
		return 0;

	switch (type) {
	case CNI_8301:
		for (p = PDC_VPS_CNI; p->short_name; p++)
			if (p->cni1 == cni) {
				*country = country_names_en[p->country];
				*short_name = p->short_name;
				*long_name = p->long_name;
				return p->id;
			}
		break;

	case CNI_8302:
		for (p = PDC_VPS_CNI; p->short_name; p++)
			if (p->cni2 == cni) {
				*country = country_names_en[p->country];
				*short_name = p->short_name;
				*long_name = p->long_name;
				return p->id;
			}

		cni &= 0x0FFF;

		/* fall through */

	case CNI_VPS:
		/* if (cni == 0x0DC3) in decoder
			cni = mark ? 0x0DC2 : 0x0DC1; */

		for (p = PDC_VPS_CNI; p->short_name; p++)
			if (p->cni4 == cni) {
				*country = country_names_en[p->country];
				*short_name = p->short_name;
				*long_name = p->long_name;
				return p->id;
			}
		break;

	case CNI_X26:
		for (p = PDC_VPS_CNI; p->short_name; p++)
			if (p->cni3 == cni) {
				*country = country_names_en[p->country];
				*short_name = p->short_name;
				*long_name = p->long_name;
				return p->id;
			}

		/* try code | 0x0080 & 0x0FFF -> VPS ? */

		break;

	default:
		break;
	}

	return 0;
}

static void
unknown_cni(struct vbi *vbi, char *dl, int cni)
{
	/* if (cni == 0) */
		return;

	fprintf(stderr,
"This network broadcasts an unknown CNI of 0x%04x using a %s data line.\n"
"If you see this message always when switching to this channel please\n"
"report network name, country, CNI and data line at http://zapping.sf.net\n"
"for inclusion in the Country and Network Identifier table. Thank you.\n",
		cni, dl);
}

#define BSDATA_TEST 0 /* Broadcaster Service Data */

#if BSDATA_TEST

static const char *month_names[] = {
	"0?", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug",
	"Sep", "Oct", "Nov", "Dec", "13?", "14?", "15?"
};

static const char *pcs_names[] = {
	"unknown", "mono", "stereo", "bilingual"
};

#define PIL(day, mon, hour, min) \
	(((day) << 15) + ((mon) << 11) + ((hour) << 6) + ((min) << 0))

static void
dump_pil(int pil)
{
	int day, mon, hour, min;

	day = pil >> 15;
	mon = (pil >> 11) & 0xF;
	hour = (pil >> 6) & 0x1F;
	min = pil & 0x3F;

	if (pil == PIL(0, 15, 31, 63))
		printf("... PDC: Timer-control (no PDC)\n");
	else if (pil == PIL(0, 15, 30, 63))
		printf("... PDC: Recording inhibit/terminate\n");
	else if (pil == PIL(0, 15, 29, 63))
		printf("... PDC: Interruption\n");
	else if (pil == PIL(0, 15, 28, 63))
		printf("... PDC: Continue\n");
	else if (pil == PIL(31, 15, 31, 63))
		printf("... PDC: No time\n");
	else
		printf("... PDC: %05x, %2d %s %02d:%02d\n",
			pil, day, month_names[mon], hour, min);
}

static void
dump_pty(int pty)
{
	if (pty == 0xFF)
		printf("... prog. type: %02x unused", pty);
	else
		printf("... prog. type: %02x class %s", pty, program_class[pty >> 4]);

	if (pty < 0x80) {
		if (program_type[pty >> 4][pty & 0xF])
			printf(", type %s", program_type[pty >> 4][pty & 0xF]);
		else
			printf(", type undefined");
	}

	putchar('\n');
}

#endif /* BSDATA_TEST */

void
vbi_vps(struct vbi *vbi, unsigned char *buf)
{
	vbi_network *n = &vbi->network.ev.network;
	char *short_name, *long_name;
	char *country;
	int cni;

	cni = + ((buf[10] & 3) << 10)
	      + ((buf[11] & 0xC0) << 2)
	      + ((buf[8] & 0xC0) << 0)
	      + (buf[11] & 0x3F);

	if (cni == 0x0DC3)
		cni = (buf[2] & 0x10) ? 0x0DC2 : 0x0DC1;

	if (cni != n->cni_vps) {
		n->cni_vps = cni;
		n->cycle = 1;
	} else if (n->cycle == 1) {
		unsigned int id = station_lookup(CNI_VPS, cni, &country, &short_name, &long_name);

		if (!id) {
			n->name[0] = 0;
			n->label[0] = 0;
			unknown_cni(vbi, "VPS", cni);
		} else {
			strncpy(n->name, long_name, sizeof(n->name) - 1);
			strncpy(n->label, short_name, sizeof(n->label) - 1);
		}

		if (id != n->nuid) {
			if (n->nuid != 0)
				vbi_chsw_reset(vbi, id);

			n->nuid = id;

			vbi->network.type = VBI_EVENT_NETWORK;
			vbi_send_event(vbi, &vbi->network);
		}

		n->cycle = 2;
	}

#if BSDATA_TEST

	{
		static char pr_label[20];
		static char label[20];
		static int l = 0;
		int cni, pcs, pty, pil;
		int c, j;

		printf("\nVPS:\n");

		c = bit_reverse[buf[1]];

		if ((char) c < 0) {
			label[l] = 0;
			memcpy(pr_label, label, sizeof(pr_label));
			l = 0;
		}

		c &= 0x7F;

		label[l] = printable(c);

		l = (l + 1) % 16;

		printf(" 3-10: %02x %02x %02x %02x %02x %02x %02x %02x (\"%s\")\n",
			buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], pr_label);

		cni = + ((buf[10] & 3) << 10)
		      + ((buf[11] & 0xC0) << 2)
		      + ((buf[8] & 0xC0) << 0)
		      + (buf[11] & 0x3F);

		if (cni)
			for (j = 0; VPS_CNI[j].short_name; j++)
				if (VPS_CNI[j].cni == cni) {
					printf(" Country: %s\n Station: %s%s\n",
						country_names_en[VPS_CNI[j].country],
						VPS_CNI[j].long_name,
						(cni == 0x0DC3) ? ((buf[2] & 0x10) ? " (ZDF)" : " (ARD)") : "");
					break;
				}

		pcs = buf[2] >> 6;
		pil = ((buf[8] & 0x3F) << 14) + (buf[9] << 6) + (buf[10] >> 2);
		pty = buf[12];

/*		if (!cni || !VPS_CNI[j].short_name) */
			printf(" CNI: %04x\n", cni);

		printf(" Analog audio: %s\n", pcs_names[pcs]);

		dump_pil(pil);
		dump_pty(pty);
	}

#endif /* BSDATA_TEST */

}

static void
parse_x26_pdc(int address, int mode, int data)
{
	static int day, month, lto, caf, duration;
	static int hour[2], min[2], mi = 0;
	int cni;

	if (mode != 6 && address < 40)
		return;

	switch (mode) {
	case 6:
		if (address >= 40)
			return;
		min[mi] = data & 0x7F; // BCD
		break;

	case 8:
		cni = address * 256 + data;

		if (0) { /* country and network identifier */
			char *country, *short_name, *long_name;

			if (station_lookup(CNI_X26, cni, &country, &short_name, &long_name))
				printf("X/26 country: %s\n... station: %s\n", country, long_name);
			else
				printf("X/26 unknown CNI %04x\n", cni);
		}

		return;

	case 9:
		month = address & 15;
		day = (data >> 4) * 10 + (data & 15);
		break;

	case 10:
		hour[0] = data & 0x3F; // BCD
		caf = !!(data & 0x40);
		mi = 0;
		break;

	case 11:
		hour[1] = data & 0x3F; // BCD
		duration = !!(data & 0x40);
		mi = 1;
		break;

	case 12:
		lto = (data & 0x40) ? ((~0x7F) | data) : data;
		break;

	case 13:
#if BSDATA_TEST
		if (0) {
			printf("X/26 pty series: %d\n", address == 0x30);
			dump_pty(data | 0x80);
		}
#endif
		break;

	default:
		return;
	}

#if BSDATA_TEST

	/*
	 *  It's life, but not as we know it...
	 */
	if (0 && mode == 6)
		printf("X/26 %2d/%2d/%2d; lto=%d, caf=%d, end/dur=%d; %d %s %02x:%02x %02x:%02x\n",
			address, mode, data,
			lto, caf, duration,
			day, month_names[month],
			hour[0], min[0], hour[1], min[1]);
#endif
}

static bool
parse_bsd(struct vbi *vbi, unsigned char *raw, int packet, int designation)
{
	vbi_network *n = &vbi->network.ev.network;
	int err, i;

	switch (packet) {
	case 26:
		/* TODO, iff */
		break;

	case 30:
		if (designation >= 4)
			break;

		if (designation <= 1) {
			char *short_name, *long_name;
			char *country;
			int cni;
#if BSDATA_TEST
			printf("\nPacket 8/30/%d:\n", designation);
#endif
			cni = bit_reverse[raw[7]] * 256 + bit_reverse[raw[8]];

			if (cni != n->cni_8301) {
				n->cni_8301 = cni;
				n->cycle = 1;
			} else if (n->cycle == 1) {
				unsigned int id = station_lookup(CNI_8301, cni, &country, &short_name, &long_name);

				if (!id) {
					n->name[0] = 0;
					n->label[0] = 0;
					unknown_cni(vbi, "8/30/1", cni);
				} else {
					strncpy(n->name, long_name, sizeof(n->name) - 1);
					strncpy(n->label, short_name, sizeof(n->label) - 1);
				}

				if (id != n->nuid) {
					if (n->nuid != 0)
						vbi_chsw_reset(vbi, id);

					n->nuid = id;

					vbi->network.type = VBI_EVENT_NETWORK;
					vbi_send_event(vbi, &vbi->network);
				}

				n->cycle = 2;
			}
#if BSDATA_TEST
			if (1) { /* country and network identifier */
				if (station_lookup(CNI_8301, cni, &country, &short_name, &long_name))
					printf("... country: %s\n... station: %s\n", country, long_name);
				else
					printf("... unknown CNI %04x\n", cni);
			}

			if (1) { /* local time */
				int lto, mjd, utc_h, utc_m, utc_s;
				struct tm tm;
				time_t ti;

				lto = (raw[9] & 0x7F) >> 1;

				mjd = + ((raw[10] & 15) - 1) * 10000
				      + ((raw[11] >> 4) - 1) * 1000
				      + ((raw[11] & 15) - 1) * 100
				      + ((raw[12] >> 4) - 1) * 10
				      + ((raw[12] & 15) - 1);

			    	utc_h = ((raw[13] >> 4) - 1) * 10 + ((raw[13] & 15) - 1);
				utc_m = ((raw[14] >> 4) - 1) * 10 + ((raw[14] & 15) - 1);
				utc_s = ((raw[15] >> 4) - 1) * 10 + ((raw[15] & 15) - 1);

				ti = (mjd - 40587) * 86400 + 43200;
				localtime_r(&ti, &tm);

				printf("... local time: MJD %d %02d %s %04d, UTC %02d:%02d:%02d %c%02d%02d\n",
					mjd, tm.tm_mday, month_names[tm.tm_mon + 1], tm.tm_year + 1900,
					utc_h, utc_m, utc_s, (raw[9] & 0x80) ? '-' : '+', lto >> 1, (lto & 1) * 30);
			}
#endif /* BSDATA_TEST */

		} else /* if (designation <= 3) */ {
			int t, b[7];
			char *short_name, *long_name;
			char *country;
			int cni;
#if BSDATA_TEST
			printf("\nPacket 8/30/%d:\n", designation);
#endif
			for (err = i = 0; i < 7; i++) {
				err |= t = hamm16(raw + i * 2 + 6);
				b[i] = bit_reverse[t];
			}

			if (err < 0)
				return FALSE;

			cni = + ((b[4] & 0x03) << 10)
			      + ((b[5] & 0xC0) << 2)
			      + (b[2] & 0xC0)
			      + (b[5] & 0x3F)
			      + ((b[1] & 0x0F) << 12);

			if (cni == 0x0DC3)
				cni = (b[2] & 0x10) ? 0x0DC2 : 0x0DC1;

			if (cni != n->cni_8302) {
				n->cni_8302 = cni;
				n->cycle = 1;
			} else if (n->cycle == 1) {
				unsigned int id = station_lookup(CNI_8302, cni, &country, &short_name, &long_name);

				if (!id) {
					n->name[0] = 0;
					n->label[0] = 0;
					unknown_cni(vbi, "8/30/2", cni);
				} else {
					strncpy(n->name, long_name, sizeof(n->name) - 1);
					strncpy(n->label, short_name, sizeof(n->label) - 1);
				}

				if (id != n->nuid) {
					if (n->nuid != 0)
						vbi_chsw_reset(vbi, id);

					n->nuid = id;

					vbi->network.type = VBI_EVENT_NETWORK;
					vbi_send_event(vbi, &vbi->network);
				}

				n->cycle = 2;
			}

#if BSDATA_TEST
			if (1) { /* country and network identifier */
				char *country, *short_name, *long_name;

				if (station_lookup(CNI_8302, cni, &country, &short_name, &long_name))
					printf("... country: %s\n... station: %s\n", country, long_name);
				else
					printf("... unknown CNI %04x\n", cni);
			}

			if (1) { /* PDC data */
				int lci, luf, prf, mi, pil;

				lci = (n[0] >> 2) & 3;
				luf = !!(n[0] & 2);
				prf = n[0] & 1;
				mi = !!(n[1] & 0x20);
				pil = ((n[2] & 0x3F) << 14) + (n[3] << 6) + (n[4] >> 2);

				printf("... label channel %d: update %d,"
				       " prepare to record %d, mode %d\n",
					lci, luf, prf, mi);
				dump_pil(pil);
			}

			if (1) {
				int pty, pcs;

				pcs = n[1] >> 6;
				pty = n[6];

				printf("... analog audio: %s\n", pcs_names[pcs]);
				dump_pty(pty);
			}
#endif /* BSDATA_TEST */

		}

#if BSDATA_TEST
		/*
		 *  "transmission status message, e.g. the programme title",
		 *  "default G0 set". Render like subtitles a la TV or lang.c
		 *  translation to latin etc?
		 */
		if (1) { 
			printf("... status: \"");

			for (i = 20; i < 40; i++) {
				int c = parity(raw[i]);

				c = (c < 0) ? '?' : printable(c);
				putchar(c);
			}

			printf("\"\n");
		}
#endif
		return TRUE;
	}

	return TRUE;
}

static int
same_header(int cur_pgno, unsigned char *cur,
	    int ref_pgno, unsigned char *ref,
	    int *page_num_offsetp)
{
	unsigned char buf[3];
	int i, j = 32 - 3, err = 0, neq = 0;

	/* Assumes is_bcd(cur_pgno) */
	buf[2] = (cur_pgno & 15) + '0';
	buf[1] = ((cur_pgno >> 4) & 15) + '0';
	buf[0] = (cur_pgno >> 8) + '0';

	set_parity(buf, 3);

	for (i = 8; i < 32; cur++, ref++, i++) {
		/* Skip page number */
		if (i < j
		    && cur[0] == buf[0]
		    && cur[1] == buf[1]
		    && cur[2] == buf[2]) {
			j = i; /* here, once */
			i += 3;
			cur += 3;
			ref += 3;
			continue;
		}

		err |= parity(*cur);
		err |= parity(*ref);

		neq |= *cur - *ref;
	}

	if (err < 0 || j >= 32 - 3) /* parity error, rare */
		return -2; /* inconclusive, useless */

	*page_num_offsetp = j;

	if (!neq)
		return TRUE;

	/* Test false negative due to date transition */

	if (((ref[32] * 256 + ref[33]) & 0x7F7F) == 0x3233
	    && ((cur[32] * 256 + cur[33]) & 0x7F7F) == 0x3030) {
		return -1; /* inconclusive */
	}

	/*
	 *  The problem here is that individual pages or
	 *  magazines from the same network can still differ.
	 */
	return FALSE;
}

static inline bool
same_clock(unsigned char *cur, unsigned char *ref)
{
	int i;

	for (i = 32; i < 40; cur++, ref++, i++)
	       	if (*cur != *ref && (parity(*cur) | parity(*ref)) >= 0)
			return FALSE;
	return TRUE;
}

static inline bool
store_lop(struct vbi *vbi, struct vt_page *vtp)
{
	struct page_info *pi;
	vbi_event event;

	event.type = VBI_EVENT_TTX_PAGE;

	event.ev.ttx_page.pgno = vtp->pgno;
	event.ev.ttx_page.subno = vtp->subno;

	event.ev.ttx_page.roll_header =
		(((vtp->flags & (  C5_NEWSFLASH
				 | C6_SUBTITLE 
				 | C7_SUPPRESS_HEADER
				 | C9_INTERRUPTED
			         | C10_INHIBIT_DISPLAY)) == 0)
		 && (vtp->pgno <= 0x199
		     || (vtp->flags & C11_MAGAZINE_SERIAL))
		 && is_bcd(vtp->pgno) /* no hex numbers */);

	event.ev.ttx_page.header_update = FALSE;
	event.ev.ttx_page.raw_header = NULL;
	event.ev.ttx_page.pn_offset = -1;

	/*
	 *  We're not always notified about a channel switch,
	 *  this code prevents a terrible mess in the cache.
	 *
	 *  The roll_header thing shall reduce false negatives,
	 *  slows down detection of some stations, but does help.
	 *  A little. Maybe this should be optional.
	 */
	if (event.ev.ttx_page.roll_header) {
		int r;

		if (vbi->vt.header_page.pgno == 0) {
			/* First page after channel switch */
			r = same_header(vtp->pgno, vtp->data.lop.raw[0] + 8,
					vtp->pgno, vtp->data.lop.raw[0] + 8,
					&event.ev.ttx_page.pn_offset);
			event.ev.ttx_page.header_update = TRUE;
			event.ev.ttx_page.clock_update = TRUE;
		} else {
			r = same_header(vtp->pgno, vtp->data.lop.raw[0] + 8,
					vbi->vt.header_page.pgno, vbi->vt.header + 8,
					&event.ev.ttx_page.pn_offset);
			event.ev.ttx_page.clock_update =
				!same_clock(vtp->data.lop.raw[0], vbi->vt.header);
		}

		switch (r) {
		case TRUE:
			// fprintf(stderr, "+");

			pthread_mutex_lock(&vbi->chswcd_mutex);
			vbi->chswcd = 0;
			pthread_mutex_unlock(&vbi->chswcd_mutex);

			vbi->vt.header_page.pgno = vtp->pgno;
			memcpy(vbi->vt.header + 8,
			       vtp->data.lop.raw[0] + 8, 32);

			event.ev.ttx_page.raw_header = vbi->vt.header;

			break;

		case FALSE:
			/*
			 *  What can I do when every magazin has its own
			 *  header? Ouch. Let's hope p100 repeats frequently.
			 */
			if (((vtp->pgno ^ vbi->vt.header_page.pgno) & 0xF00) == 0) {
			     /* pthread_mutex_lock(&vbi->chswcd_mutex);
				if (vbi->chswcd == 0)
					vbi->chswcd = 40;
				pthread_mutex_unlock(&vbi->chswcd_mutex); */

				vbi_chsw_reset(vbi, 0);
				return TRUE;
			}

			/* fall through */

		default: /* inconclusive */
			pthread_mutex_lock(&vbi->chswcd_mutex);

			if (vbi->chswcd > 0) {
				pthread_mutex_unlock(&vbi->chswcd_mutex);
				return TRUE;
			}

			pthread_mutex_unlock(&vbi->chswcd_mutex);

			if (r == -1) {
				vbi->vt.header_page.pgno = vtp->pgno;
				memcpy(vbi->vt.header + 8,
				       vtp->data.lop.raw[0] + 8, 32);

				event.ev.ttx_page.raw_header = vbi->vt.header;

				// fprintf(stderr, "/");
			} else /* broken header */ {
				event.ev.ttx_page.roll_header = FALSE;
				event.ev.ttx_page.clock_update = FALSE;

				// fprintf(stderr, "X");
			}

			break;
		}

		if (0) {
			int i;

			for (i = 0; i < 40; i++)
				putchar(printable(vtp->data.unknown.raw[0][i]));
			putchar('\r');
			fflush(stdout);
		}
	} else {
		// fprintf(stderr, "-");
	}

	/*
	 *  Collect information about those pages
	 *  not listed in MIP etc.
	 */
	pi = vbi->vt.page_info + vtp->pgno - 0x100;

	if (pi->code == VBI_SUBTITLE_PAGE) {
		if (pi->language == 0xFF)
			pi->language = page_language(&vbi->vt, vtp, 0, 0);
	} else if (pi->code == VBI_NO_PAGE || pi->code == VBI_UNKNOWN_PAGE)
		pi->code = VBI_NORMAL_PAGE;

	if (pi->subcode >= 0xFFFE || vtp->subno > pi->subcode)
		pi->subcode = vtp->subno;

	/*
	 *  Store the page and send event.
	 */
	if (vbi_cache_put(vbi, vtp))
		vbi_send_event(vbi, &event);

	return TRUE;
}

#define TTX_EVENTS (VBI_EVENT_TTX_PAGE)
#define BSDATA_EVENTS (VBI_EVENT_NETWORK)

/*
 *  Teletext packet 27, page linking
 */
static inline bool
parse_27(struct vbi *vbi, unsigned char *p,
	 struct vt_page *cvtp, int mag0)
{
	int designation, control;
	int i;

	if (cvtp->function == PAGE_FUNCTION_DISCARD)
		return TRUE;

	if ((designation = hamm8(*p)) < 0)
		return FALSE;

//	printf("Packet X/27/%d page %x\n", designation, cvtp->pgno);

	switch (designation) {
	case 0:
		if ((control = hamm8(p[37])) < 0)
			return FALSE;

		/* printf("%x.%x X/27/%d %02x\n",
		       cvtp->pgno, cvtp->subno, designation, control); */
#if 0
/*
 *  CRC cannot be trusted, some stations transmit rubbish.
 *  Link Control Byte bits 1 ... 3 cannot be trusted, ETS 300 706 is
 *  inconclusive and not all stations follow the suggestions in ETR 287.
 */
		crc = p[38] + p[39] * 256;
		/* printf("CRC: %04x\n", crc); */

		if ((control & 7) == 0)
			return FALSE;
#endif
		cvtp->data.unknown.flof = control >> 3; /* display row 24 */

		/* fall through */
	case 1:
	case 2:
	case 3:
		for (p++, i = 0; i <= 5; p += 6, i++) {
			if (!hamm8_page_number(cvtp->data.unknown.link
					       + designation * 6 + i, p, mag0))
				; /* return TRUE; */

// printf("X/27/%d link[%d] page %03x/%03x\n", designation, i,
//	cvtp->data.unknown.link[designation * 6 + i].pgno, cvtp->data.unknown.link[designation * 6 + i].subno);
		}

		break;

	case 4:
	case 5:
		for (p++, i = 0; i <= 5; p += 6, i++) {
			int t1, t2;

			t1 = hamm24(p + 0);
			t2 = hamm24(p + 3);

			if ((t1 | t2) < 0)
				return FALSE;

			cvtp->data.unknown.link[designation * 6 + i].type = t1 & 3;
			cvtp->data.unknown.link[designation * 6 + i].pgno =
				((((t1 >> 12) & 0x7) ^ mag0) ? : 8) * 256
				+ ((t1 >> 11) & 0x0F0) + ((t1 >> 7) & 0x00F);
			cvtp->data.unknown.link[designation * 6 + i].subno =
				(t2 >> 3) & 0xFFFF;
if(0)
 printf("X/27/%d link[%d] type %d page %03x subno %04x\n", designation, i,
	cvtp->data.unknown.link[designation * 6 + i].type,
	cvtp->data.unknown.link[designation * 6 + i].pgno,
	cvtp->data.unknown.link[designation * 6 + i].subno);
		}

		break;
	}

	return TRUE;
}

/*
 *  Teletext packets 28 and 29, Level 2.5/3.5 enhancement
 */
static inline bool
parse_28_29(struct vbi *vbi, unsigned char *p,
	 struct vt_page *cvtp, int mag8, int packet)
{
	int designation, function;
	int triplets[13], *triplet = triplets, buf = 0, left = 0;
	extension *ext;
	int i, j, err = 0;

	static int
	bits(int count)
	{
		int r, n;

		r = buf;

		if ((n = count - left) > 0) {
			r |= (buf = *triplet++) << left;
			left = 18;
		} else
			n = count;

		buf >>= n;
		left -= n;

		return r & ((1UL << count) - 1);
	}

	if ((designation = hamm8(*p)) < 0)
		return FALSE;

//	printf("Packet %d/%d/%d page %x\n", mag8, packet, designation, cvtp->pgno);

	for (p++, i = 0; i < 13; p += 3, i++)
		err |= triplet[i] = hamm24(p);

	switch (designation) {
	case 0: /* X/28/0, M/29/0 Level 2.5 */
	case 4: /* X/28/4, M/29/4 Level 3.5 */
		if (err < 0)
			return FALSE;

		function = bits(4);
		bits(3); /* page coding ignored */

//		printf("... function %d\n", function);

		/*
		 *  ZDF and BR3 transmit GPOP 1EE/.. with 1/28/0 function
		 *  0 = PAGE_FUNCTION_LOP, should be PAGE_FUNCTION_GPOP.
		 *  Makes no sense to me. Update: also encountered pages
		 *  mFE and mFF with function = 0. Strange. 
		 */
		if (function != PAGE_FUNCTION_LOP && packet == 28) {
			if (cvtp->function != PAGE_FUNCTION_UNKNOWN
			    && cvtp->function != function)
				return FALSE; /* XXX discard rpage? */

// XXX rethink		cvtp->function = function;
		}

		if (function != PAGE_FUNCTION_LOP)
			return FALSE;

		/* XXX X/28/0 Format 2, distinguish how? */

		ext = &vbi->vt.magazine[mag8].extension;

		if (packet == 28) {
			if (!cvtp->data.ext_lop.ext.designations) {
				cvtp->data.ext_lop.ext = *ext;
				cvtp->data.ext_lop.ext.designations <<= 16;
				cvtp->data.lop.ext = TRUE;
			}

			ext = &cvtp->data.ext_lop.ext;
		}

		if (designation == 4 && (ext->designations & (1 << 0)))
			bits(14 + 2 + 1 + 4);
		else {
			ext->char_set[0] = bits(7);
			ext->char_set[1] = bits(7);

			ext->fallback.left_side_panel = bits(1);
			ext->fallback.right_side_panel = bits(1);

			bits(1); /* panel status: level 2.5/3.5 */

			ext->fallback.left_panel_columns =
				bit_reverse[bits(4)] >> 4;

			if (ext->fallback.left_side_panel
			    | ext->fallback.right_side_panel)
				ext->fallback.left_panel_columns =
					ext->fallback.left_panel_columns ? : 16;
		}

		j = (designation == 4) ? 16 : 32;

		for (i = j - 16; i < j; i++) {
			attr_rgba col = bits(12);

			if (i == 8) /* transparent */
				continue;

			col |= (col & 0xF00) << 8;
			col &= 0xF00FF;
			col |= (col & 0xF0) << 4;
			col &= 0xF0F0F;

			ext->colour_map[i] = col | (col << 4) | 0xFF000000UL;
		}

		if (designation == 4 && (ext->designations & (1 << 0)))
			bits(10 + 1 + 3);
		else {
			ext->def_screen_colour = bits(5);
			ext->def_row_colour = bits(5);

			ext->fallback.black_bg_substitution = bits(1);

			i = bits(3); /* colour table remapping */

			ext->foreground_clut = "\00\00\00\10\10\20\20\20"[i];
			ext->background_clut = "\00\10\20\10\20\10\20\30"[i];
		}

		ext->designations |= 1 << designation;

		if (packet == 29) {
			if (0 && designation == 4)
				ext->designations &= ~(1 << 0);

			/*
			    XXX update
			    inherited_mag_desig =
			       page->extension.designations >> 16;
			    new_mag_desig = 1 << designation;
			    page_desig = page->extension.designations;
			    if (((inherited_mag_desig | page_desig)
			       & new_mag_desig) == 0)
			    shortcut: AND of (inherited_mag_desig | page_desig)
			      of all pages with extensions, no updates required
			      in round 2++
			    other option, all M/29/x should have been received
			      within the maximum repetition interval of 20 s.
			*/
		}

		return FALSE;

	case 1: /* X/28/1, M/29/1 Level 3.5 DRCS CLUT */
		ext = &vbi->vt.magazine[mag8].extension;

		if (packet == 28) {
			if (!cvtp->data.ext_lop.ext.designations) {
				cvtp->data.ext_lop.ext = *ext;
				cvtp->data.ext_lop.ext.designations <<= 16;
				cvtp->data.lop.ext = TRUE;
			}

			ext = &cvtp->data.ext_lop.ext;
			/* XXX TODO - lop? */
		}

		triplet++;

		for (i = 0; i < 8; i++)
			ext->drcs_clut[i + 2] = bit_reverse[bits(5)] >> 3;

		for (i = 0; i < 32; i++)
			ext->drcs_clut[i + 10] = bit_reverse[bits(5)] >> 3;

		ext->designations |= 1 << 1;

		if (0)
			dump_extension(ext);

		return FALSE;

	case 3: /* X/28/3 Level 2.5, 3.5 DRCS download page */
		if (packet == 29)
			break; /* M/29/3 undefined */

		if (err < 0)
			return FALSE;

		function = bits(4);
		bits(3); /* page coding ignored */

		if (function != PAGE_FUNCTION_GDRCS
		    || function != PAGE_FUNCTION_DRCS)
			return FALSE;

		if (cvtp->function == PAGE_FUNCTION_UNKNOWN) {
			memmove(cvtp->data.drcs.raw,
				cvtp->data.unknown.raw,
				sizeof(cvtp->data.drcs.raw));
			cvtp->function = function;
		} else if (cvtp->function != function) {
			cvtp->function = PAGE_FUNCTION_DISCARD;
			return 0;
		}

		bits(11);

		for (i = 0; i < 48; i++)
			cvtp->data.drcs.mode[i] = bits(4);

	default: /* ? */
		break;
	}

	return TRUE;
}

/*
 *  Teletext packet 8/30, broadcast service data
 */
static inline bool
parse_8_30(struct vbi *vbi, unsigned char *p, int packet)
{
	int designation;

	if ((designation = hamm8(*p)) < 0)
		return FALSE;

//	printf("Packet 8/30/%d\n", designation);

	if (designation > 4)
		return TRUE; /* ignored */

	if (vbi->event_mask & TTX_EVENTS) {
		if (!hamm8_page_number(&vbi->vt.initial_page, p + 1, 0))
			return FALSE;

		if ((vbi->vt.initial_page.pgno & 0xFF) == 0xFF) {
			vbi->vt.initial_page.pgno = 0x100;
			vbi->vt.initial_page.subno = ANY_SUB;
		}
	}

	if (vbi->event_mask & BSDATA_EVENTS)
		return parse_bsd(vbi, p, packet, designation);

	return TRUE;
}

bool
vbi_teletext_packet(struct vbi *vbi, unsigned char *p)
{
	struct vt_page *cvtp;
	struct raw_page *rvtp;
	int pmag, mag0, mag8, packet;
	magazine *mag;

	if ((pmag = hamm16(p)) < 0)
		return FALSE;

	mag0 = pmag & 7;
	mag8 = mag0 ? : 8;
	packet = pmag >> 3;

	if (packet < 30
	    && !(vbi->event_mask & TTX_EVENTS))
		return TRUE;

	mag = vbi->vt.magazine + mag8;
	rvtp = vbi->vt.raw_page + mag0;
	cvtp = rvtp->page;

	p += 2;

	switch (packet) {
	case 0:
	{
		int pgno, page, subpage, flags;
		struct raw_page *curr;
		struct vt_page *vtp;
		int i;

		if ((page = hamm16(p)) < 0) {
			vbi_teletext_desync(vbi);
//			printf("Hamming error in packet 0 page number\n");
			return FALSE;
		}

		pgno = mag8 * 256 + page;

		/*
		 *  Store page terminated by new header.
		 */
		while ((curr = vbi->vt.current)) {
			vtp = curr->page;

			if (vtp->flags & C11_MAGAZINE_SERIAL) {
				if (vtp->pgno == pgno)
					break;
			} else {
				curr = rvtp;
				vtp = curr->page;

				if ((vtp->pgno & 0xFF) == page)
					break;
			}

			switch (vtp->function) {
			case PAGE_FUNCTION_DISCARD:
				break;

			case PAGE_FUNCTION_LOP:
				if (!store_lop(vbi, vtp))
					return FALSE;
				break;

			case PAGE_FUNCTION_DRCS:
			case PAGE_FUNCTION_GDRCS:
				if (convert_drcs(vtp, vtp->data.drcs.raw[1]))
					vbi_cache_put(vbi, vtp);
				break;

			case PAGE_FUNCTION_MIP:
				parse_mip(vbi, vtp);
				break;

			case PAGE_FUNCTION_TRIGGER:
				eacem_trigger(vbi, vtp);
				break;

			default:
				vbi_cache_put(vbi, vtp);
				break;
			}

			vtp->function = PAGE_FUNCTION_DISCARD;
			break;
		}

		/*
		 *  Prepare for new page.
		 */

		cvtp->pgno = pgno;
		vbi->vt.current = rvtp;

		subpage = hamm16(p + 2) + hamm16(p + 4) * 256;
		flags = hamm16(p + 6);

		if (page == 0xFF || (subpage | flags) < 0) {
			cvtp->function = PAGE_FUNCTION_DISCARD;
			return FALSE;
		}

		cvtp->subno = subpage & 0x3F7F;
		cvtp->national = bit_reverse[flags] & 7;
		cvtp->flags = (flags << 16) + subpage;

		if (0 && ((page & 15) > 9 || page > 0x99))
			printf("data page %03x/%04x\n", cvtp->pgno, cvtp->subno);

		if (1
		    && pgno != 0x1E7
		    && !(cvtp->flags & C4_ERASE_PAGE)
		    && (vtp = vbi_cache_get(vbi, cvtp->pgno, cvtp->subno, -1)))
		{
			memset(&cvtp->data, 0, sizeof(cvtp->data));
			memcpy(&cvtp->data, &vtp->data,
			       vtp_size(vtp) - sizeof(*vtp) + sizeof(vtp->data));

			/* XXX write cache directly | erc?*/
			/* XXX data page update */

			cvtp->function = vtp->function;

			switch (cvtp->function) {
			case PAGE_FUNCTION_UNKNOWN:
			case PAGE_FUNCTION_LOP:
				memcpy(cvtp->data.unknown.raw[0], p, 40);

			default:
				break;
			}

			cvtp->lop_lines = vtp->lop_lines;
			cvtp->enh_lines = vtp->enh_lines;
		} else {
			cvtp->flags |= C4_ERASE_PAGE;

			if (0)
				printf("rebuilding %3x/%04x from scratch\n", cvtp->pgno, cvtp->subno);

			if (cvtp->pgno == 0x1F0) {
				cvtp->function = PAGE_FUNCTION_BTT;
				vbi->vt.page_info[0x1F0 - 0x100].code = VBI_TOP_PAGE;
			} else if (cvtp->pgno == 0x1E7) {
				cvtp->function = PAGE_FUNCTION_TRIGGER;
				vbi->vt.page_info[0x1E7 - 0x100].code = VBI_DISP_SYSTEM_PAGE;
				vbi->vt.page_info[0x1E7 - 0x100].subcode = 0;
				memset(cvtp->data.unknown.raw[0], 0x20, sizeof(cvtp->data.unknown.raw));
				memset(cvtp->data.enh_lop.enh, 0xFF, sizeof(cvtp->data.enh_lop.enh));
				cvtp->data.unknown.ext = FALSE;
			} else if (page == 0xFD) {
				cvtp->function = PAGE_FUNCTION_MIP;
				vbi->vt.page_info[cvtp->pgno - 0x100].code = VBI_SYSTEM_PAGE;
			} else if (page == 0xFE) {
				cvtp->function = PAGE_FUNCTION_MOT;
				vbi->vt.page_info[cvtp->pgno - 0x100].code = VBI_SYSTEM_PAGE;
			} else {
				cvtp->function = PAGE_FUNCTION_UNKNOWN;

				memcpy(cvtp->data.unknown.raw[0] + 0, p, 40);
				memset(cvtp->data.unknown.raw[0] + 40, 0x20, sizeof(cvtp->data.unknown.raw) - 40);
				memset(cvtp->data.unknown.link, 0xFF, sizeof(cvtp->data.unknown.link));
				memset(cvtp->data.enh_lop.enh, 0xFF, sizeof(cvtp->data.enh_lop.enh));

				cvtp->data.unknown.flof = FALSE;
				cvtp->data.unknown.ext = FALSE;
			}

			cvtp->lop_lines = 1;
			cvtp->enh_lines = 0;
		}

		if (cvtp->function == PAGE_FUNCTION_UNKNOWN) {
			page_function function = PAGE_FUNCTION_UNKNOWN;

			switch (vbi->vt.page_info[cvtp->pgno - 0x100].code) {
			case 0x01 ... 0x51:
			case 0x70 ... 0x7F:
			case 0x81 ... 0xD1:
			case 0xF4 ... 0xF7:
			case VBI_TOP_BLOCK:
			case VBI_TOP_GROUP:
				function = PAGE_FUNCTION_LOP;
				break;

			case VBI_SYSTEM_PAGE:	/* no MOT or MIP?? */
				/* remains function = PAGE_FUNCTION_UNKNOWN; */
				break;

			case VBI_TOP_PAGE:
				for (i = 0; i < 8; i++)
					if (cvtp->pgno == vbi->vt.btt_link[i].pgno)
						break;
				if (i < 8) {
					switch (vbi->vt.btt_link[i].type) {
					case 1:
						function = PAGE_FUNCTION_MPT;
						break;
					case 2:
						function = PAGE_FUNCTION_AIT;
						break;
					case 3:
						function = PAGE_FUNCTION_MPT_EX;
						break;
					default:
						if (0)
							printf("page is TOP, link %d, unknown type %d\n",
								i, vbi->vt.btt_link[i].type);
					}
				} else if (0)
					printf("page claims to be TOP, link not found\n");

				break;

			case 0xE5:
			case 0xE8 ... 0xEB:
				function = PAGE_FUNCTION_DRCS;
				break;

			case 0xE6:
			case 0xEC ... 0xEF:
				function = PAGE_FUNCTION_POP;
				break;

			case VBI_TRIGGER_DATA:
				function = PAGE_FUNCTION_TRIGGER;
				break;

			case 0x52 ... 0x6F:	/* reserved */
			case VBI_EPG_DATA:	/* EPG/NexTView transport layer */
			case VBI_ACI:		/* ACI page */
			case VBI_NOT_PUBLIC:
			case 0xD2 ... 0xDF:	/* reserved */
			case 0xE0 ... 0xE2:	/* data broadcasting */
			case 0xE4:		/* data broadcasting */
			case 0xF0 ... 0xF3:	/* broadcaster system page */
				function = PAGE_FUNCTION_DISCARD;
				break;

			default:
				if (page <= 0x99 && (page & 15) <= 9)
					function = PAGE_FUNCTION_LOP;
				/* else remains
					function = PAGE_FUNCTION_UNKNOWN; */
			}

			if (function != PAGE_FUNCTION_UNKNOWN) {
				vbi_convert_page(vbi, cvtp, FALSE, function);
			}
		}
//XXX?
		cvtp->data.ext_lop.ext.designations = 0;
		rvtp->num_triplets = 0;

		return TRUE;
	}

	case 1 ... 25:
	{
		char n;
		int i;

		switch (cvtp->function) {
		case PAGE_FUNCTION_DISCARD:
			return TRUE;

		case PAGE_FUNCTION_MOT:
			if (!parse_mot(vbi->vt.magazine + mag8, p, packet))
				return FALSE;
			break;

		case PAGE_FUNCTION_GPOP:
		case PAGE_FUNCTION_POP:
			if (!parse_pop(cvtp, p, packet))
				return FALSE;
			break;

		case PAGE_FUNCTION_GDRCS:
		case PAGE_FUNCTION_DRCS:
			memcpy(cvtp->data.drcs.raw[packet], p, 40);
			break;

		case PAGE_FUNCTION_BTT:
			if (!parse_btt(vbi, p, packet))
				return FALSE;
			break;

		case PAGE_FUNCTION_AIT:
			if (!(parse_ait(cvtp, p, packet)))
				return FALSE;
			break;

		case PAGE_FUNCTION_MPT:
			if (!(parse_mpt(&vbi->vt, p, packet)))
				return FALSE;
			break;

		case PAGE_FUNCTION_MPT_EX:
			if (!(parse_mpt_ex(&vbi->vt, p, packet)))
				return FALSE;
			break;

		case PAGE_FUNCTION_LOP:
		case PAGE_FUNCTION_TRIGGER:
			for (n = i = 0; i < 40; i++)
				n |= parity(p[i]);
			if (n < 0)
				return FALSE;

			/* fall through */

		case PAGE_FUNCTION_MIP:
		default:
			memcpy(cvtp->data.unknown.raw[packet], p, 40);
			break;
		}

		cvtp->lop_lines |= 1 << packet;

		break;
	}

	case 26:
	{
		int designation;
		vt_triplet triplet;
		int i;

		/*
		 *  Page enhancement packet
		 */

		switch (cvtp->function) {
		case PAGE_FUNCTION_DISCARD:
			return TRUE;

		case PAGE_FUNCTION_GPOP:
		case PAGE_FUNCTION_POP:
			return parse_pop(cvtp, p, packet);

		case PAGE_FUNCTION_GDRCS:
		case PAGE_FUNCTION_DRCS:
		case PAGE_FUNCTION_BTT:
		case PAGE_FUNCTION_AIT:
		case PAGE_FUNCTION_MPT:
		case PAGE_FUNCTION_MPT_EX:
			/* X/26 ? */
			vbi_teletext_desync(vbi);
			return TRUE;

		case PAGE_FUNCTION_TRIGGER:
		default:
			break;
		}

		if ((designation = hamm8(*p)) < 0)
			return FALSE;

		if (rvtp->num_triplets >= 16 * 13
		    || rvtp->num_triplets != designation * 13) {
			rvtp->num_triplets = -1;
			return FALSE;
		}

		for (p++, i = 0; i < 13; p += 3, i++) {
			int t = hamm24(p);

			if (t < 0)
				break; /* XXX */

			triplet.address = t & 0x3F;
			triplet.mode = (t >> 6) & 0x1F;
			triplet.data = t >> 11;

			cvtp->data.enh_lop.enh[rvtp->num_triplets++] = triplet;

			if (0
			    && triplet.mode >= 6
			    && triplet.mode <= 13)
				parse_x26_pdc(triplet.address,
					      triplet.mode, triplet.data);
		}

		cvtp->enh_lines |= 1 << designation;

		break;
	}

	case 27:
		if (!parse_27(vbi, p, cvtp, mag0))
			return FALSE;
		break;

	case 28:
		if (cvtp->function == PAGE_FUNCTION_DISCARD)
			break;

		/* fall through */
	case 29:
		if (!parse_28_29(vbi, p, cvtp, mag8, packet))
			return FALSE;
		break;

	case 30:
	case 31:
		/*
		 *  IDL packet (ETS 300 708)
		 */
		switch (/* Channel */ pmag & 15) {
		case 0: /* Packet 8/30 (ETS 300 706) */
			if (!parse_8_30(vbi, p, packet))
				return FALSE;
			break;

		default:
#ifdef LIBVBI_IDL_ALERT /* no 6.8 ??? */
			fprintf(stderr, "IDL: %d\n", pmag & 0x0F);
#endif
			break;
		}

		break;
	}

	return TRUE;
}

/*
 *  ETS 300 706 Table 30: Colour Map
 */
static const attr_rgba
default_colour_map[40] = {
	0xFF000000, 0xFF0000FF, 0xFF00FF00, 0xFF00FFFF,
	0xFFFF0000, 0xFFFF00FF, 0xFFFFFF00, 0xFFFFFFFF,
	0xFF000000, 0xFF000077, 0xFF007700, 0xFF007777,
	0xFF770000, 0xFF770077, 0xFF777700, 0xFF777777,
	0xFF5500FF, 0xFF0077FF, 0xFF77FF00, 0xFFBBFFFF,
	0xFFAACC00, 0xFF000055, 0xFF225566, 0xFF7777CC,
	0xFF333333, 0xFF7777FF, 0xFF77FF77, 0xFF77FFFF,
	0xFFFF7777, 0xFFFF77FF, 0xFFFFFF77, 0xFFDDDDDD,

	/* Private colours */

	0xFF000000, 0xFF99AAFF, 0xFF44EE00, 0xFF00DDFF,
	0xFFFFAA99, 0xFFFF00FF, 0xFFFFFF00, 0xFFEEEEEE,
};

/**
 * vbi_set_default_region:
 * @vbi: vbi decoder context
 * @default_region: a value between 0 ... 80, index into
 *   font_descriptors[] table, lang.c. The three lsb are
 *   insignificant.
 * 
 * The original Teletext specification distinguished between
 * eight national character sets. When more countries started
 * to broadcast Teletext the three bit character set id was
 * locally redefined and later extended to seven bits grouping
 * the regional variations. Since some stations still transmit
 * only the legacy id and we don't ship regional variants this
 * function can be used to set a default for the extended bits.
 * The factory default is 16.
 **/
void
vbi_teletext_set_default_region(struct vbi *vbi, int default_region)
{
	int i;

	if (default_region < 0 || default_region > 87)
		return;

	vbi->vt.region = default_region;

	for (i = 0; i < 9; i++) {
		extension *ext = &vbi->vt.magazine[i].extension;

		ext->char_set[0] =
		ext->char_set[1] =
			default_region;
	}
}

void
vbi_teletext_set_level(struct vbi *vbi, int level)
{
	if (level < VBI_LEVEL_1)
		level = VBI_LEVEL_1;
	else if (level > VBI_LEVEL_3p5)
		level = VBI_LEVEL_3p5;

	vbi->vt.max_level = level;
}

void
vbi_teletext_desync(struct vbi *vbi)
{
	int i;

	/* Discard all in progress pages */

	for (i = 0; i < 8; i++)
		vbi->vt.raw_page[i].page->function = PAGE_FUNCTION_DISCARD;
}

void
vbi_teletext_channel_switched(struct vbi *vbi)
{
	magazine *mag;
	extension *ext;
	int i, j;

	vbi->vt.initial_page.pgno = 0x100;
	vbi->vt.initial_page.subno = ANY_SUB;

	vbi->vt.top = FALSE;

	memset(vbi->vt.page_info, 0xFF, sizeof(vbi->vt.page_info));

	/* Magazine defaults */

	memset(vbi->vt.magazine, 0, sizeof(vbi->vt.magazine));

	for (i = 0; i < 9; i++) {
		mag = vbi->vt.magazine + i;

		for (j = 0; j < 16; j++) {
			mag->pop_link[j].pgno = 0x0FF;		/* unused */
			mag->drcs_link[j] = 0x0FF;		/* unused */
		}

		ext = &mag->extension;

		ext->def_screen_colour		= BLACK;	/* A.5 */
		ext->def_row_colour		= BLACK;	/* A.5 */
		ext->foreground_clut		= 0;
		ext->background_clut		= 0;

		for (j = 0; j < 8; j++)
			ext->drcs_clut[j + 2] = j & 3;

		for (j = 0; j < 32; j++)
			ext->drcs_clut[j + 10] = j & 15;

		memcpy(ext->colour_map, default_colour_map, sizeof(ext->colour_map));
	}

	vbi_teletext_set_default_region(vbi, vbi->vt.region);

	vbi_teletext_desync(vbi);
}

void
vbi_teletext_init(struct vbi *vbi)
{
	vbi->vt.region = 16;
	vbi->vt.max_level = VBI_LEVEL_2p5;

	vbi_teletext_channel_switched(vbi);     /* Reset */
}













