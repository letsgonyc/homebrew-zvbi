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

# $Id: motion_mmx.s,v 1.3 2001-04-19 23:54:21 mschimek Exp $

		.text
		.align		16
		.globl		p6_predict_forward_packed

p6_predict_forward_packed:

		pushl		%ecx
		pxor		%mm5,%mm5;
		movl		$mblock,%ecx
		movq		(%eax),%mm0;
		pxor		%mm6,%mm6;
		movq		(%ecx),%mm2;
		pxor		%mm7,%mm7;
		movq		8(%ecx),%mm3;

		.align 16
1:
		movq		128(%eax),%mm4;
		movq		%mm0,%mm1;
		punpcklbw	%mm5,%mm0;
		punpckhbw	%mm5,%mm1;
		psubw		%mm0,%mm2;
		paddw		%mm2,%mm6;
		psubw		%mm1,%mm3;
		paddw		%mm3,%mm6;
		movq		%mm0,768*3+0(%ecx);
		movq		%mm1,768*3+8(%ecx);
		movq		%mm2,768*1+0(%ecx);
		movq		%mm3,768*1+8(%ecx);
		pmaddwd		%mm2,%mm2;
		pmaddwd		%mm3,%mm3;
		paddd		%mm2,%mm7;
		paddd		%mm3,%mm7;
		movq		256+0(%ecx),%mm0;
		movq		256+8(%ecx),%mm1;
		movq		256(%eax),%mm3;
		movq		%mm4,%mm2;
		punpcklbw	%mm5,%mm4;
		punpckhbw	%mm5,%mm2;
		psubw		%mm4,%mm0;
		paddw		%mm0,%mm6;
		psubw		%mm2,%mm1;
		paddw		%mm1,%mm6;
		movq		%mm4,768*3+256+0(%ecx);
		movq		%mm2,768*3+256+8(%ecx);
		movq		%mm0,768*1+256+0(%ecx);
		movq		%mm1,768*1+256+8(%ecx);
		cmpl		$mblock+15*16-16,%ecx;
		pmaddwd		%mm0,%mm0;
		pmaddwd		%mm1,%mm1;
		paddd		%mm0,%mm7;
		paddd		%mm1,%mm7;
		movq		512+0(%ecx),%mm2;
		movq		512+8(%ecx),%mm4;
		movq		%mm3,%mm1;
		punpcklbw	%mm5,%mm3;
		punpckhbw	%mm5,%mm1;
		psubw		%mm3,%mm2;
		psubw		%mm1,%mm4;
		movq		%mm3,768*3+512+0(%ecx);
		movq		%mm1,768*3+512+8(%ecx);
		movq		%mm2,768*1+512+0(%ecx);
		movq		%mm4,768*1+512+8(%ecx);
		movq		8(%eax),%mm0;
		leal		8(%eax),%eax;
		movq		0+16(%ecx),%mm2;
		movq		8+16(%ecx),%mm3;
		leal		16(%ecx),%ecx;
		jne		1b;

		movq		128(%eax),%mm4;
		movq		%mm0,%mm1;
		punpcklbw	%mm5,%mm0;
		punpckhbw	%mm5,%mm1;
		psubw		%mm0,%mm2;
		paddw		%mm2,%mm6;
		psubw		%mm1,%mm3;
		paddw		%mm3,%mm6;
		movq		%mm0,768*3+0(%ecx);
		movq		%mm1,768*3+8(%ecx);
		movq		%mm2,768*1+0(%ecx);
		movq		%mm3,768*1+8(%ecx);
		pmaddwd		%mm2,%mm2;
		pmaddwd		%mm3,%mm3;
		paddd		%mm2,%mm7;
		paddd		%mm3,%mm7;
		movq		256+0(%ecx),%mm2;
		movq		256+8(%ecx),%mm3;
		movq		256(%eax),%mm0;
		movq		%mm4,%mm1;
		punpcklbw	%mm5,%mm4;
		punpckhbw	%mm5,%mm1;
		psubw		%mm4,%mm2;
		paddw		%mm2,%mm6;
		psubw		%mm1,%mm3;
		paddw		%mm3,%mm6;
		pmaddwd		c1,%mm6;
		movq		%mm4,768*3+256+0(%ecx);
		movq		%mm1,768*3+256+8(%ecx);
		movq		%mm2,768*1+256+0(%ecx);
		movq		%mm3,768*1+256+8(%ecx);
		pmaddwd		%mm2,%mm2;
		pmaddwd		%mm3,%mm3;
		paddd		%mm2,%mm7;
		paddd		%mm3,%mm7;
		movq		%mm7,%mm4;
		psrlq		$32,%mm7;
		paddd		%mm4,%mm7;
		movq		512+0(%ecx),%mm2;
		movq		512+8(%ecx),%mm3;
		movq		%mm0,%mm1;
		punpcklbw	%mm5,%mm0;
		punpckhbw	%mm5,%mm1;
		movq		%mm6,%mm5;
		psubw		%mm0,%mm2;
		psubw		%mm1,%mm3;
		movq		%mm0,768*3+512+0(%ecx);
		movq		%mm1,768*3+512+8(%ecx);
		psrlq		$32,%mm6;
		paddd		%mm5,%mm6;
		movq		%mm2,768*1+512+0(%ecx);
		movq		%mm3,768*1+512+8(%ecx);
		movd		%mm6,%ecx;
		imul		%ecx,%ecx;
		pslld		$8,%mm7;
		movd		%mm7,%eax;
		subl		%ecx,%eax;
		popl		%ecx
		ret

		.text
		.align		16
		.globl		p6_predict_forward_planar

