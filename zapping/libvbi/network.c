/*
 *  libzvbi - Network identification
 *
 *  Copyright (C) 2000, 2001, 2002, 2003, 2004 Michael H. Schimek
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

/* $Id: network.c,v 1.3 2005-01-19 04:17:54 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <ctype.h>		/* isdigit() */
#include <assert.h>
#include "misc.h"		/* CLEAR() */
#include "bcd.h"		/* vbi3_is_bcd(), vbi3_bcd2dec() */
#include "conv.h"		/* _vbi3_strdup_locale_utf8() */
#include "network.h"
#include "network-table.h"

/**
 * @param type CNI type.
 *
 * Returns the name of a CNI type, for example VBI3_CNI_TYPE_PDC_A ->
 * "PDC_A". This is mainly intended for debugging.
 * 
 * @return
 * Static ASCII string, NULL if @a type is invalid.
 */
const char *
vbi3_cni_type_name		(vbi3_cni_type		type)
{
	switch (type) {

#undef CASE
#define CASE(type) case VBI3_CNI_TYPE_##type : return #type ;

	CASE (NONE)
	CASE (VPS)
	CASE (8301)
	CASE (8302)
	CASE (PDC_A)
	CASE (PDC_B)

	}

	return NULL;
}

static unsigned int
cni_pdc_a_to_vps		(unsigned int		cni)
{
	unsigned int n;

	/* Relation guessed from values observed
	   in DE and AT. Is this defined anywhere? */

	switch (cni >> 12) {
	case 0x1A: /* Austria */
	case 0x1D: /* Germany */
		if (!vbi3_is_bcd ((int) cni & 0xFFF))
			return 0;

		n = vbi3_bcd2dec ((int) cni & 0xFFF);

		switch (n) {
		case 100 ... 163:
			cni = ((cni >> 4) & 0xF00) + n + 0xC0 - 100;
			break;

		case 200 ... 263: /* in DE */
			cni = ((cni >> 4) & 0xF00) + n + 0x80 - 200;
			break;

		default:
			return 0;
		}

		break;

	default:
		return 0;
	}

	return cni;
}

static unsigned int
cni_vps_to_pdc_a		(unsigned int		cni)
{
	switch (cni >> 8) {
	case 0xA: /* Austria */
	case 0xD: /* Germany */
		switch (cni & 0xFF) {
		case 0xC0 ... 0xFF:
			cni = ((cni << 4) & 0xF000) + 0x10000
				+ vbi3_dec2bcd ((int)(cni & 0xFF) - 0xC0 + 100);
			break;

		case 0x80 ... 0xBF:
			cni = ((cni << 4) & 0xF000) + 0x10000
				+ vbi3_dec2bcd ((int)(cni & 0xFF) - 0x80 + 200);
			break;

		default:
			return 0;
		}

		break;

	default:
		return 0;
	}

	return cni;
}

static const struct network *
cni_lookup			(vbi3_cni_type		type,
				 unsigned int		cni,
				 const char *		caller)
{
	const struct network *p;
	const struct network *network_table_end =
		network_table + N_ELEMENTS (network_table);

	/* TODO binary search. */

	if (0 == cni)
		return NULL;

	switch (type) {
	case VBI3_CNI_TYPE_8301:
		for (p = network_table; p < network_table_end; ++p)
			if (p->cni_8301 == cni) {
				return p;
			}
		break;

	case VBI3_CNI_TYPE_8302:
		for (p = network_table; p < network_table_end; ++p)
			if (p->cni_8302 == cni) {
				return p;
			}

		cni &= 0x0FFF;

		/* fall through */

	case VBI3_CNI_TYPE_VPS:
		for (p = network_table; p < network_table_end; ++p)
			if (p->cni_vps == cni) {
				return p;
			}
		break;

	case VBI3_CNI_TYPE_PDC_A:
	{
		unsigned int cni_vps;

		cni_vps = cni_pdc_a_to_vps (cni);

		if (0 == cni_vps)
			return NULL;

		for (p = network_table; p < network_table_end; ++p)
			if (p->cni_vps == cni_vps) {
				return p;
			}

		break;
	}

	case VBI3_CNI_TYPE_PDC_B:
		for (p = network_table; p < network_table_end; ++p)
			if (p->cni_pdc_b == cni) {
				return p;
			}

		/* try code | 0x0080 & 0x0FFF -> VPS ? */

		break;

	default:
		vbi3_log_printf (VBI3_DEBUG, caller,
				"Unknown CNI type %u\n", type);
		break;
	}

	return NULL;
}

/**
 * @param to_type Type of CNI to convert to.
 * @param from_type Type of CNI to convert from.
 * @param cni CNI to convert.
 *
 * Converts a CNI from one type to another.
 *
 * @returns
 * Converted CNI or 0 if conversion is not possible.
 */
