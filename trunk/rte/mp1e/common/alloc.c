/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
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

/* $Id: alloc.c,v 1.7 2002-12-14 00:43:44 mschimek Exp $ */

#include <stdlib.h>
#include "math.h"
#include "alloc.h"

#ifndef HAVE_MEMALIGN

void *
alloc_aligned(size_t size, int align, rte_bool clear)
{
	void *p, *b;

	if (align < sizeof(void *))
		align = sizeof(void *);

	if (!(b = malloc(size + align)))
		return NULL;

	p = (void *)(((long)((char *) b + align)) & -align);

	((void **) p)[-1] = b;

	if (clear)
		memset(p, 0, size);

	return p;
}

#endif // !HAVE_MEMALIGN