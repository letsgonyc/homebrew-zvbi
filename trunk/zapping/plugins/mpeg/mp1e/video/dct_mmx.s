#
#  MPEG-1 Real Time Encoder
# 
#  Copyright (C) 1999-2000 Michael H. Schimek
# 
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
# 
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
# 
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#

# $Id: dct_mmx.s,v 1.3 2000-09-29 17:54:33 mschimek Exp $

	.text
	.align		16
	.globl		mmx_fdct_intra

mmx_fdct_intra:

	pushl		%esi;				movzbl		ltp(%eax),%esi;
	pushl		%ecx;				sall		$7,%esi;
	movzbl		mmx_q_fdct_intra_sh(%eax),%eax;	xorl		%ecx,%ecx;
	movl		%eax,csh;			decl		%eax;
	bts		%eax,%ecx;			addl		$mmx_q_fdct_intra_q_lut,%esi;
	pushl		%ebx;				addl		$16,%eax;
	bts		%eax,%ecx;			movl		$mblock,%eax;
	movl		%ecx,crnd;			movl		$mblock+1536,%ebx;
	movl		%ecx,crnd+4;			movl		$6,%ecx;

	.align		16
1:
	movq		7*16+0(%eax),%mm0;		movq		(%eax),%mm7;
	movq		1*16+0(%eax),%mm6;		decl		%ecx;
	movq		6*16+0(%eax),%mm1;		paddw		%mm7,%mm0;
	movq		2*16+0(%eax),%mm5;		paddw		%mm7,%mm7;
	movq		5*16+0(%eax),%mm2;		psubw		%mm0,%mm7;
	movq		3*16+0(%eax),%mm4;		paddw		%mm6,%mm1;
	movq		4*16+0(%eax),%mm3;		paddw		%mm6,%mm6;
	paddw		%mm5,%mm2;			psubw		%mm1,%mm6;
	paddw		%mm4,%mm3;			paddw		%mm4,%mm4;
	paddw		%mm5,%mm5;			psubw		%mm3,%mm4;
	psubw		%mm2,%mm5;			psubw		%mm2,%mm1;
	psubw		%mm3,%mm0;			paddw		%mm2,%mm2;
	paddw		%mm1,%mm2;			paddw		%mm3,%mm3;
	paddw		%mm0,%mm3;			paddw		%mm0,%mm1;
	paddw		%mm3,%mm2;			psllw		$3,%mm1;
	paddw		%mm5,%mm4;			psllw		$4,%mm3;
	paddw		%mm6,%mm5;			psllw		$3,%mm2;
	pmulhw		cC4_15,%mm1;			psllw		$3,%mm5;
	pmulhw		cC4C6_14,%mm5;			psubw		%mm2,%mm3;
	movq		%mm3,4*16+0(%ebx);		psllw		$2,%mm0;
	paddw		%mm7,%mm6;			psllw		$4,%mm7;
	paddw		%mm0,%mm1;			movq		%mm6,%mm3;
	pmulhw		c1C6_13,%mm7;			paddw		%mm0,%mm0;
	psubw		%mm1,%mm0;			psllw		$3,%mm3;
	movq		%mm0,6*16+0(%ebx);		paddw		%mm4,%mm6;
	pmulhw		cC4_15,%mm3;			movq		%mm4,%mm0;
	paddw		%mm6,%mm6;			psllw		$3,%mm0;
	paddw		%mm7,%mm5; 			psllw		$2,%mm4;
	pmulhw		cC4_15,%mm0;			paddw		%mm7,%mm7;
	psubw		%mm6,%mm4;	 		psubw		%mm5,%mm7;
	paddw		%mm7,%mm4;			paddw		%mm7,%mm7;
	paddw		%mm0,%mm4;			paddw		%mm3,%mm6;
	psubw		c128_6,%mm2;			paddw		%mm5,%mm6;
	psubw		%mm4,%mm7;			paddw		%mm5,%mm5;
	movq		%mm2,%mm0;			punpcklwd	%mm6,%mm2;
	psubw		%mm6,%mm5;			punpckhwd	%mm6,%mm0;
	movq		%mm1,%mm6;			punpcklwd	%mm7,%mm1;
	movq		c1,%mm3;			punpckhwd	%mm7,%mm6;
	movq		%mm2,%mm7;			punpckldq	%mm1,%mm2;
        movq		%mm2,(%ebx);			punpckhdq	%mm1,%mm7;
	movq		%mm0,%mm2;			punpckldq	%mm6,%mm0;
	paddw		%mm3,%mm7;			punpckhdq	%mm6,%mm2;
	movq		4*16+0(%ebx),%mm6;		paddw		%mm3,%mm0;
        movq		%mm7,1*16+0(%ebx);		paddw		%mm3,%mm2;
	movq		%mm6,%mm1;			punpcklwd	%mm4,%mm6;
	movq		6*16+0(%ebx),%mm7;		punpckhwd	%mm4,%mm1;
	movq		%mm7,%mm4;			punpcklwd	%mm5,%mm7;
	movq		%mm0,2*16+0(%ebx);		punpckhwd	%mm5,%mm4;
	movq		%mm6,%mm0;			punpckldq	%mm7,%mm6;
	movq		%mm2,3*16+0(%ebx);		punpckhdq	%mm7,%mm0;
	movq		%mm1,%mm7;			punpckldq	%mm4,%mm1;
	movq		%mm6,0*16+8(%ebx);		paddw		%mm3,%mm0;
	movq		6*16+8(%eax),%mm2;		punpckhdq	%mm4,%mm7;
	movq		%mm0,1*16+8(%ebx);		paddw		%mm3,%mm1;
	movq		1*16+8(%eax),%mm6;		paddw		%mm3,%mm7;
	movq		%mm1,2*16+8(%ebx);		paddw		%mm6,%mm2;
	movq		2*16+8(%eax),%mm5;		paddw		%mm6,%mm6;
	movq		5*16+8(%eax),%mm1; 		psubw		%mm2,%mm6;
	movq		%mm7,3*16+8(%ebx);		paddw		%mm5,%mm1;
	movq		0*16+8(%eax),%mm7;		paddw		%mm5,%mm5;
	movq		7*16+8(%eax),%mm0; 		psubw		%mm1,%mm5;
	movq		4*16+8(%eax),%mm3; 		paddw		%mm7,%mm0;
	movq		3*16+8(%eax),%mm4;		paddw		%mm7,%mm7;
	psubw		%mm0,%mm7;			paddw		%mm2,%mm1;
	paddw		%mm4,%mm3; 			paddw		%mm4,%mm4;
	psubw		%mm3,%mm4;			paddw		%mm0,%mm3;
	paddw		%mm0,%mm0; 			paddw		%mm2,%mm2;
	psubw		%mm3,%mm0;			psubw		%mm1,%mm2;
	paddw		%mm0,%mm2; 			psllw		$2,%mm0;
	paddw		%mm3,%mm1; 			psllw		$3,%mm2;
	pmulhw		cC4_15,%mm2;			psllw		$3,%mm1;
	paddw		%mm5,%mm4; 			psllw		$4,%mm3;
	psubw		%mm1,%mm3;			paddw		%mm6,%mm5;
	paddw		%mm7,%mm6;			psllw		$3,%mm5;
	movq		%mm3,4*16+8(%ebx); 		psllw		$4,%mm7;
	pmulhw		cC4C6_14,%mm5;			paddw		%mm0,%mm2;
	paddw		%mm0,%mm0; 			psubw		%mm2,%mm0;
	pmulhw		c1C6_13,%mm7;			movq		%mm6,%mm3;
	movq		%mm0,6*16+8(%ebx);		psllw		$3,%mm3;
	pmulhw		cC4_15,%mm3;			movq		%mm4,%mm0;
	paddw		%mm4,%mm6; 			psllw		$3,%mm0;
	pmulhw		cC4_15,%mm0;			paddw		%mm6,%mm6;
	paddw		%mm7,%mm5;			paddw		%mm7,%mm7;
	psubw		%mm5,%mm7;			psllw		$2,%mm4;
	psubw		c128_6,%mm1;			psubw		%mm6,%mm4;
	paddw		%mm3,%mm6;			paddw		%mm7,%mm4;
	paddw		%mm0,%mm4;			paddw		%mm7,%mm7;
	psubw		%mm4,%mm7;			paddw		%mm5,%mm6;
	movq		%mm1,%mm0;			punpcklwd	%mm6,%mm1;
	paddw		%mm5,%mm5; 			punpckhwd	%mm6,%mm0;
	movq		%mm2,%mm3;			punpcklwd	%mm7,%mm2;
	psubw		%mm6,%mm5;			punpckhwd	%mm7,%mm3;
	movq		%mm1,%mm6;			punpckldq	%mm2,%mm1;
	movq		%mm0,%mm7;			punpckldq	%mm3,%mm0;
	movq		%mm1,4*16+0(%ebx);		punpckhdq	%mm2,%mm6;
	movq		4*16+8(%ebx),%mm2;		punpckhdq	%mm3,%mm7;
	movq		%mm0,6*16+0(%ebx);		movq		%mm2,%mm0;
	movq		6*16+8(%ebx),%mm1;		punpcklwd	%mm4,%mm2;
	movq		%mm6,5*16+0(%ebx);		punpckhwd	%mm4,%mm0;
	movq		%mm1,%mm3;			punpcklwd	%mm5,%mm1;
	movq		%mm7,7*16+0(%ebx);    		punpckhwd	%mm5,%mm3;
	movq		%mm2,%mm4;			punpckldq	%mm1,%mm2;
	movq		1*16+8(%ebx),%mm6;    		punpckhdq	%mm1,%mm4;
	movq		%mm0,%mm5;			punpckldq	%mm3,%mm5;
	movq		0*16+8(%ebx),%mm7;		punpckhdq	%mm3,%mm0;
	movq		3*16+8(%ebx),%mm1;		paddw		%mm6,%mm5;
	movq		2*16+8(%ebx),%mm3;		paddw		%mm6,%mm6;
	paddw		%mm7,%mm0; 			psubw		%mm5,%mm6;
	paddw		%mm7,%mm7; 			paddw		%mm1,%mm2;
	psubw		%mm0,%mm7;			paddw		%mm1,%mm1;
	paddw		%mm3,%mm4; 			paddw		%mm3,%mm3;
	psubw		%mm2,%mm1;			psubw		%mm4,%mm3;
	paddw		%mm0,%mm2; 			paddw		%mm0,%mm0;
	paddw		%mm5,%mm4; 			paddw		%mm5,%mm5;
	psubw		%mm2,%mm0;			psraw		$1,%mm2;
	psubw		%mm4,%mm5;			psraw		$1,%mm4;
	paddw		%mm2,%mm4;			paddw		%mm2,%mm2;
	movq		%mm0,0*16+8(%ebx);		psubw		%mm4,%mm2;
	pmulhw		0*16+8(%esi),%mm4;		paddw		%mm3,%mm1;
	movq		crnd,%mm0;			paddw		%mm6,%mm3;
	pmulhw		4*16+8(%esi),%mm2;		paddw		%mm0,%mm4;
	psraw		csh,%mm4;			paddw		%mm7,%mm6;
	pmulhw		cC4C6_14,%mm3;			paddw		%mm0,%mm2;
        movq		%mm4,0*16+8+768(%eax);		movq		%mm1,%mm4;
        psraw		csh,%mm2;	        	psraw		$3,%mm1; 
	movq		%mm2,4*16+8+768(%eax);		movq		%mm6,%mm2;
	pmulhw		c1C6_13,%mm7;			psraw		$3,%mm6;
	pmulhw		cC2C61_13,%mm4;			paddw		%mm1,%mm6;
	pmulhw		cC2C61_13,%mm2;			paddw		%mm1,%mm1;
	psubw		%mm6,%mm1;			psraw		$1,%mm3; // red
	paddw		%mm4,%mm1; 			paddw		%mm7,%mm3;
	paddw		%mm2,%mm6;			paddw		%mm7,%mm7;
	psubw		%mm3,%mm7;			paddw		%mm3,%mm6;
	movq		csh,%mm4;			psraw		$1,%mm5;
	paddw		%mm7,%mm1; 			paddw		%mm7,%mm7;
	psubw		%mm1,%mm7;			paddw		%mm3,%mm3;
	movq		0*16+8(%ebx),%mm2;		psubw		%mm6,%mm3;
	pmulhw		5*16+8(%esi),%mm1;		psraw		$1,%mm2;
	pmulhw		3*16+8(%esi),%mm7;		paddw		%mm2,%mm5;
	pmulhw		cC4_15,%mm5; 			psraw		$1,%mm2;
	pmulhw		7*16+8(%esi),%mm3; 		paddw		%mm0,%mm1;
	pmulhw		1*16+8(%esi),%mm6;		psraw		%mm4,%mm1;
	paddw		%mm2,%mm5; 			paddw		%mm2,%mm2;
	movq		%mm1,5*16+8+768(%eax);		psubw		%mm5,%mm2;
	pmulhw		6*16+8(%esi),%mm2;		paddw		%mm0,%mm7;
	psraw		%mm4,%mm7;			paddw		%mm0,%mm3;
	pmulhw		2*16+8(%esi),%mm5;		paddw		%mm0,%mm6;
	paddw		%mm0,%mm2;			psraw		%mm4,%mm6;
	movq		%mm7,3*16+8+768(%eax);		psraw		%mm4,%mm3;
	movq		%mm3,7*16+8+768(%eax);		psraw		%mm4,%mm2;
	movq		7*16+0(%ebx),%mm1; 		paddw		%mm0,%mm5;
	movq		(%ebx),%mm7;			psraw		%mm4,%mm5;
	movq		%mm6,1*16+8+768(%eax);		paddw		%mm7,%mm1;
	movq		2*16+0(%ebx),%mm3;		paddw		%mm7,%mm7;
	movq		5*16+0(%ebx),%mm4; 		psubw		%mm1,%mm7;
	movq		%mm2,6*16+8+768(%eax);		paddw		%mm3,%mm4;
	movq		3*16+0(%ebx),%mm0;		paddw		%mm3,%mm3;
	movq		4*16+0(%ebx),%mm2; 		psubw		%mm4,%mm3;
	movq		%mm5,2*16+8+768(%eax);		paddw		%mm0,%mm2;
	movq		1*16+0(%ebx),%mm6;		paddw		%mm0,%mm0;
	movq		6*16+0(%ebx),%mm5; 		psubw		%mm2,%mm0;
	paddw		%mm1,%mm2; 			paddw		%mm1,%mm1;
	paddw		%mm6,%mm5; 			paddw		%mm6,%mm6;
	psubw		%mm5,%mm6;			psubw		%mm2,%mm1;
	paddw		%mm5,%mm4; 			paddw		%mm5,%mm5;
	psubw		%mm4,%mm5;			psraw		$1,%mm4; 
	movq		%mm5,(%ebx);			psraw		$1,%mm2;
	paddw		%mm2,%mm4; 			paddw		%mm2,%mm2;
	psubw		%mm4,%mm2;			psraw		$1,%mm1;
	pmulhw		4*16+0(%esi),%mm2;		movq		%mm4,%mm5;
	pmulhw		(%esi),%mm4;			paddw		%mm3,%mm0;
	paddw		c128,%mm5;			paddw		%mm6,%mm3;
	paddw		crnd,%mm2;			psraw		$8,%mm5;
	psraw		csh,%mm2;			psllq		$48,%mm5;
	paddw		crnd,%mm4;			psrlq		$48,%mm5;
	movq		%mm2,4*16+0+768(%eax);		paddw		%mm7,%mm6;
	psraw		csh,%mm4;			movq		%mm6,%mm2;
	pmulhw		cC2C61_13,%mm2;			por		%mm5,%mm4;
	pmulhw		cC4C6_14,%mm3;			psraw		$3,%mm6;
	movq		%mm4,768(%eax);			movq		%mm0,%mm4;
	pmulhw		c1C6_13,%mm7;			psraw		$3,%mm0; 
	pmulhw		cC2C61_13,%mm4;			paddw		%mm0,%mm6;
	paddw		%mm0,%mm0; 			psraw		$1,%mm3; // red
	paddw		%mm7,%mm3; 			psubw		%mm6,%mm0;
	paddw		%mm4,%mm0; 			paddw		%mm7,%mm7;
	paddw		%mm2,%mm6;			psubw		%mm3,%mm7;
	movq		crnd,%mm5;			paddw		%mm7,%mm0;
	paddw		%mm7,%mm7; 			leal		128(%eax),%eax;
	movq		(%ebx),%mm4;			paddw		%mm3,%mm6;
	psubw		%mm0,%mm7;			paddw		%mm3,%mm3;
	pmulhw		5*16+0(%esi),%mm0;		psraw		$1,%mm4;
	movq		csh,%mm2;			paddw		%mm1,%mm4;
	pmulhw		cC4_15,%mm4;			psubw		%mm6,%mm3;
	pmulhw		3*16+0(%esi),%mm7;		psraw		$1,%mm1;
	pmulhw		1*16+0(%esi),%mm6;		paddw		%mm1,%mm4;
	pmulhw		7*16+0(%esi),%mm3;		paddw		%mm1,%mm1;
	psubw		%mm4,%mm1;			paddw		%mm5,%mm0;
	pmulhw		2*16+0(%esi),%mm4;		psraw		%mm2,%mm0;
	pmulhw		6*16+0(%esi),%mm1;		paddw		%mm5,%mm7;
	paddw		%mm5,%mm6;			psraw		%mm2,%mm7;
	movq		%mm0,5*16-128+768(%eax);	psraw		%mm2,%mm6;
	movq		%mm7,3*16-128+768(%eax);	paddw		%mm5,%mm3;
	movq		%mm6,1*16-128+768(%eax);	psraw		%mm2,%mm3;
	paddw		%mm5,%mm4;			psraw		%mm2,%mm4;
	movq		%mm3,7*16-128+768(%eax);	paddw		%mm5,%mm1;
	movq		%mm4,2*16-128+768(%eax);	psraw		%mm2,%mm1;
	movq		%mm1,6*16-128+768(%eax);	jne		1b;

	popl		%ebx;			
	popl		%ecx;				
	popl		%esi;
	ret