unsigned int
vbi3_convert_cni			(vbi3_cni_type		to_type,
				 vbi3_cni_type		from_type,
				 unsigned int		cni)
{
	const struct network *p;

	if (!(p = cni_lookup (from_type, cni, __FUNCTION__)))
		return 0;

	switch (to_type) {
	case VBI3_CNI_TYPE_VPS:
		return p->cni_vps;

	case VBI3_CNI_TYPE_8301:
		return p->cni_8301;

	case VBI3_CNI_TYPE_8302:
		return p->cni_8302;

	case VBI3_CNI_TYPE_PDC_A:
		return cni_vps_to_pdc_a (p->cni_vps);

	case VBI3_CNI_TYPE_PDC_B:
		return p->cni_pdc_b;

	default:
		vbi3_log_printf (VBI3_DEBUG, __FUNCTION__,
				"Unknown CNI type %u\n", to_type);
		break;
	}

	return 0;
}

/**
 * @internal
 */
void
_vbi3_network_dump		(const vbi3_network *	nk,
				 FILE *			fp)
{
	assert (NULL != nk);
	assert (NULL != fp);

	fprintf (fp,
		 "'%s' call_sign=%s cni=%x/%x/%x/%x/%x country=%s",
		 nk->name ? nk->name : "unknown",
		 nk->call_sign[0] ? nk->call_sign : "unknown",
		 nk->cni_vps,
		 nk->cni_8301,
		 nk->cni_8302,
		 nk->cni_pdc_a,
		 nk->cni_pdc_b,
		 nk->country_code[0] ? nk->country_code : "unknown");
}

/**
 */
char *
vbi3_network_id_string		(const vbi3_network *	nk)
{
	char buffer[sizeof (nk->call_sign) * 3 + 5 * 9 + 1];
	char *s;
	unsigned int i;

	s = buffer;

	for (i = 0; i < sizeof (nk->call_sign); ++i) {
		if (isalnum (nk->call_sign[i])) {
			*s++ = nk->call_sign[i];
		} else {
			s += sprintf (s, "%%%02x", nk->call_sign[i]);
		}
	}

	s += sprintf (s, "-%8x", nk->cni_vps);
	s += sprintf (s, "-%8x", nk->cni_8301);
	s += sprintf (s, "-%8x", nk->cni_8302);
	s += sprintf (s, "-%8x", nk->cni_pdc_a);
	s += sprintf (s, "-%8x", nk->cni_pdc_b);

	return strdup (buffer);
}

/**
 */
vbi3_bool
vbi3_network_set_name		(vbi3_network *		nk,
				 const char *		name)
{
	char *name1;

	assert (NULL != nk);

	if (!(name1 = strdup (name)))
		return FALSE;

	vbi3_free (nk->name);
	nk->name = name1;

	return TRUE;
}

/**
 * @internal
 */
vbi3_bool
_vbi3_network_set_name_from_ttx_header
				(vbi3_network *		nk,
				 const uint8_t		buffer[40])
{
	unsigned int i;

	assert (NULL != nk);
	assert (NULL != buffer);

	for (i = 0; i < N_ELEMENTS (ttx_header_table); ++i) {
		const uint8_t *s1;
		const uint8_t *s2;
		uint8_t c1;
		uint8_t c2;
		char *name;

		s1 = ttx_header_table[i].header;
		s2 = buffer + 8;

		while (0 != (c1 = *s1) && s2 < &buffer[40]) {
			switch (c1) {
			case '?':
				break;

			case '#':
				if (!isdigit (*s2 & 0x7F))
					goto next;
				break;

			default:
				if (((c2 = *s2) & 0x7F) <= 0x20) {
					if (0x20 != c1)
						goto next;
					else
						break;
				} else if (0 != ((c1 ^ c2) & 0x7F)) {
					goto next;
				}
				break;
			}

			++s1;
			++s2;
		}

		name = _vbi3_strdup_locale_utf8 (ttx_header_table[i].name);

		if (!name)
			return FALSE;

		vbi3_free (nk->name);
		nk->name = name;

		return TRUE;

	next:
		;
	}

	return FALSE;
}

/**
 */