p6_predict_forward_planar:

		movq		(%eax),%mm0;
		pxor		%mm5,%mm5;
		movq		8(%eax),%mm4;
		pushl		%edi
		pxor		%mm6,%mm6;
		pushl		%esi
		pxor		%mm7,%mm7;
		pushl		%ebx
		movl		$mblock,%ebx
		pushl		%ecx
		movl		mb_address+4,%ecx
		leal		8(%eax,%ecx,8),%esi

		.align 16
1:
		addl		%ecx,%eax;
		movq		(%ebx),%mm2;
		movq		8(%ebx),%mm3;
		movq		%mm0,%mm1;
		punpcklbw	%mm5,%mm0;
		punpckhbw	%mm5,%mm1;
		psubw		%mm0,%mm2;
		psubw		%mm1,%mm3;
		movq		%mm0,3*768(%ebx);
		movq		(%eax),%mm0;
		movq		%mm1,3*768+8(%ebx);
		movq		%mm2,1*768(%ebx);
		movq		%mm3,1*768+8(%ebx);
		paddw		%mm2,%mm6;
		paddw		%mm3,%mm6;
		pmaddwd		%mm2,%mm2;
		pmaddwd		%mm3,%mm3;
		paddd		%mm2,%mm7;
		paddd		%mm3,%mm7;

		movq		256(%ebx),%mm2;
		movq		256+8(%ebx),%mm3;
		cmpl		$mblock+256-32,%ebx;
		movq		%mm4,%mm1;
		punpcklbw	%mm5,%mm4;
		punpckhbw	%mm5,%mm1;
		psubw		%mm4,%mm2;
		psubw		%mm1,%mm3;
		movq		%mm4,3*768+256(%ebx);
		movq		8(%eax),%mm4;
		movq		%mm1,3*768+256+8(%ebx);
		movq		%mm2,1*768+256(%ebx);
		movq		%mm3,1*768+256+8(%ebx);
		leal		16(%ebx),%ebx;
		paddw		%mm2,%mm6;
		paddw		%mm3,%mm6;
		pmaddwd		%mm2,%mm2;
		pmaddwd		%mm3,%mm3;
		paddd		%mm2,%mm7;
		paddd		%mm3,%mm7;

		jne		1b;

		movq		(%ebx),%mm2;
		movq		8(%ebx),%mm3;
		movq		%mm0,%mm1;
		punpcklbw	%mm5,%mm0;
		punpckhbw	%mm5,%mm1;
		psubw		%mm0,%mm2;
		psubw		%mm1,%mm3;
		movq		%mm0,3*768(%ebx);
		movq		%mm1,3*768+8(%ebx);
		movq		%mm2,1*768(%ebx);
		movq		%mm3,1*768+8(%ebx);
		paddw		%mm2,%mm6;
		paddw		%mm3,%mm6;
		pmaddwd		%mm2,%mm2;
		pmaddwd		%mm3,%mm3;
		paddd		%mm2,%mm7;
		paddd		%mm3,%mm7;

		movq		256(%ebx),%mm2;
		movq		256+8(%ebx),%mm3;
		movq		%mm4,%mm1;
		punpcklbw	%mm5,%mm4;
		punpckhbw	%mm5,%mm1;
		psubw		%mm4,%mm2;
		psubw		%mm1,%mm3;
		paddw		%mm2,%mm6;
		paddw		%mm3,%mm6;
		pmaddwd		c1,%mm6;
		movq		%mm4,3*768+256(%ebx);
		movq		%mm1,3*768+256+8(%ebx);
		movq		%mm2,1*768+256(%ebx);
		movq		%mm3,1*768+256+8(%ebx);
		pmaddwd		%mm2,%mm2;
		pmaddwd		%mm3,%mm3;
		paddd		%mm2,%mm7;
		paddd		%mm3,%mm7;

		movq		%mm7,%mm4;
		psrlq		$32,%mm7;
		paddd		%mm4,%mm7;
		movq		%mm6,%mm1;
		psrlq		$32,%mm6;
		paddd		%mm1,%mm6;
		movd		%mm6,%edi;
		imul		%edi,%edi;
		addl		mb_address+32,%esi
		movl		$mblock+512,%ecx
		movl		mb_address+40,%ebx
		pslld		$8,%mm7;
		movd		%mm7,%eax;
		subl		%edi,%eax;
		movl		mb_address+36,%edi;