OLD_mmx_fdct_intra:

	leal		-16(%esp),%esp;
	movl		%esi,12(%esp);			movl		$mblock,%esi;
	movl		%ebx,8(%esp);			movl		$mblock+1536,%ebx;
	movl		%eax,4(%esp);			movl		qfiql,%eax;
	movl		%ecx,(%esp);			movl		$6,%ecx;

	.align		16
1:
	movq		7*16+0(%esi),%mm0;		movq		(%esi),%mm7;
	movq		1*16+0(%esi),%mm6;		decl		%ecx;
	movq		6*16+0(%esi),%mm1;		paddw		%mm7,%mm0;
	movq		2*16+0(%esi),%mm5;		paddw		%mm7,%mm7;
	movq		5*16+0(%esi),%mm2;		psubw		%mm0,%mm7;
	movq		3*16+0(%esi),%mm4;		paddw		%mm6,%mm1;
	movq		4*16+0(%esi),%mm3;		paddw		%mm6,%mm6;
	paddw		%mm5,%mm2;			psubw		%mm1,%mm6;
	paddw		%mm4,%mm3;			paddw		%mm4,%mm4;
	paddw		%mm5,%mm5;			psubw		%mm3,%mm4;
	psubw		%mm2,%mm5;			psubw		%mm2,%mm1;
	psubw		%mm3,%mm0;			paddw		%mm2,%mm2;
	paddw		%mm1,%mm2;			paddw		%mm3,%mm3;
	paddw		%mm0,%mm3;			paddw		%mm0,%mm1;
	paddw		%mm3,%mm2;			psllw		$3,%mm1;
	paddw		%mm5,%mm4;			psllw		$4,%mm3;
	paddw		%mm6,%mm5;			psllw		$3,%mm2;
	pmulhw		cC4_15,%mm1;			psllw		$3,%mm5;
	pmulhw		cC4C6_14,%mm5;			psubw		%mm2,%mm3;
	movq		%mm3,4*16+0(%ebx);		psllw		$2,%mm0;
	paddw		%mm7,%mm6;			psllw		$4,%mm7;
	paddw		%mm0,%mm1;			movq		%mm6,%mm3;
	pmulhw		c1C6_13,%mm7;			paddw		%mm0,%mm0;
	psubw		%mm1,%mm0;			psllw		$3,%mm3;
	movq		%mm0,6*16+0(%ebx);		paddw		%mm4,%mm6;
	pmulhw		cC4_15,%mm3;			movq		%mm4,%mm0;
	paddw		%mm6,%mm6;			psllw		$3,%mm0;
	paddw		%mm7,%mm5; 			psllw		$2,%mm4;
	pmulhw		cC4_15,%mm0;			paddw		%mm7,%mm7;
	psubw		%mm6,%mm4;	 		psubw		%mm5,%mm7;
	paddw		%mm7,%mm4;			paddw		%mm7,%mm7;
	paddw		%mm0,%mm4;			paddw		%mm3,%mm6;
	psubw		c128_6,%mm2;			paddw		%mm5,%mm6;
	psubw		%mm4,%mm7;			paddw		%mm5,%mm5;
	movq		%mm2,%mm0;			punpcklwd	%mm6,%mm2;
	psubw		%mm6,%mm5;			punpckhwd	%mm6,%mm0;
	movq		%mm1,%mm6;			punpcklwd	%mm7,%mm1;
	movq		c1,%mm3;			punpckhwd	%mm7,%mm6;
	movq		%mm2,%mm7;			punpckldq	%mm1,%mm2;
        movq		%mm2,(%ebx);			punpckhdq	%mm1,%mm7;
	movq		%mm0,%mm2;			punpckldq	%mm6,%mm0;
	paddw		%mm3,%mm7;			punpckhdq	%mm6,%mm2;
	movq		4*16+0(%ebx),%mm6;		paddw		%mm3,%mm0;
        movq		%mm7,1*16+0(%ebx);		paddw		%mm3,%mm2;
	movq		%mm6,%mm1;			punpcklwd	%mm4,%mm6;
	movq		6*16+0(%ebx),%mm7;		punpckhwd	%mm4,%mm1;
	movq		%mm7,%mm4;			punpcklwd	%mm5,%mm7;
	movq		%mm0,2*16+0(%ebx);		punpckhwd	%mm5,%mm4;
	movq		%mm6,%mm0;			punpckldq	%mm7,%mm6;
	movq		%mm2,3*16+0(%ebx);		punpckhdq	%mm7,%mm0;
	movq		%mm1,%mm7;			punpckldq	%mm4,%mm1;
	movq		%mm6,0*16+8(%ebx);		paddw		%mm3,%mm0;
	movq		6*16+8(%esi),%mm2;		punpckhdq	%mm4,%mm7;
	movq		%mm0,1*16+8(%ebx);		paddw		%mm3,%mm1;
	movq		1*16+8(%esi),%mm6;		paddw		%mm3,%mm7;
	movq		%mm1,2*16+8(%ebx);		paddw		%mm6,%mm2;
	movq		2*16+8(%esi),%mm5;		paddw		%mm6,%mm6;
	movq		5*16+8(%esi),%mm1; 		psubw		%mm2,%mm6;
	movq		%mm7,3*16+8(%ebx);		paddw		%mm5,%mm1;
	movq		0*16+8(%esi),%mm7;		paddw		%mm5,%mm5;
	movq		7*16+8(%esi),%mm0; 		psubw		%mm1,%mm5;
	movq		4*16+8(%esi),%mm3; 		paddw		%mm7,%mm0;
	movq		3*16+8(%esi),%mm4;		paddw		%mm7,%mm7;
	psubw		%mm0,%mm7;			paddw		%mm2,%mm1;
	paddw		%mm4,%mm3; 			paddw		%mm4,%mm4;
	psubw		%mm3,%mm4;			paddw		%mm0,%mm3;
	paddw		%mm0,%mm0; 			paddw		%mm2,%mm2;
	psubw		%mm3,%mm0;			psubw		%mm1,%mm2;
	paddw		%mm0,%mm2; 			psllw		$2,%mm0;
	paddw		%mm3,%mm1; 			psllw		$3,%mm2;
	pmulhw		cC4_15,%mm2;			psllw		$3,%mm1;
	paddw		%mm5,%mm4; 			psllw		$4,%mm3;
	psubw		%mm1,%mm3;			paddw		%mm6,%mm5;
	paddw		%mm7,%mm6;			psllw		$3,%mm5;
	movq		%mm3,4*16+8(%ebx); 		psllw		$4,%mm7;
	pmulhw		cC4C6_14,%mm5;			paddw		%mm0,%mm2;
	paddw		%mm0,%mm0; 			psubw		%mm2,%mm0;
	pmulhw		c1C6_13,%mm7;			movq		%mm6,%mm3;
	movq		%mm0,6*16+8(%ebx);		psllw		$3,%mm3;
	pmulhw		cC4_15,%mm3;			movq		%mm4,%mm0;
	paddw		%mm4,%mm6; 			psllw		$3,%mm0;
	pmulhw		cC4_15,%mm0;			paddw		%mm6,%mm6;
	paddw		%mm7,%mm5;			paddw		%mm7,%mm7;
	psubw		%mm5,%mm7;			psllw		$2,%mm4;
	psubw		c128_6,%mm1;			psubw		%mm6,%mm4;
	paddw		%mm3,%mm6;			paddw		%mm7,%mm4;
	paddw		%mm0,%mm4;			paddw		%mm7,%mm7;
	psubw		%mm4,%mm7;			paddw		%mm5,%mm6;
	movq		%mm1,%mm0;			punpcklwd	%mm6,%mm1;
	paddw		%mm5,%mm5; 			punpckhwd	%mm6,%mm0;
	movq		%mm2,%mm3;			punpcklwd	%mm7,%mm2;
	psubw		%mm6,%mm5;			punpckhwd	%mm7,%mm3;
	movq		%mm1,%mm6;			punpckldq	%mm2,%mm1;
	movq		%mm0,%mm7;			punpckldq	%mm3,%mm0;
	movq		%mm1,4*16+0(%ebx);		punpckhdq	%mm2,%mm6;
	movq		4*16+8(%ebx),%mm2;		punpckhdq	%mm3,%mm7;
	movq		%mm0,6*16+0(%ebx);		movq		%mm2,%mm0;
	movq		6*16+8(%ebx),%mm1;		punpcklwd	%mm4,%mm2;
	movq		%mm6,5*16+0(%ebx);		punpckhwd	%mm4,%mm0;
	movq		%mm1,%mm3;			punpcklwd	%mm5,%mm1;
	movq		%mm7,7*16+0(%ebx);    		punpckhwd	%mm5,%mm3;
	movq		%mm2,%mm4;			punpckldq	%mm1,%mm2;
	movq		1*16+8(%ebx),%mm6;    		punpckhdq	%mm1,%mm4;
	movq		%mm0,%mm5;			punpckldq	%mm3,%mm5;
	movq		0*16+8(%ebx),%mm7;		punpckhdq	%mm3,%mm0;
	movq		3*16+8(%ebx),%mm1;		paddw		%mm6,%mm5;
	movq		2*16+8(%ebx),%mm3;		paddw		%mm6,%mm6;
	paddw		%mm7,%mm0; 			psubw		%mm5,%mm6;
	paddw		%mm7,%mm7; 			paddw		%mm1,%mm2;
	psubw		%mm0,%mm7;			paddw		%mm1,%mm1;
	paddw		%mm3,%mm4; 			paddw		%mm3,%mm3;
	psubw		%mm2,%mm1;			psubw		%mm4,%mm3;
	paddw		%mm0,%mm2; 			paddw		%mm0,%mm0;
	paddw		%mm5,%mm4; 			paddw		%mm5,%mm5;
	psubw		%mm2,%mm0;			psraw		$1,%mm2;
	psubw		%mm4,%mm5;			psraw		$1,%mm4;
	paddw		%mm2,%mm4;			paddw		%mm2,%mm2;
	movq		%mm0,0*16+8(%ebx);		psubw		%mm4,%mm2;
	pmulhw		0*16+8(%eax),%mm4;		paddw		%mm3,%mm1;
	movq		crnd,%mm0;			paddw		%mm6,%mm3;
	pmulhw		4*16+8(%eax),%mm2;		paddw		%mm0,%mm4;
	psraw		csh,%mm4;			paddw		%mm7,%mm6;
	pmulhw		cC4C6_14,%mm3;			paddw		%mm0,%mm2;
        movq		%mm4,0*16+8+768(%esi);		movq		%mm1,%mm4;
        psraw		csh,%mm2;	        	psraw		$3,%mm1; 
	movq		%mm2,4*16+8+768(%esi);		movq		%mm6,%mm2;
	pmulhw		c1C6_13,%mm7;			psraw		$3,%mm6;
	pmulhw		cC2C61_13,%mm4;			paddw		%mm1,%mm6;
	pmulhw		cC2C61_13,%mm2;			paddw		%mm1,%mm1;
	psubw		%mm6,%mm1;			psraw		$1,%mm3; // red
	paddw		%mm4,%mm1; 			paddw		%mm7,%mm3;
	paddw		%mm2,%mm6;			paddw		%mm7,%mm7;
	psubw		%mm3,%mm7;			paddw		%mm3,%mm6;
	movq		csh,%mm4;			psraw		$1,%mm5;
	paddw		%mm7,%mm1; 			paddw		%mm7,%mm7;
	psubw		%mm1,%mm7;			paddw		%mm3,%mm3;
	movq		0*16+8(%ebx),%mm2;		psubw		%mm6,%mm3;
	pmulhw		5*16+8(%eax),%mm1;		psraw		$1,%mm2;
	pmulhw		3*16+8(%eax),%mm7;		paddw		%mm2,%mm5;
	pmulhw		cC4_15,%mm5; 			psraw		$1,%mm2;
	pmulhw		7*16+8(%eax),%mm3; 		paddw		%mm0,%mm1;
	pmulhw		1*16+8(%eax),%mm6;		psraw		%mm4,%mm1;
	paddw		%mm2,%mm5; 			paddw		%mm2,%mm2;
	movq		%mm1,5*16+8+768(%esi);		psubw		%mm5,%mm2;
	pmulhw		6*16+8(%eax),%mm2;		paddw		%mm0,%mm7;
	psraw		%mm4,%mm7;			paddw		%mm0,%mm3;
	pmulhw		2*16+8(%eax),%mm5;		paddw		%mm0,%mm6;
	paddw		%mm0,%mm2;			psraw		%mm4,%mm6;
	movq		%mm7,3*16+8+768(%esi);		psraw		%mm4,%mm3;
	movq		%mm3,7*16+8+768(%esi);		psraw		%mm4,%mm2;
	movq		7*16+0(%ebx),%mm1; 		paddw		%mm0,%mm5;
	movq		(%ebx),%mm7;			psraw		%mm4,%mm5;
	movq		%mm6,1*16+8+768(%esi);		paddw		%mm7,%mm1;
	movq		2*16+0(%ebx),%mm3;		paddw		%mm7,%mm7;
	movq		5*16+0(%ebx),%mm4; 		psubw		%mm1,%mm7;
	movq		%mm2,6*16+8+768(%esi);		paddw		%mm3,%mm4;
	movq		3*16+0(%ebx),%mm0;		paddw		%mm3,%mm3;
	movq		4*16+0(%ebx),%mm2; 		psubw		%mm4,%mm3;
	movq		%mm5,2*16+8+768(%esi);		paddw		%mm0,%mm2;
	movq		1*16+0(%ebx),%mm6;		paddw		%mm0,%mm0;
	movq		6*16+0(%ebx),%mm5; 		psubw		%mm2,%mm0;
	paddw		%mm1,%mm2; 			paddw		%mm1,%mm1;
	paddw		%mm6,%mm5; 			paddw		%mm6,%mm6;
	psubw		%mm5,%mm6;			psubw		%mm2,%mm1;
	paddw		%mm5,%mm4; 			paddw		%mm5,%mm5;
	psubw		%mm4,%mm5;			psraw		$1,%mm4; 
	movq		%mm5,(%ebx);			psraw		$1,%mm2;
	paddw		%mm2,%mm4; 			paddw		%mm2,%mm2;
	psubw		%mm4,%mm2;			psraw		$1,%mm1;
	pmulhw		4*16+0(%eax),%mm2;		movq		%mm4,%mm5;
	pmulhw		(%eax),%mm4;			paddw		%mm3,%mm0;
	paddw		c128,%mm5;			paddw		%mm6,%mm3;
	paddw		crnd,%mm2;			psraw		$8,%mm5;
	psraw		csh,%mm2;			psllq		$48,%mm5;
	paddw		crnd,%mm4;			psrlq		$48,%mm5;
	movq		%mm2,4*16+0+768(%esi);		paddw		%mm7,%mm6;
	psraw		csh,%mm4;			movq		%mm6,%mm2;
	pmulhw		cC2C61_13,%mm2;			por		%mm5,%mm4;
	pmulhw		cC4C6_14,%mm3;			psraw		$3,%mm6;
	movq		%mm4,768(%esi);			movq		%mm0,%mm4;
	pmulhw		c1C6_13,%mm7;			psraw		$3,%mm0; 
	pmulhw		cC2C61_13,%mm4;			paddw		%mm0,%mm6;
	paddw		%mm0,%mm0; 			psraw		$1,%mm3; // red
	paddw		%mm7,%mm3; 			psubw		%mm6,%mm0;
	paddw		%mm4,%mm0; 			paddw		%mm7,%mm7;
	paddw		%mm2,%mm6;			psubw		%mm3,%mm7;
	movq		crnd,%mm5;			paddw		%mm7,%mm0;
	paddw		%mm7,%mm7; 			leal		128(%esi),%esi;
	movq		(%ebx),%mm4;			paddw		%mm3,%mm6;
	psubw		%mm0,%mm7;			paddw		%mm3,%mm3;
	pmulhw		5*16+0(%eax),%mm0;		psraw		$1,%mm4;
	movq		csh,%mm2;			paddw		%mm1,%mm4;
	pmulhw		cC4_15,%mm4;			psubw		%mm6,%mm3;
	pmulhw		3*16+0(%eax),%mm7;		psraw		$1,%mm1;
	pmulhw		1*16+0(%eax),%mm6;		paddw		%mm1,%mm4;
	pmulhw		7*16+0(%eax),%mm3;		paddw		%mm1,%mm1;
	psubw		%mm4,%mm1;			paddw		%mm5,%mm0;
	pmulhw		2*16+0(%eax),%mm4;		psraw		%mm2,%mm0;
	pmulhw		6*16+0(%eax),%mm1;		paddw		%mm5,%mm7;
	paddw		%mm5,%mm6;			psraw		%mm2,%mm7;
	movq		%mm0,5*16-128+768(%esi);	psraw		%mm2,%mm6;
	movq		%mm7,3*16-128+768(%esi);	paddw		%mm5,%mm3;
	movq		%mm6,1*16-128+768(%esi);	psraw		%mm2,%mm3;
	paddw		%mm5,%mm4;			psraw		%mm2,%mm4;
	movq		%mm3,7*16-128+768(%esi);	paddw		%mm5,%mm1;
	movq		%mm4,2*16-128+768(%esi);	psraw		%mm2,%mm1;
	movq		%mm1,6*16-128+768(%esi);	jne		1b;

	popl		%ecx;				popl		%eax;
	popl		%ebx;				popl		%esi;
	ret





	.text
	.align		16
	.globl		mmx_fdct_inter

