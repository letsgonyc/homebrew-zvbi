/*
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

/* $Id: types.h,v 1.2 2001-08-22 01:28:08 mschimek Exp $ */

#ifndef TYPES_H
#define TYPES_H

#include <stddef.h>

#undef TRUE
#undef FALSE

enum { FALSE, TRUE };

typedef unsigned char bool;

#define PARENT(ptr, type, member) \
	((type *)(((char *) ptr) - offsetof(type, member)))

/*
 *  Same as libc assert, but also reports the caller.
 */
#ifdef	NDEBUG
# define asserts(expr) ((void) 0)
#else
extern void asserts_fail(char *assertion, char *file, unsigned int line, char *function, void *caller);
#define asserts(expr)							\
	((void) ((expr) ? 0 : asserts_fail(#expr, __FILE__, __LINE__,	\
		__PRETTY_FUNCTION__, __builtin_return_address(0))))
#endif

#endif /* TYPES_H */