vbi3_bool
vbi3_network_set_cni		(vbi3_network *		nk,
				 vbi3_cni_type		type,
				 unsigned int		cni)
{
	const struct network *p;
	char *name;

	assert (NULL != nk);

	switch (type) {
	case VBI3_CNI_TYPE_VPS:
		nk->cni_vps = cni;
		break;

	case VBI3_CNI_TYPE_8301:
		nk->cni_8301 = cni;
		break;

	case VBI3_CNI_TYPE_8302:
		nk->cni_8302 = cni;
		break;

	case VBI3_CNI_TYPE_PDC_A:
		nk->cni_pdc_a = cni;
		break;

	case VBI3_CNI_TYPE_PDC_B:
		nk->cni_pdc_b = cni;
		break;

	default:
		vbi3_log_printf (VBI3_DEBUG, __FUNCTION__,
				"Unknown CNI type %u\n", type);
	}

	if (!(p = cni_lookup (type, cni, __FUNCTION__)))
		return FALSE;

	/* Keep in mind our table may be wrong. */

	if (p->cni_vps && nk->cni_vps
	    && p->cni_vps != nk->cni_vps)
		return FALSE;

	if (p->cni_8301 && nk->cni_8301
	    && p->cni_8301 != nk->cni_8301)
		return FALSE;

	if (p->cni_8302 && nk->cni_8302
	    && p->cni_8302 != nk->cni_8302)
		return FALSE;

	if (!(name = _vbi3_strdup_locale_utf8 (p->name)))
		return FALSE;

	vbi3_free (nk->name);
	nk->name = name;

	nk->cni_vps	= p->cni_vps;
	nk->cni_8301	= p->cni_8301;
	nk->cni_8302	= p->cni_8302;

	if (0 == nk->cni_pdc_a)
		nk->cni_pdc_a = cni_vps_to_pdc_a (p->cni_vps);

	if (0 == nk->cni_pdc_b)
		nk->cni_pdc_b = p->cni_pdc_b;

	if (0 == nk->country_code[0]) {
		assert (p->country < N_ELEMENTS (country_table));

		_vbi3_strlcpy (nk->country_code,
			      country_table[p->country].country_code,
			      sizeof (nk->country_code));
	}

	return TRUE;
}

/**
 */
vbi3_bool
vbi3_network_set_call_sign	(vbi3_network *		nk,
				 const char *		call_sign)
{
	assert (NULL != nk);
	assert (NULL != call_sign);

	_vbi3_strlcpy (nk->call_sign, call_sign, sizeof (nk->call_sign));

	if (0 == nk->country_code[0]) {
		const char *country_code;

		country_code = "";

		/* See http://www.fcc.gov/cgb/statid.html */
		switch (call_sign[0]) {
		case 'A':
			switch (call_sign[1]) {
			case 'A' ... 'F':
				country_code = "US";
				break;
			}
			break;

		case 'K':
		case 'W':
		case 'N':
			country_code = "US";
			break;

		case 'C':
			switch (call_sign[1]) {
			case 'F' ... 'K':
			case 'Y' ... 'Z':
				country_code = "CA";
				break;
			}
			break;

		case 'V':
			switch (call_sign[1]) {
			case 'A' ... 'G':
			case 'O':
			case 'X' ... 'Y':
				country_code = "CA";
				break;
			}
			break;

		case 'X':
			switch (call_sign[1]) {
			case 'J' ... 'O':
				country_code = "CA";
				break;
			}
			break;
		}

		_vbi3_strlcpy (nk->country_code,
			      country_code,
			      sizeof (nk->country_code));
	}

	return TRUE;
}

/**
 * @param nk Initialized vbi3_network structure.
 *
 * @returns
 * @c TRUE if all fields user_data, cni_vps, cni_8301, cni_8302
 * and call_sign are unset (zero).
 */
vbi3_bool
vbi3_network_is_anonymous	(const vbi3_network *	nk)
{
	assert (NULL != nk);

	return (NULL == nk->user_data
		&& 0 == (nk->cni_vps |
			 nk->cni_8301 |
			 nk->cni_8302 |
			 nk->call_sign[0]));
}

/**
 * @param nk1 Initialized vbi3_network structure.
 * @param nk2 Initialized vbi3_network structure.
 *
 * Compares two networks for weak equality. Networks are considered
 * weak equal if each pair of user_data, cni_vps, cni_8301, cni_8302
 * and call_sign is equal or one value is unset (zero).
 *
 * @returns
 * @c TRUE if the networks are weakly equal.
 */
vbi3_bool
vbi3_network_weak_equal		(const vbi3_network *	nk1,
				 const vbi3_network *	nk2)
{
	assert (NULL != nk1);
	assert (NULL != nk2);

	if (nk1->user_data && nk2->user_data)
		if (nk1->user_data != nk2->user_data)
			return FALSE;

	if (nk1->cni_vps && nk2->cni_vps)
		if (nk1->cni_vps != nk2->cni_vps)
			return FALSE;

	if (nk1->cni_8301 && nk2->cni_8301)
		if (nk1->cni_8301 != nk2->cni_8301)
			return FALSE;

	if (nk1->cni_8302 && nk2->cni_8302)
		if (nk1->cni_8302 != nk2->cni_8302)
			return FALSE;

	if (nk1->call_sign[0] && nk2->call_sign[0])
		if (0 != strcmp (nk1->call_sign, nk2->call_sign))
			return FALSE;

	return TRUE;
}