mmx_fdct_inter:

	pushl		%ebx;				movl		$mblock+768*2,%ebx;
	pushl		%esi;				cmpl		%eax,%ebx;
	pushl		%edi;				movl		%eax,%esi;
	pushl		%ecx;				movl		$0,%eax;
	movl		$mblock-128,%edi;		movl		$6,%ecx;
	jne		1f;
	movl		$mblock+768*1,%ebx;

	.align		16
1:
	movq		(%esi),%mm7;
	leal		128(%edi),%edi;
	movq		7*16+0(%esi),%mm0;
	movq		1*16+0(%esi),%mm6;
	paddw		%mm7,%mm0;
	movq		6*16+0(%esi),%mm1;
	paddw		%mm7,%mm7;
	movq		2*16+0(%esi),%mm5;
	psubw		%mm0,%mm7;
	movq		5*16+0(%esi),%mm2;
	paddw		%mm6,%mm1;
	movq		3*16+0(%esi),%mm4;
	paddw		%mm6,%mm6;
	movq		4*16+0(%esi),%mm3;
	psubw		%mm1,%mm6;
	paddw		%mm5,%mm2;
	paddw		%mm5,%mm5;
	paddw		%mm4,%mm3;
	psubw		%mm2,%mm5;
	paddw		%mm4,%mm4;
	psubw		%mm2,%mm1;
	psubw		%mm3,%mm4;
	paddw		%mm0,%mm3;
	paddw		%mm0,%mm0;
	paddw		%mm2,%mm2;
	psubw		%mm3,%mm0;
	paddw		%mm1,%mm2;
	paddw		%mm0,%mm1;
	psllw		$2,%mm0;
	psllw		$3,%mm1;
	paddw		%mm3,%mm2;
	paddw		c1,%mm1;
	paddw		%mm2,%mm2;
	paddw		%mm5,%mm4;
	psllw		$2,%mm3;
	pmulhw		cC4_15,%mm1;
	psubw		%mm2,%mm3;
	paddw		%mm6,%mm5;
	paddw		%mm7,%mm6;
	movq		%mm3,4*16+0(%ebx);
	psllw		$4,%mm5;
	pmulhw		cC4C6_14,%mm5;
	paddw		%mm0,%mm1;
	paddw		%mm0,%mm0;
	psllw		$5,%mm7;
	pmulhw		c1C6_13,%mm7;
	psubw		%mm1,%mm0;
	paddw		c2,%mm1;
	movq		%mm6,%mm3;
	movq		%mm0,6*16+0(%ebx);
	psraw		$2,%mm1;
	movq		%mm4,%mm0;
	psllw		$4,%mm3;
	paddw		%mm7,%mm5;
	psllw		$4,%mm0;
	pmulhw		cC4_15,%mm0;
	paddw		%mm7,%mm7;
	pmulhw		cC4_15,%mm3;
	psubw		%mm5,%mm7;
	paddw		%mm4,%mm6;
	psllw		$3,%mm4;
	paddw		%mm0,%mm4;
	psllw		$2,%mm6;
	psubw		%mm6,%mm4;
	paddw		%mm3,%mm6;
	paddw		%mm5,%mm6;
	paddw		%mm5,%mm5;
	paddw		%mm7,%mm4;
	psubw		%mm6,%mm5;
	paddw		c4,%mm6;
	paddw		%mm7,%mm7;
	movq		%mm2,%mm3;
	psraw		$3,%mm6;
	psubw		%mm4,%mm7;
	punpcklwd	%mm6,%mm2;
	paddw		c4,%mm7;
	punpckhwd	%mm6,%mm3;
	movq		%mm1,%mm0;
	psraw		$3,%mm7;
	paddw		c2,%mm4;
	punpcklwd	%mm7,%mm1;
	punpckhwd	%mm7,%mm0;
	movq		%mm2,%mm7;
	paddw		c1,%mm5;
	punpckldq	%mm1,%mm2;
	punpckhdq	%mm1,%mm7;
	movq		%mm3,%mm1;
	movq		%mm2,0*16+0(%ebx);
	punpckldq	%mm0,%mm1;
	movq		4*16+0(%ebx),%mm6;
	punpckhdq	%mm0,%mm3;
	movq		%mm7,1*16+0(%ebx);
	psraw		$2,%mm4;
	movq		6*16+0(%ebx),%mm0;
	psraw		$1,%mm5;
	movq		%mm0,%mm7;
	punpcklwd	%mm5,%mm0;
	movq		%mm1,2*16+0(%ebx);
	punpckhwd	%mm5,%mm7;
	movq		%mm3,3*16+0(%ebx);
	movq		%mm6,%mm3;
	movq		2*16+8(%esi),%mm5;
	punpcklwd	%mm4,%mm3;
	movq		5*16+8(%esi),%mm2;
	punpckhwd	%mm4,%mm6;
	movq		%mm3,%mm4;
	punpckldq	%mm0,%mm3;
	movq		0*16+8(%esi),%mm1;
	punpckhdq	%mm0,%mm4;
	movq		%mm6,%mm0;
	punpckldq	%mm7,%mm0;
	movq		%mm3,0*16+8(%ebx);
	punpckhdq	%mm7,%mm6;
	movq		7*16+8(%esi),%mm3;
	paddw		%mm5,%mm2;
	movq		%mm0,2*16+8(%ebx);
	paddw		%mm5,%mm5;
	movq		%mm4,1*16+8(%ebx);
	psubw		%mm2,%mm5;
	movq		4*16+8(%esi),%mm0;
	paddw		%mm1,%mm3;
	movq		3*16+8(%esi),%mm4;
	paddw		%mm1,%mm1;
	movq		%mm6,3*16+8(%ebx);
	psubw		%mm3,%mm1;
	movq		6*16+8(%esi),%mm7;
	paddw		%mm4,%mm0;
	movq		1*16+8(%esi),%mm6;
	leal		128(%esi),%esi;
	paddw		%mm4,%mm4;
	psubw		%mm0,%mm4;
	paddw		%mm6,%mm7;
	paddw		%mm6,%mm6;
	paddw		%mm3,%mm0;
	psubw		%mm7,%mm6;
	psubw		%mm2,%mm7;
	paddw		%mm3,%mm3;
	paddw		%mm2,%mm2;
	psubw		%mm0,%mm3;
	paddw		%mm7,%mm2;
	paddw		%mm3,%mm7;
	psllw		$2,%mm3;
	paddw		%mm0,%mm2;
	psllw		$3,%mm7;
	paddw		c1,%mm7;
	paddw		%mm2,%mm2;
	paddw		%mm5,%mm4;
	psllw		$2,%mm0;
	pmulhw		cC4_15,%mm7;
	psubw		%mm2,%mm0;
	paddw		%mm6,%mm5;
	paddw		%mm1,%mm6;
	movq		%mm0,4*16+8(%ebx);
	psllw		$4,%mm5;
	pmulhw		cC4C6_14,%mm5;
	psllw		$5,%mm1;
	paddw		%mm3,%mm7;
	paddw		%mm3,%mm3;
	pmulhw		c1C6_13,%mm1;
	psubw		%mm7,%mm3;
	paddw		c2,%mm7;
	movq		%mm6,%mm0;
	movq		%mm3,6*16+8(%ebx);
	psraw		$2,%mm7;
	paddw		%mm1,%mm5;
	movq		%mm4,%mm3;
	paddw		%mm1,%mm1;
	psllw		$4,%mm3;
	pmulhw		cC4_15,%mm3;
	psllw		$4,%mm0;
	pmulhw		cC4_15,%mm0;
	paddw		%mm4,%mm6;
	psubw		%mm5,%mm1;
	psllw		$3,%mm4;
	paddw		%mm3,%mm4;
	psllw		$2,%mm6;
	psubw		%mm6,%mm4;
	paddw		%mm0,%mm6;
	paddw		%mm5,%mm6;
	paddw		%mm5,%mm5;
	psubw		%mm6,%mm5;
	paddw		%mm1,%mm4;
	paddw		c4,%mm6;
	paddw		%mm1,%mm1;
	psubw		%mm4,%mm1;
	psraw		$3,%mm6;
	movq		%mm2,%mm0;
	punpcklwd	%mm6,%mm2;
	paddw		c4,%mm1;
	punpckhwd	%mm6,%mm0;
	movq		%mm7,%mm3;
	psraw		$3,%mm1;
	paddw		c2,%mm4;
	punpcklwd	%mm1,%mm7;
	movq		%mm2,%mm6;
	punpckhwd	%mm1,%mm3;
	paddw		c1,%mm5;
	punpckldq	%mm7,%mm2;
	punpckhdq	%mm7,%mm6;
	movq		%mm0,%mm7;
	movq		%mm2,4*16+0(%ebx);
	punpckldq	%mm3,%mm7;
	movq		6*16+8(%ebx),%mm1;
	punpckhdq	%mm3,%mm0;
	movq		%mm6,5*16+0(%ebx);
	psraw		$2,%mm4;
	movq		%mm7,6*16+0(%ebx);
	psraw		$1,%mm5;
	movq		%mm1,%mm3;
	punpckhwd	%mm5,%mm1;
	movq		4*16+8(%ebx),%mm7;
	punpcklwd	%mm5,%mm3;
	movq		%mm7,%mm6;
	punpcklwd	%mm4,%mm7;
	movq		3*16+8(%ebx),%mm5;
	punpckhwd	%mm4,%mm6;
	movq		%mm7,%mm4;
	punpckldq	%mm3,%mm7;
	movq		%mm0,7*16+0(%ebx);
	punpckhdq	%mm3,%mm4;
	movq		%mm6,%mm2;
	punpckhdq	%mm1,%mm6;
	movq		2*16+8(%ebx),%mm3;
	punpckldq	%mm1,%mm2;
	movq		0*16+8(%ebx),%mm0;
	paddw		%mm5,%mm7;
	movq		1*16+8(%ebx),%mm1;
	paddw		%mm5,%mm5;
	psubw		%mm7,%mm5;
	paddw		%mm3,%mm4;
	paddw		%mm0,%mm6;
	paddw		%mm3,%mm3;
	psubw		%mm4,%mm3;
	paddw		%mm0,%mm0;
	paddw		%mm1,%mm2;
	paddw		%mm1,%mm1;
	psubw		%mm6,%mm0;
	psubw		%mm2,%mm1;
	paddw		%mm6,%mm7;
	paddw		%mm6,%mm6;
	paddw		%mm2,%mm4;
	paddw		%mm2,%mm2;
	psubw		%mm7,%mm6;
	psubw		%mm4,%mm2;
	paddw		%mm7,%mm4;
	paddw		%mm7,%mm7;
	psubw		%mm4,%mm7;
	pmulhw		mmx_q_fdct_inter_lut0+8,%mm4;
	pmulhw		mmx_q_fdct_inter_lut0+8,%mm7;
	movq		%mm1,128+16(%ebx);
	movq		%mm3,128+32(%ebx);
	paddw		c2,%mm7;
	paddw		c2,%mm4;
	movq		%mm0,128+24(%ebx);
	movq		%mm5,128+40(%ebx);
	movq		%mm4,%mm1;
	punpcklwd	%mm4,%mm4;
	movq		cfae,%mm3;
	psrad		$18,%mm4;
	pmaddwd		%mm3,%mm4;
	punpckhwd	%mm1,%mm1;
	movq		%mm6,%mm5;
	psrad		$18,%mm1;
	pmaddwd		%mm3,%mm1;
	punpcklwd	%mm2,%mm6;
	movq		%mm7,%mm0;
	punpckhwd	%mm2,%mm5;
	movq		%mm6,%mm2;
	punpcklwd	%mm7,%mm7;
	pmaddwd		mmx_q_fdct_inter_lut+1*32+0+16,%mm6;
	psrad		$18,%mm7;
	pmaddwd		%mm3,%mm7;
	psrad		$15,%mm4;
	pmaddwd		mmx_q_fdct_inter_lut+4*32+0+16,%mm2;
	psrad		$15,%mm1;
	paddd		c1_16,%mm6;
	packssdw	%mm1,%mm4;
	movq		c1_16,%mm1;
	punpckhwd	%mm0,%mm0;
	paddd		%mm1,%mm2;
	psrad		$18,%mm0;
	pmaddwd		%mm3,%mm0;
	psrad		$15,%mm7;
	movq		%mm4,0*16+8(%edi);
	psrad		$17,%mm6;
	pmaddwd		%mm3,%mm6;
	psrad		$17,%mm2;
	pmaddwd		%mm3,%mm2;
	psrad		$15,%mm0;
	packssdw	%mm0,%mm7;
	movq		%mm5,%mm0;
	pmaddwd		mmx_q_fdct_inter_lut+1*32+8+16,%mm5;
	psrad		$15,%mm6;
	pmaddwd		mmx_q_fdct_inter_lut+4*32+8+16,%mm0;
	psrad		$15,%mm2;
	movq		%mm7,4*16+8(%edi);
	por		%mm7,%mm4;
	movq		128+24(%ebx),%mm7;
	paddd		%mm1,%mm5;
	psrad		$17,%mm5;
	pmaddwd		%mm3,%mm5;
	paddd		%mm1,%mm0;
	movq		128+16(%ebx),%mm1;
	psrad		$17,%mm0;
	pmaddwd		%mm3,%mm0;
	movq		128+32(%ebx),%mm3;
	psrad		$15,%mm5;
	packssdw	%mm5,%mm6;
	por		%mm6,%mm4;
	psrad		$15,%mm0;
	movq		%mm6,2*16+8(%edi);
	packssdw	%mm0,%mm2;
	movq		128+40(%ebx),%mm5;
	por		%mm2,%mm4;
	movq		%mm2,6*16+8(%edi);
	paddw		%mm3,%mm5;
	movq		%mm4,128+8(%ebx);
	paddw		%mm1,%mm3;
	paddw		%mm7,%mm1;
	paddw		%mm3,%mm3;
	paddw		c1,%mm3;
	pmulhw		cC4_15,%mm3;
	paddw		%mm7,%mm3;
	paddw		%mm7,%mm7;
	psubw		%mm3,%mm7;
	movq		%mm3,%mm6;
	movq		%mm7,%mm2;
	paddw		%mm5,%mm5;
	paddw		c1,%mm5;
	paddw		%mm1,%mm1;
	paddw		c1,%mm1;
	movq		%mm5,%mm4;
	punpckhwd	%mm1,%mm4;
	movq		%mm4,%mm0;
	punpcklwd	%mm1,%mm5;
	movq		%mm5,%mm1;
	pmaddwd		cC2626_15,%mm5;
	psrad		$16,%mm5;
	pmaddwd		cC6262_15,%mm1;
	psrad		$16,%mm1;
	pmaddwd		cC2626_15,%mm4;
	psrad		$16,%mm4;
	pmaddwd		cC6262_15,%mm0;
	psrad		$16,%mm0;
	packssdw	%mm4,%mm5;
	packssdw	%mm0,%mm1;
	punpcklwd	%mm1,%mm6;
	movq		%mm6,%mm0;
	punpcklwd	%mm5,%mm2;
	movq		%mm2,%mm4;
	pmaddwd		mmx_q_fdct_inter_lut+0*32+0+16,%mm6;
	paddd		c1_17,%mm6;
	psrad		$18,%mm6;
	pmaddwd		cfae,%mm6;
	psrad		$15,%mm6;
	pmaddwd		mmx_q_fdct_inter_lut+2*32+0+16,%mm2;
	paddd		c1_17,%mm2;
	psrad		$18,%mm2;
	pmaddwd		cfae,%mm2;
	psrad		$15,%mm2;
	pmaddwd		mmx_q_fdct_inter_lut+3*32+0+16,%mm4;
	paddd		c1_17,%mm4;
	psrad		$18,%mm4;
	pmaddwd		cfae,%mm4;
	psrad		$15,%mm4;
	pmaddwd		mmx_q_fdct_inter_lut+5*32+0+16,%mm0;
	paddd		c1_15,%mm0;
	psrad		$16,%mm0;
	pmaddwd		cfae,%mm0;
	psrad		$15,%mm0;
	punpckhwd	%mm1,%mm3;
	movq		%mm3,%mm1;
	punpckhwd	%mm5,%mm7;
	movq		%mm7,%mm5;
	pmaddwd		mmx_q_fdct_inter_lut+0*32+8+16,%mm3;
	paddd		c1_17,%mm3;
	psrad		$18,%mm3;
	pmaddwd		cfae,%mm3;
	psrad		$15,%mm3;
	pmaddwd		mmx_q_fdct_inter_lut+2*32+8+16,%mm7;
	paddd		c1_17,%mm7;
	psrad		$18,%mm7;
	pmaddwd		cfae,%mm7;
	psrad		$15,%mm7;
	pmaddwd		mmx_q_fdct_inter_lut+3*32+8+16,%mm5;
	paddd		c1_17,%mm5;
	psrad		$18,%mm5;
	pmaddwd		cfae,%mm5;
	psrad		$15,%mm5;
	pmaddwd		mmx_q_fdct_inter_lut+5*32+8+16,%mm1;	
	paddd		c1_15,%mm1;
	psrad		$16,%mm1;
	pmaddwd		cfae,%mm1;
	psrad		$15,%mm1;
	packssdw	%mm3,%mm6;
	packssdw	%mm7,%mm2;
	packssdw	%mm5,%mm4;
	packssdw	%mm1,%mm0;
	movq		%mm6,1*16+8(%edi);
	por		%mm2,%mm6;
	movq		%mm2,3*16+8(%edi);
	movq		%mm4,5*16+8(%edi);
	por		%mm0,%mm4;
	movq		%mm0,7*16+8(%edi);
	por		128+8(%ebx),%mm6;
	por		%mm4,%mm6;
	movq		%mm6,128+8(%ebx);
	movq		(%ebx),%mm7;
	movq		7*16+0(%ebx),%mm6;
	paddw		%mm7,%mm6;
	paddw		%mm7,%mm7;
	psubw		%mm6,%mm7;
	movq		1*16+0(%ebx),%mm1;
	movq		6*16+0(%ebx),%mm2;
	paddw		%mm1,%mm2;
	paddw		%mm1,%mm1;
	psubw		%mm2,%mm1;
	movq		2*16+0(%ebx),%mm3;
	movq		5*16+0(%ebx),%mm4;
	paddw		%mm3,%mm4;
	paddw		%mm3,%mm3;
	psubw		%mm4,%mm3;
	movq		3*16+0(%ebx),%mm5;
	movq		4*16+0(%ebx),%mm0;
	paddw		%mm5,%mm0;
	paddw		%mm5,%mm5;
	psubw		%mm0,%mm5;
	paddw		%mm6,%mm0;
	paddw		%mm6,%mm6;
	psubw		%mm0,%mm6;
	paddw		%mm2,%mm4;
	paddw		%mm2,%mm2;
	psubw		%mm4,%mm2;
	paddw		%mm0,%mm4;
	paddw		%mm0,%mm0;
	psubw		%mm4,%mm0;
	pmulhw		mmx_q_fdct_inter_lut0,%mm4;
	pmulhw		mmx_q_fdct_inter_lut0,%mm0;
	movq		%mm1,128+16(%ebx);
	paddw		c2,%mm4;
	paddw		c2,%mm0;
	movq		%mm4,%mm1;
	movq		%mm5,128+32(%ebx);
	punpckhwd	%mm1,%mm1;
	movq		%mm0,%mm5;
	punpcklwd	%mm4,%mm4;
	movq		%mm7,128+24(%ebx);
	punpckhwd	%mm5,%mm5;
	punpcklwd	%mm0,%mm0;
	psrad		$18,%mm1;
	movq		cfae,%mm7;
	psrad		$18,%mm4;
	pmaddwd		%mm7,%mm1;
	psrad		$18,%mm5;
	pmaddwd		%mm7,%mm4;
	psrad		$18,%mm0;
	pmaddwd		%mm7,%mm5;
	psrad		$15,%mm1;
	pmaddwd		%mm7,%mm0;
	psrad		$15,%mm4;
	psrad		$15,%mm5;
	psrad		$15,%mm0;
	packssdw	%mm1,%mm4;
	packssdw	%mm5,%mm0;
	movq		%mm4,0*16+0(%edi);
	movq		%mm6,%mm5;
	movq		c1_16,%mm1;
	punpcklwd	%mm2,%mm6;
	movq		%mm0,4*16+0(%edi);
	por		%mm4,%mm0;
	movq		%mm6,%mm4;
	pmaddwd		mmx_q_fdct_inter_lut+1*32+0,%mm6;
	punpckhwd	%mm2,%mm5;
	pmaddwd		mmx_q_fdct_inter_lut+4*32+0,%mm4;
	movq		%mm5,%mm2;
	pmaddwd		mmx_q_fdct_inter_lut+1*32+8,%mm5;
	paddd		%mm1,%mm6;
	pmaddwd		mmx_q_fdct_inter_lut+4*32+8,%mm2;
	psrad		$17,%mm6;
	paddd		%mm1,%mm4;
	psrad		$17,%mm4;
	paddd		%mm1,%mm5;
	por		128+8(%ebx),%mm0;
	paddd		%mm1,%mm2;
	psrad		$17,%mm5;
	pmaddwd		%mm7,%mm6;
	psrad		$17,%mm2;
	pmaddwd		%mm7,%mm5;
	movq		128+32(%ebx),%mm1;
	pmaddwd		%mm7,%mm4;
	pmaddwd		%mm7,%mm2;
	psrad		$15,%mm6;
	movq		128+16(%ebx),%mm7;
	psrad		$15,%mm5;
	paddw		%mm3,%mm1;
	packssdw	%mm5,%mm6;
	movq		128+24(%ebx),%mm5;
	psrad		$15,%mm4;
	movq		%mm6,2*16+0(%edi);
	psrad		$15,%mm2;
	paddw		%mm7,%mm3;
	packssdw	%mm2,%mm4;
	movq		%mm4,6*16+0(%edi);
	paddw		%mm3,%mm3;
	paddw		c1,%mm3;
	por		%mm4,%mm6;
	paddw		%mm5,%mm7;
	por		%mm0,%mm6;
	pmulhw		cC4_15,%mm3;
	paddw		%mm1,%mm1;
	paddw		c1,%mm1;
	paddw		%mm7,%mm7;
	paddw		c1,%mm7;
	movq		%mm1,%mm2;
	paddw		%mm5,%mm3;
	paddw		%mm5,%mm5;
	psubw		%mm3,%mm5;
	punpckhwd	%mm7,%mm2;
	movq		%mm0,128+8(%ebx);
	punpcklwd	%mm7,%mm1;
	movq		%mm3,%mm6;
	movq		%mm1,%mm7;
	pmaddwd		cC2626_15,%mm1;
	movq		%mm5,%mm4;
	pmaddwd		cC6262_15,%mm7;
	movq		%mm2,%mm0;
	pmaddwd		cC2626_15,%mm2;
	pmaddwd		cC6262_15,%mm0;
	psrad		$16,%mm1;
	movq		%mm5,128+32(%ebx);
	psrad		$16,%mm7;
	movq		c1_17,%mm5;
	psrad		$16,%mm2;
	psrad		$16,%mm0;
	packssdw	%mm2,%mm1;
	packssdw	%mm0,%mm7;
	movq		%mm1,128+40(%ebx);
	punpcklwd	%mm7,%mm6;
	movq		%mm6,%mm0;
	pmaddwd		mmx_q_fdct_inter_lut+0*32+0,%mm6;
	punpckhwd	%mm7,%mm3;
	pmaddwd		mmx_q_fdct_inter_lut+5*32+0,%mm0;
	movq		%mm3,%mm7;
	pmaddwd		mmx_q_fdct_inter_lut+0*32+8,%mm3;
	punpcklwd	%mm1,%mm4;
	pmaddwd		mmx_q_fdct_inter_lut+5*32+8,%mm7;
	movq		%mm4,%mm2;
	pmaddwd		mmx_q_fdct_inter_lut+2*32+0,%mm4;
	pmaddwd		mmx_q_fdct_inter_lut+3*32+0,%mm2;
	paddd		%mm5,%mm6;
	paddd		%mm5,%mm3;
	paddd		c1_15,%mm7;
	paddd		c1_15,%mm0;
	paddd		%mm5,%mm4;
	paddd		%mm5,%mm2;
	movq		cfae,%mm1;
	psrad		$18,%mm6;
	psrad		$18,%mm3;
	psrad		$16,%mm7;
	psrad		$16,%mm0;
	psrad		$18,%mm4;
	psrad		$18,%mm2;
	pmaddwd		%mm1,%mm6;
	pmaddwd		%mm1,%mm3;
	pmaddwd		%mm1,%mm7;
	pmaddwd		%mm1,%mm0;
	pmaddwd		%mm1,%mm4;
	pmaddwd		%mm1,%mm2;
	psrad		$15,%mm6; 
	psrad		$15,%mm3;
	psrad		$15,%mm0;
	psrad		$15,%mm7;
	psrad		$15,%mm4;
	psrad		$15,%mm2;
	packssdw	%mm3,%mm6;
	packssdw	%mm7,%mm0;
	movq		%mm6,1*16+0(%edi);
	por		%mm0,%mm6;
	movq		%mm0,7*16+0(%edi);
	movq		128+32(%ebx),%mm3;
	punpckhwd	128+40(%ebx),%mm3;
	movq		%mm3,%mm7;
	pmaddwd		mmx_q_fdct_inter_lut+2*32+8,%mm3;
	pmaddwd		mmx_q_fdct_inter_lut+3*32+8,%mm7;
	paddd		%mm5,%mm3;
	paddd		%mm5,%mm7;
	psrad		$18,%mm3;
	psrad		$18,%mm7;
	pmaddwd		%mm1,%mm3;
	pmaddwd		%mm1,%mm7;
	psrad		$15,%mm3;
	psrad		$15,%mm7;
	packssdw	%mm3,%mm4;
	packssdw	%mm7,%mm2;
	movq		%mm4,3*16+0(%edi);
	por		%mm2,%mm4;
	movq		%mm2,5*16+0(%edi);
	por		%mm4,%mm6;
	por		128+8(%ebx),%mm6;
	movq		%mm6,%mm4;
	psrlq		$32,%mm6;
	por		%mm4,%mm6;
	movd		%mm6,%edx;
	cmpl		$1,%edx;
	rcll		%eax;
	decl		%ecx;
	jne		1b;

	popl		%ecx;
	popl		%edi;
	popl		%esi;
	xorb		$63,%al;
	popl		%ebx;
	ret

	.text
	.align		16
	.globl		mmx_copy_refblock