1:
		movq		(%esi),%mm0;
		movq		(%esi,%ebx),%mm4;
		addl		%edi,%esi;
		movq		(%ecx),%mm2;
		movq		8(%ecx),%mm3;
		movq		%mm0,%mm1;
		punpcklbw	%mm5,%mm0;
		punpckhbw	%mm5,%mm1;
		movq		%mm0,3*768(%ecx);
		psubw		%mm0,%mm2;
		movq		(%esi),%mm0;
		psubw		%mm1,%mm3;
		movq		%mm1,3*768+8(%ecx);
		movq		%mm2,1*768(%ecx);
		movq		%mm3,1*768+8(%ecx);

		movq		128(%ecx),%mm2;
		movq		128+8(%ecx),%mm3;
		movq		%mm4,%mm1;
		punpcklbw	%mm5,%mm4;
		punpckhbw	%mm5,%mm1;
		movq		%mm4,3*768+128(%ecx);
		psubw		%mm4,%mm2;
		movq		(%esi,%ebx),%mm4;
		addl		%edi,%esi;
		psubw		%mm1,%mm3;
		movq		%mm1,3*768+128+8(%ecx);
		movq		%mm2,1*768+128(%ecx);
		movq		%mm3,1*768+128+8(%ecx);

		movq		16(%ecx),%mm2;
		movq		24(%ecx),%mm3;
		cmpl		$mblock+5*128-32,%ecx;
		movq		%mm0,%mm1;
		punpcklbw	%mm5,%mm0;
		punpckhbw	%mm5,%mm1;
		psubw		%mm0,%mm2;
		psubw		%mm1,%mm3;
		movq		%mm0,3*768+16(%ecx);
		movq		%mm1,3*768+24(%ecx);
		movq		%mm2,1*768+16(%ecx);
		movq		%mm3,1*768+24(%ecx);

		movq		128+16(%ecx),%mm2;
		movq		128+24(%ecx),%mm3;
		movq		%mm4,%mm1;
		punpcklbw	%mm5,%mm4;
		punpckhbw	%mm5,%mm1;
		psubw		%mm4,%mm2;
		psubw		%mm1,%mm3;
		movq		%mm4,3*768+128+16(%ecx);
		movq		%mm1,3*768+128+24(%ecx);
		movq		%mm2,1*768+128+16(%ecx);
		movq		%mm3,1*768+128+24(%ecx);

		leal		32(%ecx),%ecx;
		jne		1b;

		popl		%ecx
		popl		%ebx
		popl		%esi
		popl		%edi
		ret

		.text
		.align		16
		.globl		mmx_predict_bidirectional_packed

