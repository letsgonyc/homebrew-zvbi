/*
 *  Copyright (C) 2001-2004 Michael H. Schimek
 *  Copyright (C) 2000-2003 I�aki Garc�a Etxebarria
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

/* $Id: pixel_format.h,v 1.3 2005-01-08 14:54:19 mschimek Exp $ */

#ifndef __ZTV_PIXEL_FORMAT_H__
#define __ZTV_PIXEL_FORMAT_H__

#include <inttypes.h>		/* uint64_t */
#include "macros.h"

TV_BEGIN_DECLS

typedef enum {
	TV_PIXFMT_NONE,
	TV_PIXFMT_UNKNOWN = TV_PIXFMT_NONE,

	TV_PIXFMT_RESERVED0,

	/* Planar YUV formats */

	TV_PIXFMT_YUV444,		/* 4x4 4x4 4x4 */
	TV_PIXFMT_YVU444,
	TV_PIXFMT_YUV422,		/* 4x4 2x4 2x4 */
	TV_PIXFMT_YVU422,
	TV_PIXFMT_YUV411,		/* 4x4 1x4 1x4 */
	TV_PIXFMT_YVU411,
	TV_PIXFMT_YUV420,		/* 4x4 2x2 2x2 */
	TV_PIXFMT_YVU420,
	TV_PIXFMT_YUV410,		/* 4x4 1x1 1x1 */
	TV_PIXFMT_YVU410,

	/* Packed YUV formats */	/* reg msb..lsb -> memory byte 0..3 */

	TV_PIXFMT_YUVA32_LE,		/* AVUY -> LE  Y U V A  BE  A V U Y */
	TV_PIXFMT_YUVA32_BE,
	TV_PIXFMT_YVUA32_LE,		/* AUVY -> LE  Y V U A  BE  A U V Y */
	TV_PIXFMT_YVUA32_BE,

	TV_PIXFMT_AVUY32_BE = TV_PIXFMT_YUVA32_LE,
	TV_PIXFMT_AVUY32_LE,		/* YUVA -> LE  A V U Y  BE  Y U V A */
	TV_PIXFMT_AUVY32_BE,
	TV_PIXFMT_AUVY32_LE,		/* YVUA -> LE  A U V Y  BE  Y V U A */

	TV_PIXFMT_YUV24_LE,		/*  VUY -> LE  Y U V  BE  V U Y */
	TV_PIXFMT_YUV24_BE,
	TV_PIXFMT_YVU24_LE,		/*  UVY -> LE  Y V U  BE  U V Y */
	TV_PIXFMT_YVU24_BE,

	TV_PIXFMT_VUY24_BE = TV_PIXFMT_YUV24_LE,
	TV_PIXFMT_VUY24_LE,		/*  YUV -> LE  V U Y  BE  Y U V */
	TV_PIXFMT_UVY24_BE,
	TV_PIXFMT_UVY24_LE,		/*  YVU -> LE  U V Y  BE  Y V U */

	TV_PIXFMT_YUYV,			/* Y0 U Y1 V in memory */
	TV_PIXFMT_YVYU,			/* Y0 V Y1 U */
	TV_PIXFMT_UYVY,			/* U Y0 V Y1 */
	TV_PIXFMT_VYUY,			/* V Y0 U Y1 */

	TV_PIXFMT_RESERVED1,
	TV_PIXFMT_Y8,			/* Y */

	TV_PIXFMT_RESERVED2,
	TV_PIXFMT_RESERVED3,

	/* Packed RGB formats */

	TV_PIXFMT_RGBA32_LE,		/* ABGR -> LE  R G B A  BE  A B G R */
	TV_PIXFMT_RGBA32_BE,
	TV_PIXFMT_BGRA32_LE,		/* ARGB -> LE  B G R A  BE  A R G B */
	TV_PIXFMT_BGRA32_BE,

	TV_PIXFMT_ABGR32_BE = TV_PIXFMT_RGBA32_LE,
	TV_PIXFMT_ABGR32_LE,		/* RGBA -> LE  A B G R  BE  R G B A */
	TV_PIXFMT_ARGB32_BE,
	TV_PIXFMT_ARGB32_LE,		/* BGRA -> LE  A R G B  BE  B G R A */

	TV_PIXFMT_RGB24_LE,		/*  BGR -> LE  R G B  BE  B G R */
	TV_PIXFMT_BGR24_LE,		/*  RGB -> LE  B G R  BE  R G B */

	TV_PIXFMT_BGR24_BE = TV_PIXFMT_RGB24_LE,
	TV_PIXFMT_RGB24_BE,

	TV_PIXFMT_RGB16_LE,		/* bbbbbggggggrrrrr msb..lsb */
	TV_PIXFMT_RGB16_BE,
	TV_PIXFMT_BGR16_LE,		/* rrrrrggggggbbbbb */
	TV_PIXFMT_BGR16_BE,

	TV_PIXFMT_RGBA16_LE,		/* abbbbbgggggrrrrr */
	TV_PIXFMT_RGBA16_BE,
	TV_PIXFMT_BGRA16_LE,		/* arrrrrgggggbbbbb */
	TV_PIXFMT_BGRA16_BE,
	TV_PIXFMT_ARGB16_LE,		/* bbbbbgggggrrrrra */
	TV_PIXFMT_ARGB16_BE,
	TV_PIXFMT_ABGR16_LE,		/* rrrrrgggggbbbbba */
	TV_PIXFMT_ABGR16_BE,

	TV_PIXFMT_RGBA12_LE,		/* aaaabbbbggggrrrr */
	TV_PIXFMT_RGBA12_BE,
	TV_PIXFMT_BGRA12_LE,		/* aaaarrrrggggbbbb */
	TV_PIXFMT_BGRA12_BE,
	TV_PIXFMT_ARGB12_LE,		/* bbbbggggrrrraaaa */
	TV_PIXFMT_ARGB12_BE,
	TV_PIXFMT_ABGR12_LE,		/* rrrrggggbbbbaaaa */
	TV_PIXFMT_ABGR12_BE,

	TV_PIXFMT_RGB8,			/* bbgggrrr */
	TV_PIXFMT_BGR8,			/* rrrgggbb */

	TV_PIXFMT_RGBA8,		/* abbgggrr */
	TV_PIXFMT_BGRA8,		/* arrgggbb */
	TV_PIXFMT_ARGB8,		/* bbgggrra */
	TV_PIXFMT_ABGR8,		/* rrgggbba */
} tv_pixfmt;

