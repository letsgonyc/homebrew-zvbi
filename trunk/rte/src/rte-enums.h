#ifndef __RTE_ENUMS_H__
#define __RTE_ENUMS_H__

/* Stream types */
typedef enum {
  RTE_STREAM_VIDEO = 0,  /* XXX STREAM :-( need a better term */
  RTE_STREAM_AUDIO,	 /* input/output distinction? */
  RTE_STREAM_SLICED_VBI,
  /* ... */
  RTE_STREAM_MAX = 15
} rte_stream_type;

/*
  Source parameters
  FIXME: A link to a doc explaining this modes would be nice (V4L2
  api perhaps)
 */
typedef enum {
  RTE_PIXFMT_YUV420 = 1,
  RTE_PIXFMT_YUYV,
  RTE_PIXFMT_YVYU,
  RTE_PIXFMT_UYVY,
  RTE_PIXFMT_VYUY,
  RTE_PIXFMT_RGB32,
  RTE_PIXFMT_BGR32,
  RTE_PIXFMT_RGB24,
  RTE_PIXFMT_BGR24,
  RTE_PIXFMT_RGB16,
  RTE_PIXFMT_BGR16,
  RTE_PIXFMT_RGB15,
  RTE_PIXFMT_BGR15,
  /* ... */
  RTE_PIXFMT_MAX = 31
} rte_pixfmt;

typedef enum {
  RTE_SNDFMT_S8 = 1,
  RTE_SNDFMT_U8,
  RTE_SNDFMT_S16LE,
  RTE_SNDFMT_S16BE,
  RTE_SNDFMT_U16LE,
  RTE_SNDFMT_U16BE,
  /* ... */
  RTE_SNDFMT_MAX = 31
} rte_sndfmt;

/*
  FIXME: Explain what's VBI and provide links to the appropiate specs
  (when publically available).
 */
typedef enum {
  RTE_VBIFMT_TELETEXT_B_L10_625 = 1,
  RTE_VBIFMT_TELETEXT_B_L25_625,
  RTE_VBIFMT_VPS,
  RTE_VBIFMT_CAPTION_625_F1,
  RTE_VBIFMT_CAPTION_625,
  RTE_VBIFMT_CAPTION_525_F1,
  RTE_VBIFMT_CAPTION_525,
  RTE_VBIFMT_2xCAPTION_525,
  RTE_VBIFMT_NABTS,
  RTE_VBIFMT_TELETEXT_BD_525,
  RTE_VBIFMT_WSS_625,
  RTE_VBIFMT_WSS_CPR1204,
  /* ... */
  RTE_VBIFMT_RESERVED1 = 30,
  RTE_VBIFMT_RESERVED2 = 31
} rte_vbifmt;

#define RTE_VBIFMTS_TELETEXT_B_L10_625	(1UL << RTE_VBIFMT_TELETEXT_B_L10_625)
#define RTE_VBIFMTS_TELETEXT_B_L25_625	(1UL << RTE_VBIFMT_TELETEXT_B_L25_625)
#define RTE_VBIFMTS_VPS			(1UL << RTE_VBIFMT_VPS)
#define RTE_VBIFMTS_CAPTION_625_F1	(1UL << RTE_VBIFMT_CAPTION_625_F1)
#define RTE_VBIFMTS_CAPTION_625		(1UL << RTE_VBIFMT_CAPTION_625)
#define RTE_VBIFMTS_CAPTION_525_F1	(1UL << RTE_VBIFMT_CAPTION_525_F1)
#define RTE_VBIFMTS_CAPTION_525		(1UL << RTE_VBIFMT_CAPTION_525)
#define RTE_VBIFMTS_2xCAPTION_525	(1UL << RTE_VBIFMT_2xCAPTION_525)
#define RTE_VBIFMTS_NABTS		(1UL << RTE_VBIFMT_NABTS)
#define RTE_VBIFMTS_TELETEXT_BD_525	(1UL << RTE_VBIFMT_TELETEXT_BD_525)
#define RTE_VBIFMTS_WSS_625		(1UL << RTE_VBIFMT_WSS_625)
#define RTE_VBIFMTS_WSS_CPR1204		(1UL << RTE_VBIFMT_WSS_CPR1204)
#define RTE_VBIFMTS_RESERVED1		(1UL << RTE_VBIFMT_RESERVED1)
#define RTE_VBIFMTS_RESERVED2		(1UL << RTE_VBIFMT_RESERVED2)

#endif /* rte-enums.h */