# XXX needs work

mmx_predict_bidirectional_packed:

	subl $28,%esp
	movl $mblock,%eax
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %ebx
	xorl %edi,%edi
	movl $32,%ebp
	movl 48(%esp),%ebx
	movl 52(%esp),%esi
#APP
		pxor		%mm5,%mm5;
		pxor		%mm6,%mm6;
		pxor		%mm7,%mm7;
1:
		movq		(%ebx,%edi),%mm0;
		punpcklbw	c0,%mm0;
		movq		(%esi,%edi),%mm1;
		punpcklbw	c0,%mm1;
		movq		(%eax,%edi,2),%mm2;
		movq		%mm2,%mm3;
		movq		%mm2,%mm4;
		psubw		%mm0,%mm3;
		movq		%mm3,768(%eax,%edi,2);
		pmaddwd		%mm3,%mm3;
		paddd		%mm3,%mm5;
		psubw		%mm1,%mm4;
		movq		%mm4,1536(%eax,%edi,2);
		pmaddwd		%mm4,%mm4;
		paddd		%mm4,%mm6;
		paddw		%mm1,%mm0;
		paddw		c1,%mm0;
		psrlw		$1,%mm0;
		psubw		%mm0,%mm2;
		movq		%mm2,2304(%eax,%edi,2);
		pmaddwd		%mm2,%mm2;
		paddd		%mm2,%mm7;		
		
		movq		(%ebx,%edi),%mm0;
		punpckhbw	c0,%mm0;
		movq		(%esi,%edi),%mm1;
		punpckhbw	c0,%mm1;
		movq		8(%eax,%edi,2),%mm2;
		movq		%mm2,%mm3;
		movq		%mm2,%mm4;
		psubw		%mm0,%mm3;
		movq		%mm3,768+8(%eax,%edi,2);
		pmaddwd		%mm3,%mm3;
		paddd		%mm3,%mm5;
		psubw		%mm1,%mm4;
		movq		%mm4,1536+8(%eax,%edi,2);
		pmaddwd		%mm4,%mm4;
		paddd		%mm4,%mm6;
		paddw		%mm1,%mm0;
		paddw		c1,%mm0;
		psrlw		$1,%mm0;
		psubw		%mm0,%mm2;
		movq		%mm2,2304+8(%eax,%edi,2);
		pmaddwd		%mm2,%mm2;
		paddd		%mm2,%mm7;

		addl		$8,%edi;
		decl		%ebp;
		jne		1b;

		movq		%mm7,%mm0;		psrlq		$32,%mm7;
		paddd		%mm7,%mm0;		movq		%mm5,%mm1;
		psrlq		$32,%mm5;		movq		%mm6,%mm2;
		movd		%mm0,28(%esp);		paddd		%mm5,%mm1;
		psrlq		$32,%mm6;		pxor		%mm7,%mm7;
		movd		%mm1,%edx;		paddd		%mm6,%mm2;		
		movl		$256,%edi;		movl		$16,%ebp;
		movd		%mm2,%ecx;
2:
		movq		(%ebx,%edi),%mm0;
		movq		(%esi,%edi),%mm1;	movq		%mm0,%mm5;
		movq		%mm1,%mm6;		punpcklbw	%mm7,%mm0;
		movq		(%eax,%edi,2),%mm2;	punpckhbw	%mm7,%mm5;
		movq		%mm2,%mm3;		punpcklbw	%mm7,%mm1;
		psubw		%mm0,%mm3;		punpckhbw	%mm7,%mm6;		
		movq		%mm2,%mm4;		paddw		%mm1,%mm0;
		movq		%mm3,768(%eax,%edi,2);	psubw		%mm1,%mm4;
		paddw		c1,%mm0;
		movq		%mm4,1536(%eax,%edi,2);
		psrlw		$1,%mm0;
		psubw		%mm0,%mm2;
		movq		%mm2,2304(%eax,%edi,2);

		movq		8(%eax,%edi,2),%mm2;
		movq		%mm2,%mm3;
		movq		%mm2,%mm4;
		psubw		%mm5,%mm3;
		movq		%mm3,768+8(%eax,%edi,2);
		psubw		%mm6,%mm4;
		movq		%mm4,1536+8(%eax,%edi,2);
		paddw		%mm6,%mm5;
		paddw		c1,%mm5;
		psrlw		$1,%mm5;
		psubw		%mm5,%mm2;
		movq		%mm2,2304+8(%eax,%edi,2);

		addl		$8,%edi;
		decl		%ebp;
		jne		2b;
	