#define TV_MAX_PIXFMTS 64

typedef uint64_t tv_pixfmt_set;

#define TV_PIXFMT_SET(pixfmt) (((tv_pixfmt_set) 1) << (pixfmt))

#define TV_PIXFMT_SET_UNKNOWN 0
#define TV_PIXFMT_SET_EMPTY 0
#define TV_PIXFMT_SET_YUV_PLANAR (+ TV_PIXFMT_SET (TV_PIXFMT_YUV444)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YVU444)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YUV422)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YVU422)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YUV411)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YVU411)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YUV420)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YVU420)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YUV410)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YVU410))
#define TV_PIXFMT_SET_YUVA32     (+ TV_PIXFMT_SET (TV_PIXFMT_YUVA32_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YUVA32_BE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YVUA32_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YVUA32_BE))
#define TV_PIXFMT_SET_YUV24	 (+ TV_PIXFMT_SET (TV_PIXFMT_YUV24_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YUV24_BE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YVU24_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YVU24_BE))
#define TV_PIXFMT_SET_YUV16	 (+ TV_PIXFMT_SET (TV_PIXFMT_YUYV)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YVYU)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_UYVY)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_VYUY))
#define TV_PIXFMT_SET_YUV_PACKED (+ TV_PIXFMT_SET_YUVA32		\
				  + TV_PIXFMT_SET_YUV24			\
				  + TV_PIXFMT_SET_YUV16			\
				  + TV_PIXFMT_SET (TV_PIXFMT_Y8))