mmx_copy_refblock:

	pushl		%eax;
	movl		$mblock+3*6*128,%eax;
	pushl		%ebx;
	movl		$mb_address,%ebx;
	pushl		%edx;			
	movl		newref,%edx;
	pushl		%esi;			
	pushl		%edi;

	.align		16
1:
	addl		(%ebx),%edx;		movl		4(%ebx),%esi;
	movq		(%eax),%mm0;		addl		$8,%ebx;
	movq		1*8(%eax),%mm1;		leal		(%edx,%esi),%edi;
	movq		2*8(%eax),%mm2;		packuswb 	%mm1,%mm0;
	movq		3*8(%eax),%mm3;		movq 		%mm0,(%edx);		// 0
	movq		4*8(%eax),%mm4;		packuswb	%mm3,%mm2;
	movq		5*8(%eax),%mm5;		movq		%mm2,(%edx,%esi);	// 1
	movq		6*8(%eax),%mm6;		packuswb 	%mm5,%mm4;
	movq		7*8(%eax),%mm7;		movq 		%mm4,(%edx,%esi,2);	// 2
	movq		8*8(%eax),%mm0;		packuswb	%mm7,%mm6;
	movq		9*8(%eax),%mm1;		movq		%mm6,(%edi,%esi,2);	// 3
	movq		10*8(%eax),%mm2;	packuswb 	%mm1,%mm0;
	movq		11*8(%eax),%mm3;	movq 		%mm0,(%edx,%esi,4);	// 4
	movq		12*8(%eax),%mm4;	leal		(%edi,%esi,4),%edi;
	movq		13*8(%eax),%mm5;	packuswb	%mm3,%mm2;
	movq		14*8(%eax),%mm6;	movq		%mm2,(%edi);		// 5
	movq		15*8(%eax),%mm7;	packuswb 	%mm5,%mm4;
	leal		128(%eax),%eax;		movq 		%mm4,(%edi,%esi);	// 6	
	cmpl		$mb_address+6*8,%ebx;	packuswb	%mm7,%mm6;
	movq		%mm6,(%edi,%esi,2);	jne		1b;			// 7

	popl		%edi;			
	popl		%esi;
	popl		%edx;			
	popl		%ebx;			
	popl		%eax;
	ret;
