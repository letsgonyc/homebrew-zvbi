/*
 *  Copyright (C) 2004 Michael H. Schimek
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

/* $Id: memcpy.c,v 1.1 2005-01-08 14:43:22 mschimek Exp $ */

#undef NDEBUG

#include <inttypes.h>
#include <string.h>
#include "libtv/image_format.h"

#include "guard.h"

#include "src/cpu.c"

static uint8_t *		sbuffer;
static uint8_t *		sbuffer_end;
static uint8_t *		dbuffer;
static uint8_t *		dbuffer_end;

static void
test1				(uint8_t *		dst,
				 uint8_t *		src,
				 unsigned int		n_bytes)
{
	unsigned int i;
	uint8_t *p;

	memset (dbuffer, 0xAA, dbuffer_end - dbuffer);

	for (i = 0; i < sbuffer_end - sbuffer; ++i)
		sbuffer[i] = i;

	tv_memcpy (dst, src, n_bytes);

	for (p = dbuffer; p < dst;)
		assert (0xAA == *p++);

	for (i = 0; i < n_bytes; ++i)
		assert (*p++ == *src++);

	while (p < dbuffer_end)
		assert (0xAA == *p++);
}

static void
test				(const char *		name)
{
	unsigned int i;

	fprintf (stderr, "%s\n", name);

	for (i = 0; i < 33; ++i) {
		test1 (dbuffer, sbuffer, i);
		test1 (dbuffer, sbuffer, i + 16384);
		test1 (dbuffer_end - i, sbuffer_end - i, i);
		test1 (dbuffer_end - i - 16384,
		       sbuffer_end - i - 16384, i + 16384);
		test1 (dbuffer + i, sbuffer + (i ^ 31), i * 3);
	}
}

int
main				(int			argc,
				 char **		argv)
{
	unsigned int buffer_size;
	cpu_feature_set features;

	(void) argc;
	(void) argv;

	buffer_size = 64 << 10;

	sbuffer = guard_alloc (buffer_size);
	sbuffer_end = sbuffer + buffer_size;

	dbuffer = guard_alloc (buffer_size);
	dbuffer_end = dbuffer + buffer_size;

	features = cpu_detection ();

	cpu_features = 0;
	test ("memcpy generic");

	if (features & CPU_FEATURE_MMX) {
		cpu_features = CPU_FEATURE_MMX;
		test ("memcpy mmx");
	}

	if (features & CPU_FEATURE_SSE) {
		cpu_features = CPU_FEATURE_SSE;
		test ("memcpy sse");
	}

	return EXIT_SUCCESS;
}