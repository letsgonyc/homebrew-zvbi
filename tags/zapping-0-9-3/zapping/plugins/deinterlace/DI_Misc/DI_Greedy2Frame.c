///////////////////////////////////////////////////////////////////////////////
// $Id: DI_Greedy2Frame.c,v 1.3 2005-03-30 21:27:32 mschimek Exp $
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000 John Adcock, Tom Barry, Steve Grimm  All rights reserved.
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
///////////////////////////////////////////////////////////////////////////////
// Change Log
//
// Date          Developer             Changes
//
// 08 Feb 2000   John Adcock           New Method
//
/////////////////////////////////////////////////////////////////////////////
// CVS Log
//
// $Log: not supported by cvs2svn $
// Revision 1.2  2005/02/05 22:19:53  mschimek
// Completed l18n.
//
// Revision 1.1  2005/01/08 14:35:01  mschimek
// TomsMoCompMethod, MoComp2Method, VideoWeaveMethod, VideoBobMethod,
// TwoFrameMethod, OldGameMethod, Greedy2FrameMethod, GreedyMethod,
// DI_GreedyHSettings: Localized.
//
// Revision 1.2  2004/11/15 23:03:19  michael
// *** empty log message ***
//
// Revision 1.1  2004/11/14 15:35:14  michael
// *** empty log message ***
//
// Revision 1.8  2002/06/18 19:46:07  adcockj
// Changed appliaction Messages to use WM_APP instead of WM_USER
//
// Revision 1.7  2002/06/13 12:10:24  adcockj
// Move to new Setings dialog for filers, video deint and advanced settings
//
// Revision 1.6  2001/07/13 16:13:33  adcockj
// Added CVS tags and removed tabs
//
/////////////////////////////////////////////////////////////////////////////

#include "windows.h"
#include "DS_Deinterlace.h"

extern long GreedyTwoFrameThreshold;

SIMD_PROTOS (DeinterlaceGreedy2Frame);

#ifdef SIMD

///////////////////////////////////////////////////////////////////////////////
// Field 1 | Field 2 | Field 3 | Field 4 |
//   T0    |         |    T1   |         | 
//         |   M0    |         |    M1   | 
//   B0    |         |    B1   |         | 
//

// debugging feature
// output the value of mm4 at this point which is pink where we will weave
// and green were we are going to bob
#define CHECK_BOBWEAVE 0