#NO_APP
	movl 56(%esp),%eax
	movl %edx,(%eax)
	movl 60(%esp),%eax
	movl %ecx,(%eax)
	movl 28(%esp),%eax
	popl %ebx
	popl %esi
	popl %edi
	popl %ebp
	addl $28,%esp
	ret


		.text
		.align		16
		.globl		mmx_bipp_sad

mmx_bipp_sad:
	pushl %ebp
	movl %esp,%ebp
	pushl %edi
	pushl %esi
	pushl %ebx
	movl 8(%ebp),%ecx
	movl 12(%ebp),%edx
	movl 16(%ebp),%eax

		pxor		%mm5,%mm5;
		pxor		%mm6,%mm6;
		pxor		%mm7,%mm7;

		movl		$16,%ebx

		.p2align	4,,7
.L1164:
		movq		(%eax),%mm4;
		movq		(%ecx),%mm3;		
		movq		%mm4,%mm2;
		psubusb		%mm3,%mm2;		
		psubusb		%mm4,%mm3;
		por		%mm3,%mm2;
		movq		%mm2,%mm3;
		punpcklbw	%mm5,%mm2;
		paddw		%mm2,%mm6;
		punpckhbw	%mm5,%mm3;
		paddw		%mm3,%mm6;

		movq		1(%ecx),%mm1;
		movq		%mm4,%mm0;		
		psubusb		%mm1,%mm0;		
		psubusb		%mm4,%mm1;		
		por		%mm1,%mm0;		
		movq		%mm0,%mm1;
		punpcklbw	%mm5,%mm0;
		paddw		%mm0,%mm7;
		punpckhbw	%mm5,%mm1;
		paddw		%mm1,%mm7;

		movq		8(%eax),%mm4;		
		movq		8(%ecx),%mm3;		
		movq		%mm4,%mm2;
		movq		%mm3,%mm0;		
		psrlq		$8,%mm0;
		psubusb		%mm3,%mm2;		
		psubusb		%mm4,%mm3;		
		por		%mm3,%mm2;		
		movq		%mm2,%mm3;
		punpcklbw	%mm5,%mm2;
		paddw		%mm2,%mm6;
		punpckhbw	%mm5,%mm3;
		paddw		%mm3,%mm6;

		movq		(%edx),%mm1;		
		psllq		$56,%mm1;		
		por		%mm0,%mm1;
		movq		%mm4,%mm0;		
		psubusb		%mm1,%mm0;		
		psubusb		%mm4,%mm1;		
		por		%mm1,%mm0;		
		movq		%mm0,%mm1;		
		punpcklbw	%mm5,%mm0;
		paddw		%mm0,%mm7;		
		punpckhbw	%mm5,%mm1;
		paddw		%mm1,%mm7;

		addl $16,%eax
		addl $16,%ecx
		incl %edx

		decl %ebx
		jne .L1164

		movq		%mm7,%mm0;
		punpcklwd	%mm6,%mm0;
		punpckhwd	%mm6,%mm7;
		paddw		%mm0,%mm7;

		movq		%mm7,%mm0;
		psrlq		$32,%mm0;
		paddw		%mm0,%mm7;
		movd		%mm7,%edx;
		movl		%edx,%esi;

		movl		24(%ebp),%eax
		movzxw		%dx,%edx;
		movl		%edx,(%eax); // right

		movl		20(%ebp),%eax
		shrl		$16,%esi
		movl		%esi,(%eax)

	popl %ebx
	popl %esi
	popl %edi
	movl %ebp,%esp
	popl %ebp
	ret





		.text
		.align		16
		.globl		mmx_sad

