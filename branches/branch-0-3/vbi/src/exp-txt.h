/*
 *  libzvbi - Text export functions
 *
 *  Copyright (C) 2001 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998, 1999 Edgar Toernig <froese@gmx.de>
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

/* $Id: exp-txt.h,v 1.5.2.3 2004-02-25 17:35:28 mschimek Exp $ */

#ifndef EXP_TXT_H
#define EXP_TXT_H

#include <stdarg.h>
#include "format.h"

/* Public */

/**
 * @addtogroup Render
 * @{
 */
extern unsigned int
vbi_print_page_region_va_list	(vbi_page *		pg,
				 char *			buffer,
				 unsigned int		buffer_size,
				 const char *		format,
				 const char *		separator,
				 unsigned int		separator_size,
				 unsigned int		column,
				 unsigned int		row,
				 unsigned int		width,
				 unsigned int		height,
				 va_list		export_options);
extern unsigned int
vbi_print_page_region		(vbi_page *		pg,
				 char *			buffer,
				 unsigned int		buffer_size,
				 const char *		format,
				 const char *		separator,
				 unsigned int		separator_size,
				 unsigned int		column,
				 unsigned int		row,
				 unsigned int		width,
				 unsigned int		height,
				 ...);
extern unsigned int
vbi_print_page_va_list		(vbi_page *		pg,
				 char *			buffer,
				 unsigned int		buffer_size,
				 const char *		format,
				 va_list		export_options);
extern unsigned int
vbi_print_page			(vbi_page *		pg,
				 char *			buffer,
				 unsigned int		buffer_size,
				 const char *		format,
				 ...);
/** @} */

/* Private */

#endif /* EXP_TXT_H */