#define TV_PIXFMT_SET_YUV	 (+ TV_PIXFMT_SET_YUV_PLANAR		\
				  + TV_PIXFMT_SET_YUV_PACKED)
#define TV_PIXFMT_SET_RGBA32	 (+ TV_PIXFMT_SET (TV_PIXFMT_RGBA32_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_RGBA32_BE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_BGRA32_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_BGRA32_BE))
#define TV_PIXFMT_SET_RGB24	 (+ TV_PIXFMT_SET (TV_PIXFMT_RGB24_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_BGR24_LE))
#define TV_PIXFMT_SET_RGB16	 (+ TV_PIXFMT_SET (TV_PIXFMT_RGB16_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_RGB16_BE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_BGR16_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_BGR16_BE))
#define TV_PIXFMT_SET_RGBA16	 (+ TV_PIXFMT_SET (TV_PIXFMT_RGBA16_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_RGBA16_BE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_BGRA16_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_BGRA16_BE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_ARGB16_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_ARGB16_BE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_ABGR16_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_ABGR16_BE))
#define TV_PIXFMT_SET_RGBA12	 (+ TV_PIXFMT_SET (TV_PIXFMT_RGBA12_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_RGBA12_BE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_BGRA12_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_BGRA12_BE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_ARGB12_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_ARGB12_BE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_ABGR12_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_ABGR12_BE))
#define TV_PIXFMT_SET_RGB8	 (+ TV_PIXFMT_SET (TV_PIXFMT_RGB8)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_BGR8))
#define TV_PIXFMT_SET_RGBA8	 (+ TV_PIXFMT_SET (TV_PIXFMT_RGBA8)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_BGRA8)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_ARGB8)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_ABGR8))
#define TV_PIXFMT_SET_RGB_PACKED (+ TV_PIXFMT_SET_RGBA32		\
				  + TV_PIXFMT_SET_RGB24			\
				  + TV_PIXFMT_SET_RGB16			\
				  + TV_PIXFMT_SET_RGBA16		\
				  + TV_PIXFMT_SET_RGBA12		\
				  + TV_PIXFMT_SET_RGB8			\
				  + TV_PIXFMT_SET_RGBA8)
#define TV_PIXFMT_SET_RGB	    TV_PIXFMT_SET_RGB_PACKED
#define TV_PIXFMT_SET_PLANAR	    TV_PIXFMT_SET_YUV_PLANAR
#define TV_PIXFMT_SET_PACKED	 (+ TV_PIXFMT_SET_YUV_PACKED		\
				  + TV_PIXFMT_SET_RGB_PACKED)
#define TV_PIXFMT_SET_ALL	 (+ TV_PIXFMT_SET_YUV			\
				  + TV_PIXFMT_SET_RGB)

#define TV_PIXFMT_IS_YUV(pixfmt)					\
	(0 != (TV_PIXFMT_SET (pixfmt) & TV_PIXFMT_SET_YUV))
#define TV_PIXFMT_IS_RGB(pixfmt)					\
	(0 != (TV_PIXFMT_SET (pixfmt) & TV_PIXFMT_SET_RGB))
#define TV_PIXFMT_IS_PLANAR(pixfmt)					\
	(0 != (TV_PIXFMT_SET (pixfmt) & TV_PIXFMT_SET_PLANAR))
#define TV_PIXFMT_IS_PACKED(pixfmt)					\
	(0 != (TV_PIXFMT_SET (pixfmt) & TV_PIXFMT_SET_PACKED))