mmx_sad:
		movq		(%edx),%mm0;
		pushl		%ebx;
		movq		(%edx,%ecx),%mm2;
		pxor		%mm6,%mm6;
		movq		8(%edx),%mm1;
		pxor		%mm7,%mm7;
		movq		8(%edx,%ecx),%mm3;
		leal		(%edx,%ecx,2),%edx;
		movq		(%eax),%mm4;
		movl		$7,%ebx;

		.align 4
1:
		movq		%mm0,%mm5;
		psubusb		%mm4,%mm0;
		psubusb		%mm5,%mm4;
		movq		(%edx),%mm5;
		por		%mm4,%mm0;
		movq		%mm0,%mm4;
		punpcklbw	%mm6,%mm0;
		paddw		%mm0,%mm7;
		movq		8(%eax),%mm0;
		punpckhbw	%mm6,%mm4;
		paddw		%mm4,%mm7;
		movq		%mm1,%mm4;
		psubusb		%mm0,%mm1;
		psubusb		%mm4,%mm0;
		movq		(%edx,%ecx),%mm4;
		por		%mm0,%mm1;
		movq		%mm1,%mm0;
		punpcklbw	%mm6,%mm1;
		paddw		%mm1,%mm7;
		movq		16(%eax),%mm1;
		punpckhbw	%mm6,%mm0;
		paddw		%mm0,%mm7;
		movq		%mm5,%mm0;
		movq		%mm2,%mm5;
		psubusb		%mm1,%mm2;
		psubusb		%mm5,%mm1;
		movq		24(%eax),%mm5;
		addl		$32,%eax;
		por		%mm1,%mm2;
		movq		%mm2,%mm1;
		punpckhbw	%mm6,%mm1;
		paddw		%mm1,%mm7;
		movq		8(%edx),%mm1;
		punpcklbw	%mm6,%mm2;
		paddw		%mm2,%mm7;
		movq		%mm4,%mm2;
		movq		%mm3,%mm4;
		psubusb		%mm5,%mm3;
		psubusb		%mm4,%mm5;
		movq		(%eax),%mm4;
		por		%mm5,%mm3;
		movq		%mm3,%mm5;
		punpcklbw	%mm6,%mm3;
		paddw		%mm3,%mm7;
		movq		8(%edx,%ecx),%mm3;
		leal		(%edx,%ecx,2),%edx;
		punpckhbw	%mm6,%mm5;
		paddw		%mm5,%mm7;

		decl		%ebx;
		jne		1b;

		movq		%mm0,%mm5;
		psubusb		%mm4,%mm0;
		psubusb		%mm5,%mm4;
		movq		8(%eax),%mm5;
		por		%mm4,%mm0;
		movq		%mm0,%mm4;
		punpcklbw	%mm6,%mm0;
		punpckhbw	%mm6,%mm4;
		paddw		%mm0,%mm7;
		movq		16(%eax),%mm0;
		paddw		%mm4,%mm7;
		movq		%mm1,%mm4;
		psubusb		%mm5,%mm1;
		psubusb		%mm4,%mm5;
		por		%mm5,%mm1;
		movq		24(%eax),%mm4;
		movq		%mm1,%mm5;
		punpcklbw	%mm6,%mm1;
		punpckhbw	%mm6,%mm5;
		paddw		%mm1,%mm7;
		paddw		%mm5,%mm7;
		movq		%mm2,%mm5;
		psubusb		%mm0,%mm2;
		psubusb		%mm5,%mm0;
		movq		c1,%mm5;
		por		%mm0,%mm2;
		movq		%mm2,%mm0;
		punpcklbw	%mm6,%mm2;
		punpckhbw	%mm6,%mm0;
		paddw		%mm2,%mm7;
		paddw		%mm0,%mm7;
		movq		%mm3,%mm1;
		psubusb		%mm4,%mm3;
		psubusb		%mm1,%mm4;
		por		%mm4,%mm3;
		movq		%mm3,%mm4;
		punpcklbw	%mm6,%mm3;
		punpckhbw	%mm6,%mm4;
		paddw		%mm3,%mm7;
		paddw		%mm4,%mm7;
		pmaddwd		%mm5,%mm7;
		popl		%ebx;
		movq		%mm7,%mm0;
		psrlq		$32,%mm7;
		paddd		%mm0,%mm7;
		movd		%mm7,%eax;
		ret;

		.text
		.align		16
		.globl		mmx_bipp_sad2
