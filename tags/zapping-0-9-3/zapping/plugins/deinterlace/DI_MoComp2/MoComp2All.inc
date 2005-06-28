/////////////////////////////////////////////////////////////////////////////
// $Id: MoComp2All.inc,v 1.1 2005-01-08 14:54:24 mschimek Exp $
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2003 Tom Barry & John Adcock  All rights reserved.
/////////////////////////////////////////////////////////////////////////////
//
//  This file is subject to the terms of the GNU General Public License as
//  published by the Free Software Foundation.  A copy of this license is
//  included with this software distribution in the file COPYING.  If you
//  do not have a copy, you may obtain a copy by writing to the Free
//  Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details
//
/////////////////////////////////////////////////////////////////////////////
// CVS Log
//
// $Log: not supported by cvs2svn $
// Revision 1.2  2003/02/11 15:40:52  adcockj
// New implementation of StrangeBob based off paper
// Tidy up sources
//
// Revision 1.1  2003/01/02 13:15:01  adcockj
// Added new plug-ins ready for developement by copying TomsMoComp and Gamma
//
//
// Log: TomsMoCompAll.inc,v
// Revision 1.3  2002/12/10 16:32:21  adcockj
// Fix StrangeBob for MMX
//
// Revision 1.2  2002/11/26 21:32:14  adcockj
// Made new strange bob method optional
//
// Revision 1.1  2002/07/07 20:07:24  trbarry
// First cut at TomsMoComp, motion compensated deinterlace
//
//
/////////////////////////////////////////////////////////////////////////////


#include "windows.h"
#include "DS_Deinterlace.h"
#include "MoComp2.h"

int  Max_Frames;
int  PrevFrame;
int  PrevInFrame;
int  dropFrame;
BOOL IsOdd;
#define TopFirst IsOdd
unsigned char* dstp;

const BYTE* pWeaveSrc;
const BYTE* pWeaveSrcP;
BYTE* pWeaveDest;
const BYTE* pCopySrc;
const BYTE* pCopySrcP;
BYTE* pCopyDest;
MEMCPY_FUNC* pMyMemcpy;

int src_pitch;
int dst_pitch;
int rowsize;
int RowPixels;
int height;
int FldHeight;
int BadAlign;

BOOL FUNCT_NAME(TDeinterlaceInfo* pInfo)
{
	pMyMemcpy = pInfo->pMemcpy;

    dstp = pInfo->Overlay;
    src_pitch = pInfo->InputPitch;
    dst_pitch = pInfo->OverlayPitch;
    rowsize = pInfo->LineLength;
	RowPixels = rowsize >> 1;
	FldHeight = pInfo->FieldHeight;
       
    pCopySrc = pInfo->PictureHistory[1]->pData;
    pCopySrcP = pInfo->PictureHistory[3]->pData;
    pWeaveSrc = pInfo->PictureHistory[0]->pData;  
    pWeaveSrcP = pInfo->PictureHistory[2]->pData;
	IsOdd =	pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD;

    if(IsOdd)
    {
		// if we have an odd field we copy an even field and weave an odd field
		pCopyDest = pInfo->Overlay;
		pWeaveDest = pInfo->Overlay + dst_pitch;
	}
    else
    {
		// if we have an ever field we copy an odd field and weave an even field
		pCopyDest = pInfo->Overlay + dst_pitch;
		pWeaveDest = pInfo->Overlay;

	}
	// copy 1st and last weave lines 
	Fieldcopy(pWeaveDest, pCopySrc, rowsize,		
				1, dst_pitch*2, src_pitch);
	Fieldcopy(pWeaveDest+(FldHeight-1)*dst_pitch*2,
		pCopySrc+(FldHeight-1)*src_pitch, rowsize, 
				1, dst_pitch*2, src_pitch);

	// copy all of the copy field
	Fieldcopy(pCopyDest, pCopySrc, rowsize, 
				FldHeight, dst_pitch*2, src_pitch);

	// then go fill in the hard part, being variously lazy depending upon
	// SearchEffort

	DoMoComp2_SimpleWeave();
	__asm emms;

    return TRUE;
}

int Fieldcopy(void *dest, const void *src, size_t count, 
				int rows, int dst_pitch, int src_pitch)
{
BYTE* pDest = (BYTE*) dest;
BYTE* pSrc = (BYTE*) src;
int i;
	
	for (i=0; i < rows; i++)
	{
		MyMemCopy(pDest, pSrc, count);
		pSrc += src_pitch;
		pDest += dst_pitch;
	}
	return 0;
}

#include "MoComp2All2.inc"