/*
 *  MPEG-1 Real Time Encoder
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

/* $Id: dct.h,v 1.2 2000-09-25 17:08:57 mschimek Exp $ */

#define reg(n) __attribute__ ((regparm (n)))

extern void		fdct_intra(void);
extern unsigned int	fdct_inter(short iblock[6][8][8]) reg(1);
extern void		mpeg1_idct_intra(void);
extern void		mpeg1_idct_inter(unsigned int cbp) reg(1);
extern void		new_intra_quant(int quant_scale) reg(1);
extern void		new_inter_quant(int quant_scale) reg(1);

extern void		mmx_fdct_intra(void);
extern unsigned int	mmx_fdct_inter(short iblock[6][8][8]) reg(1);
extern void		mmx_mpeg1_idct_intra(void);
extern void		mmx_mpeg1_idct_inter(unsigned int cbp) reg(1);
extern void		mmx_new_intra_quant(int quant_scale) reg(1);
extern void		mmx_new_inter_quant(int quant_scale) reg(1);

extern void		mmx_copy_refblock(void);