/**
 * @param nk1 Initialized vbi3_network structure.
 * @param nk2 Initialized vbi3_network structure.
 *
 * Compares two networks for equality. Networks are considered equal
 * if each pair of user_data, cni_vps, cni_8301, cni_8302 and
 * call_sign is equal or both values are unset (zero).
 *
 * @returns
 * @c TRUE if the networks are equal.
 */
vbi3_bool
vbi3_network_equal		(const vbi3_network *	nk1,
				 const vbi3_network *	nk2)
{
	assert (NULL != nk1);
	assert (NULL != nk2);

	if (nk1->user_data || nk2->user_data)
		if (nk1->user_data != nk2->user_data)
			return FALSE;

	if (0 != ((nk1->cni_vps ^ nk2->cni_vps) |
		  (nk1->cni_8301 ^ nk2->cni_8301) |
		  (nk1->cni_8302 ^ nk2->cni_8302)))
		return FALSE;

	if (nk1->call_sign[0] || nk2->call_sign[0])
		if (0 != strcmp (nk1->call_sign, nk2->call_sign))
			return FALSE;

	return TRUE;
}

/**
 * @param nk Initialized vbi3_network structure.
 *
 * Resets all fields to zero, creating an anonymous network.
 */
void
vbi3_network_reset		(vbi3_network *		nk)
{
	assert (NULL != nk);

	vbi3_free (nk->name);

	CLEAR (*nk);
}

/**
 * @param nk vbi3_network structure initialized with vbi3_network_init()
 *   or vbi3_network_copy().
 *
 * Frees all resources associated with @a nk, except the structure itself.
 */
void
vbi3_network_destroy		(vbi3_network *		nk)
{
	vbi3_network_reset (nk);
}

/**
 * @param dst Initialized vbi3_network structure.
 * @param src Initialized vbi3_network structure, can be @c NULL.
 *
 * Sets all fields of @a dst by copying from @a src. Does
 * nothing if @a dst and @a src are the same.
 *
 * @returns
 * @c FALSE on failure (out of memory), with @a dst unmodified.
 */
vbi3_bool
vbi3_network_set			(vbi3_network *		dst,
				 const vbi3_network *	src)
{
	assert (NULL != dst);

	if (dst == src)
		return TRUE;

	if (!src) {
		vbi3_network_reset (dst);
	} else {
		char *name;

		name = NULL;

		if (src->name && !(name = strdup (src->name)))
			return FALSE;

		vbi3_free (dst->name);

		memcpy (dst, src, sizeof (*dst));

		dst->name = name;
	}

	return TRUE;
}

/**
 * @param dst vbi3_network structure to initialize.
 * @param src Initialized vbi3_network structure, can be @c NULL.
 *
 * Initializes all fields of @a dst by copying from @a src.
 * Does nothing if @a dst and @a src are the same.
 *
 * @returns
 * @c FALSE on failure (out of memory), with @a dst unmodified.
 */
vbi3_bool
vbi3_network_copy		(vbi3_network *		dst,
				 const vbi3_network *	src)
{
	assert (NULL != dst);

	if (dst == src)
		return TRUE;

	if (!src) {
		CLEAR (*dst);
	} else if (dst != src) {
		char *name;

		name = NULL;

		if (src->name && !(name = strdup (src->name)))
			return FALSE;

		memcpy (dst, src, sizeof (*dst));

		dst->name = name;
	}

	return TRUE;
}

/**
 * @param nk vbi3_network structure to initialize.
 *
 * Initializes all fields of @a nk to zero, creating an
 * anonymous network.
 *
 * @returns
 * @c TRUE.
 */
vbi3_bool
vbi3_network_init		(vbi3_network *		nk)
{
	return vbi3_network_copy (nk, NULL);
}

/**
 * @param nk malloc()ed array of initialized vbi3_network structures,
 *   can be NULL.
 * @param n_elements Number of elements in the array.
 * 
 * Frees an array of vbi3_network structures and all data associated
 * with them. Does nothing if @a nk or @a n_elements are zero.
 */
extern void
vbi3_network_array_delete	(vbi3_network *		nk,
				 unsigned int		n_elements)
{
	unsigned int i;

	if (NULL == nk || 0 == n_elements)
		return;

	for (i = 0; i < n_elements; ++i)
		vbi3_network_destroy (nk + i);

	vbi3_free (nk);
}