BOOL
SIMD_NAME (DeinterlaceGreedy2Frame) (TDeinterlaceInfo *pInfo)
{
    v8 qwGreedyTwoFrameThreshold;
    uint8_t *Dest;
    const uint8_t *T0;
    const uint8_t *T1;
    const uint8_t *M0;
    const uint8_t *M1;
    unsigned int byte_width;
    unsigned int height;
    unsigned int dst_padding;
    unsigned int src_padding;
    unsigned int dst_bpl;
    unsigned int src_bpl;

    if (SIMD == SSE2) {
	if (((unsigned int) pInfo->Overlay |
	     (unsigned int) pInfo->PictureHistory[0]->pData |
	     (unsigned int) pInfo->PictureHistory[1]->pData |
	     (unsigned int) pInfo->PictureHistory[2]->pData |
	     (unsigned int) pInfo->PictureHistory[3]->pData |
	     (unsigned int) pInfo->OverlayPitch |
	     (unsigned int) pInfo->InputPitch |
	     (unsigned int) pInfo->LineLength) & 15)
	    return DeinterlaceGreedy2Frame__SSE (pInfo);
    }

    qwGreedyTwoFrameThreshold = vsplat8 (GreedyTwoFrameThreshold);

    byte_width = pInfo->LineLength;

    dst_bpl = pInfo->OverlayPitch;
    src_bpl = pInfo->InputPitch;

    Dest = pInfo->Overlay;

    M1 = pInfo->PictureHistory[0]->pData;
    T1 = pInfo->PictureHistory[1]->pData;
    M0 = pInfo->PictureHistory[2]->pData;
    T0 = pInfo->PictureHistory[3]->pData;

    if (!(pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD)) {
        copy_line (Dest, M1, byte_width);
        Dest += dst_bpl;

        M1 = (const uint8_t *) pInfo->PictureHistory[0]->pData + src_bpl;
        M0 = (const uint8_t *) pInfo->PictureHistory[2]->pData + src_bpl;
    }

    dst_padding = dst_bpl * 2 - byte_width;
    src_padding = src_bpl - byte_width;

    for (height = pInfo->FieldHeight; height > 0; --height) {
        unsigned int count;

	for (count = byte_width / sizeof (vu8); count > 0; --count) {
	    vu8 m0, m1, t0, t1, b0, b1, avg, mm4;
	    v32 mm5, sum;

	    t1 = * (const vu8 *) &T1[0];
	    b1 = * (const vu8 *) &T1[src_bpl];
	    T1 += sizeof (vu8);

	    // Always use the most recent data verbatim.  By definition it's
	    // correct (it'd be shown on an interlaced display) and our job is
	    // to fill in the spaces between the new lines.
	    vstorent ((vu8 *) Dest, t1);

	    avg = fast_vavgu8 (t1, b1);

	    m0 = * (const vu8 *) M0;
	    M0 += sizeof (vu8);
	    m1 = * (const vu8 *) M1;
	    M1 += sizeof (vu8);

	    // if we have a good processor then make mm0 the average of M1
	    // and M0 which should make weave look better when there is
	    // small amounts of movement
	    if (SIMD != MMX)
		m1 = vavgu8 (m1, m0);

	    // if |M1-M0| > Threshold we want dword worth of twos
	    // XXX emulates vcmpgtu8, AVEC has this instruction
	    mm4 = vsr1u8 (vabsdiffu8 (m1, m0));
	    mm4 = (vu8) vcmpgt8 ((v8) mm4, qwGreedyTwoFrameThreshold);
	    mm4 = vand (mm4, vsplat8_127); // get rid of any sign bit
	    // XXX can be replaced by logic ops.
	    sum = (v32) vandnot (vcmpgt32 ((v32) mm4, vsplat32_1), vsplat32_2);

	    // if |T1-T0| > Threshold we want dword worth of ones
	    t0 = * (const vu8 *) &T0[0];
	    mm4 = vsr1u8 (vabsdiffu8 (t1, t0));
	    mm4 = (vu8) vcmpgt8 ((v8) mm4, qwGreedyTwoFrameThreshold);
	    mm4 = vand (mm4, vsplat8_127);
	    mm5 = (v32) vandnot (vcmpgt32 ((v32) mm4, vsplat32_1), vsplat32_1);
	    sum = vadd32 (sum, mm5);

	    b0 = * (const vu8 *) &T0[src_bpl];
	    T0 += sizeof (vu8);

	    // if |B1-B0| > Threshold we want dword worth of ones
	    mm4 = vsr1u8 (vabsdiffu8 (b1, b0));
	    mm4 = (vu8) vcmpgt8 ((v8) mm4, qwGreedyTwoFrameThreshold);
	    mm4 = vand (mm4, vsplat8_127);
	    mm5 = (v32) vandnot (vcmpgt32 ((v32) mm4, vsplat32_1), vsplat32_1);
	    sum = vadd32 (sum, mm4);

	    mm4 = (vu8) vcmpgt32 (sum, vsplat32_2);

	    // debugging feature
	    // output the value of mm4 at this point which is pink
	    // where we will weave and green were we are going to bob
	    if (CHECK_BOBWEAVE)
		vstorent ((vu8 *)(Dest + dst_bpl), mm4);
	    else
		vstorent ((vu8 *)(Dest + dst_bpl), vsel (avg, m1, mm4));

	    Dest += sizeof (vu8);
	}

        M1 += src_padding;
        T1 += src_padding;
        M0 += src_padding;
        T0 += src_padding;
        Dest += dst_padding;
    }

    if (pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD) {
	copy_line (Dest, T1, byte_width);
	Dest += dst_bpl;
	copy_line (Dest, M1, byte_width);
    } else {
	copy_line (Dest, T1, byte_width);
    }

    vempty ();

    return TRUE;
}

#else /* !SIMD */

long GreedyTwoFrameThreshold = 4;


////////////////////////////////////////////////////////////////////////////
// Start of Settings related code
/////////////////////////////////////////////////////////////////////////////
SETTING DI_Greedy2FrameSettings[DI_GREEDY2FRAME_SETTING_LASTONE] =
{
    {
        N_("Greedy 2 Frame Luma Threshold"), SLIDER, 0,
	&GreedyTwoFrameThreshold,
        4, 0, 128, 1, 1,
        NULL,
        "Deinterlace", "GreedyTwoFrameThreshold", NULL,
    },
};

DEINTERLACE_METHOD Greedy2FrameMethod =
{
    sizeof(DEINTERLACE_METHOD),
    DEINTERLACE_CURRENT_VERSION,
    N_("Greedy 2 Frame"), 
    "Greedy2", 
    FALSE, 
    FALSE, 
    /* pfnAlgorithm */ NULL,
    50, 
    60,
    DI_GREEDY2FRAME_SETTING_LASTONE,
    DI_Greedy2FrameSettings,
    INDEX_VIDEO_GREEDY2FRAME,
    NULL,
    NULL,
    NULL,
    NULL,
    4,
    0,
    0,
    WM_DI_GREEDY2FRAME_GETVALUE - WM_APP,
    NULL,
    0,
    FALSE,
    FALSE,
    IDH_GREEDY2
};

DEINTERLACE_METHOD* DI_Greedy2Frame_GetDeinterlacePluginInfo(long CpuFeatureFlags)
{
    Greedy2FrameMethod.pfnAlgorithm = SIMD_SELECT (DeinterlaceGreedy2Frame);
    return &Greedy2FrameMethod;
}

#endif /* !SIMD */

/*
Local Variables:
c-basic-offset: 4
End:
 */