#ifdef __GNUC__
#define TV_PIXFMT_BYTES_PER_PIXEL(pixfmt)				\
	(!__builtin_constant_p (pixfmt) ?				\
	 tv_pixfmt_bytes_per_pixel (pixfmt) :				\
	 ((TV_PIXFMT_SET (pixfmt) & TV_PIXFMT_SET_YUVA32) ? 4 :		\
	  ((TV_PIXFMT_SET (pixfmt) & TV_PIXFMT_SET_RGBA32) ? 4 :	\
	   ((TV_PIXFMT_SET (pixfmt) & TV_PIXFMT_SET_YUV24) ? 3 :	\
	    ((TV_PIXFMT_SET (pixfmt) & TV_PIXFMT_SET_RGB24) ? 3 :	\
	     ((TV_PIXFMT_SET (pixfmt) & TV_PIXFMT_SET_YUV16) ? 2 :	\
	      ((TV_PIXFMT_SET (pixfmt) & TV_PIXFMT_SET_RGB16) ? 2 :	\
	       ((TV_PIXFMT_SET (pixfmt) & TV_PIXFMT_SET_RGBA16) ? 2 :	\
	        ((TV_PIXFMT_SET (pixfmt) & TV_PIXFMT_SET_RGBA12) ? 2 :	\
		 1))))))))
#else
#define TV_PIXFMT_BYTES_PER_PIXEL(pixfmt)				\
	(tv_pixfmt_bytes_per_pixel (pixfmt))
#endif

/** Color space identifier. No values defined yet. */
typedef enum {
	TV_COLSPC_NONE,					/**< */
	TV_COLSPC_UNKNOWN = TV_COLSPC_NONE,		/**< */
	/** Unspecified RGB color space. */
	TV_COLSPC_RGB,
	/** Unspecified YUV (YCbCr) color space. */
	TV_COLSPC_YUV,
} tv_colspc;

extern const char *
tv_pixfmt_name			(tv_pixfmt		pixfmt);
extern unsigned int
tv_pixfmt_bytes_per_pixel	(tv_pixfmt		pixfmt);

/* Broken-down pixel format */

typedef struct {
	const char *		name;

	tv_pixfmt		pixfmt;
        tv_colspc		colspc;

	/* Number of bits per pixel. For packed YUV 4:2:2 this is 16.
	   For planar formats this refers to the Y plane only. */
	unsigned int		bits_per_pixel;

	/* Number of red, green and blue, or luma and chroma bits
	   per pixel. Averaged if U and V plane are smaller than Y plane. */
	unsigned int		color_depth;

	/* Width and height of the U and V plane:
	   uv_width = width >> uv_hshift,
	   uv_height = height >> uv_vshift. */
	unsigned int		uv_hshift;
	unsigned int		uv_vshift;

	/* Format is packed and pixels are stored in 16, 24 or 32 bit
	   (bits_per_pixel) quantities with most significant byte
	   first in memory. */
	tv_bool			big_endian;

	/* Y, U and V color components are stored in separate arrays,
	   first Y, then U and V. */
	tv_bool			planar;

	/* For packed YUV 4:2:2, V pixel is stored before U pixel
	   in memory. For planar YUV formats, V plane is stored
	   before U plane in memory. */
	tv_bool			vu_order;

	tv_bool			_reserved2[5];

	/* Bit masks describing size and position of color components
	   in a 8, 16, 24 or 32 bit (bits_per_pixel) quantity, as
	   seen when reading a word from memory with proper endianess.
	   For packed YUV 4:2:2 and planar formats y, u and v will be
	   0xFF. The a (alpha) component can be zero. */
	union {
		struct {
			unsigned int		r;
			unsigned int		g;
			unsigned int		b;
			unsigned int		a;
		}			rgb;
		struct {
			unsigned int		y;
			unsigned int		u;
			unsigned int		v;
			unsigned int		a;
		}			yuv;
	}			mask;
} tv_pixel_format;

extern const tv_pixel_format *
tv_pixel_format_from_pixfmt	(tv_pixfmt		pixfmt);
extern tv_pixfmt
tv_pixel_format_to_pixfmt	(const tv_pixel_format *format);

TV_END_DECLS

#endif /* __ZTV_PIXEL_FORMAT_H__ */