# preliminary
mmx_bipp_sad2:
	pushl %ebp
	movl %esp,%ebp
	pushl %edi
	pushl %esi
	pushl %ebx
	movl 8(%ebp),%ecx
	movl 12(%ebp),%edx
	movl 16(%ebp),%eax

		pxor		%mm5,%mm5;
		pxor		%mm6,%mm6;
		pxor		%mm7,%mm7;

		movl		$8,%ebx

		.p2align	4,,7
.L1165:
// 0/0
		movq		(%eax),%mm4;
		movq		(%ecx),%mm3;		
		movq		%mm4,%mm2;
		psubusb		%mm3,%mm2;		
		psubusb		%mm4,%mm3;
		por		%mm3,%mm2;
		movq		%mm2,%mm3;
		punpcklbw	%mm5,%mm2;
		paddw		%mm2,%mm6;
		punpckhbw	%mm5,%mm3;
		paddw		%mm3,%mm6;

		movq		1(%ecx),%mm1;
		movq		%mm4,%mm0;		
		psubusb		%mm1,%mm0;		
		psubusb		%mm4,%mm1;		
		por		%mm1,%mm0;		
		movq		%mm0,%mm1;
		punpcklbw	%mm5,%mm0;
		paddw		%mm0,%mm7;
		punpckhbw	%mm5,%mm1;
		paddw		%mm1,%mm7;

		addl $16,%eax
		addl $16,%ecx
		incl %edx
// 1/1
		movq		8(%eax),%mm4;		
		movq		8(%ecx),%mm3;		
		movq		%mm4,%mm2;
		movq		%mm3,%mm0;		
		psubusb		%mm3,%mm2;		
		psubusb		%mm4,%mm3;		
		por		%mm3,%mm2;		
		movq		%mm2,%mm3;
		punpcklbw	%mm5,%mm2;
		paddw		%mm2,%mm6;
		punpckhbw	%mm5,%mm3;
		paddw		%mm3,%mm6;

		movq		(%edx),%mm1;		
		psrlq		$8,%mm0;
		psllq		$56,%mm1;		
		por		%mm0,%mm1;
		movq		%mm4,%mm0;		
		psubusb		%mm1,%mm0;		
		psubusb		%mm4,%mm1;		
		por		%mm1,%mm0;		
		movq		%mm0,%mm1;		
		punpcklbw	%mm5,%mm0;
		paddw		%mm0,%mm7;		
		punpckhbw	%mm5,%mm1;
		paddw		%mm1,%mm7;

		addl $16,%eax
		addl $16,%ecx
		incl %edx

		decl %ebx
		jne .L1165

		movq		%mm7,%mm0;
		punpcklwd	%mm6,%mm0;
		punpckhwd	%mm6,%mm7;
		paddw		%mm0,%mm7;

		movq		%mm7,%mm0;
		psrlq		$32,%mm0;
		paddw		%mm0,%mm7;
		movd		%mm7,%edx;
		movl		%edx,%esi;

		movl		24(%ebp),%eax
		movzxw		%dx,%edx;
		movl		%edx,(%eax); // right

		movl		20(%ebp),%eax
		shrl		$16,%esi
		movl		%esi,(%eax)

	popl %ebx
	popl %esi
	popl %edi
	movl %ebp,%esp
	popl %ebp
	ret

	.align 16
	.type	 rwmsr,@function
	.globl	rwmsr
rwmsr:
	pushl %ebp
	movl %esp,%ebp
	pushl %esi
	movl $192,%eax
	pushl %ebx
	movl 8(%ebp),%ebx
	movl 12(%ebp),%ecx
	movl 16(%ebp),%edx
#APP
	int $0x80
#NO_APP
	movl %eax,%esi
	cmpl $-126,%esi
	jbe .L312
	call __errno_location
	negl %esi
	movl %esi,(%eax)
	movl $-1,%esi
	.align 4
.L312:
	movl %esi,%eax
	leal -8(%ebp),%esp
	popl %ebx
	popl %esi
	movl %ebp,%esp
	popl %ebp
	ret

