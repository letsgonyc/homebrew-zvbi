/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000 I�aki Garc�a Etxebarria
 * Copyright (C) 2003 Michael H. Schimek
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "site_def.h"

/*
  This is the library in charge of simplifying Video Access API (I
  don't want to use thirteen lines of code with ioctl's every time I
  want to change tuning freq).
  the name is TV Engine, since it is intended mainly for TV viewing.
  This file is separated so zapping doesn't need to know about V4L[2]
*/


#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef ENABLE_V4L
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <errno.h>
#include <linux/kernel.h>
#include <ctype.h>

#include "tveng.h"
#define TVENG25_PROTOTYPES 1
#include "tveng25.h"
#include "tvengxv.h"

#include "zmisc.h"

/* Kernel interface */
#include <linux/types.h>	/* __u32 etc */
#include "common/videodev.h"
#include "common/_videodev.h"
#include "common/videodev25.h"
#include "common/_videodev25.h"

/* This is not a V4L2 pixfmt, and it's not returned by any driver.
   The mplayer folks made it up for the ivtv YUV interface. */
#define FOURCC_HM12 v4l2_fourcc ('H', 'M', '1', '2')

static void
fprint_ioctl_arg		(FILE *			fp,
				 unsigned int		cmd,
				 int			rw,
				 void *			arg)
{
	if (0 == ((cmd ^ VIDIOC_QUERYCAP) & (_IOC_TYPEMASK << _IOC_TYPESHIFT)))
		fprint_v4l25_ioctl_arg (fp, cmd, rw, arg);
	else
		fprint_v4l_ioctl_arg (fp, cmd, rw, arg);
}

/* This macro checks at compile time if the arg type is correct,
   device_ioctl() repeats the ioctl if interrupted (EINTR) and logs
   the args and result if log_fp is non-zero. When the ioctl failed
   ioctl_failure() stores the cmd, caller and errno in info. */
#define xioctl(info, cmd, arg)						\
	(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),				\
	 ((0 == device_ioctl ((info)->log_fp, fprint_ioctl_arg,		\
			      (info)->fd, cmd, (void *)(arg))) ?	\
	  0 : (ioctl_failure (info, __FILE__, __FUNCTION__,		\
			      __LINE__, # cmd), -1)))

#define xioctl_may_fail(info, cmd, arg)					\
	(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),				\
	 device_ioctl ((info)->log_fp, fprint_ioctl_arg,		\
		       (info)->fd, cmd, (void *)(arg)))

/** @internal */
struct xcontrol {
	tv_control		pub;

	/** V4L2_CID_. */
	unsigned int		id;
};

#define C(l) PARENT (l, struct xcontrol, pub)

struct video_input {
	tv_video_line		pub;

	int			index;		/* struct v4l2_input */
	unsigned int		tuner;		/* struct v4l2_tuner */

	unsigned int		step_shift;

	/* Only applicable if tuner. */
	tv_audio_capability	audio_capability;
};

#define VI(l) PARENT (l, struct video_input, pub)
#define CVI(l) CONST_PARENT (l, struct video_input, pub)

struct audio_input {
	tv_audio_line		pub;

	unsigned int		index;		/* struct v4l2_audio */
};

#define AI(l) PARENT (l, struct audio_input, pub)
#define CAI(l) CONST_PARENT (l, struct audio_input, pub)

/** @internal */
struct xbuffer {
	tv_capture_buffer	cb;

	/** Data from VIDIOC_REQBUFS, updated after
	    VIDIOC_QBUF, VIDIOC_DQBUF. */
	struct v4l2_buffer	vb;

	/** Client got a cb pointer with dequeue_buffer(). */
	tv_bool			dequeued;

	/** Clear this buffer before next VIDIOC_QBUF. */
	tv_bool			clear;
};

#ifndef TVENG25_BAYER_TEST
#  define TVENG25_BAYER_TEST 0
#endif
#ifndef TVENG25_HM12_TEST
#  define TVENG25_HM12_TEST 0
#endif
#ifndef TVENG25_XV_TEST
#  define TVENG25_XV_TEST 0
#endif
#ifndef TVENG25_NOVERLAY_TEST
#  define TVENG25_NOVERLAY_TEST 0
#endif

struct private_tveng25_device_info
{
  tveng_device_info info; /* Info field, inherited */
  struct timeval last_timestamp; /* The timestamp of the last captured buffer */
  uint32_t killme_chroma;
  int audio_mode; /* 0 mono */
	char *			reopen_name;
	int			reopen_flags;

	struct v4l2_capability	caps;

	struct xbuffer *	buffers;

	tv_control *		mute;

	unsigned int		bttv_driver;
	unsigned int		ivtv_driver;

	tv_bool			read_back_controls;
	tv_bool			use_v4l_audio;
	tv_bool			use_v4l_freq;
	tv_bool			use_s_ctrl_old;

#if defined (HAVE_XV_EXTENSION) && TVENG25_XV_TEST
	/* Grabbed the info->overlay.xv_port_id for overlay. */
	tv_bool			grabbed_xv_port;

	Window			xwindow;
	GC			xgc;

	Atom			xa_xv_brightness;
#endif

	/* Capturing has been started (STREAMON). */
	tv_bool			capturing;

	/* Capturing has been started and we called read() at least once. */
	tv_bool			reading;
};

#define P_INFO(p) PARENT (p, struct private_tveng25_device_info, info)

/* For tests only. */
static int
g_fmt				(tveng_device_info *	info,
				 struct v4l2_format *	f)
{
	struct v4l2_format format;
	int r;

	format = *f;

	r = xioctl (info, VIDIOC_G_FMT, &format);
	if (-1 == r)
		return -1;

	/* This should actually go into tvengemu.c. */
	if (TVENG25_BAYER_TEST) {
		if (V4L2_PIX_FMT_GREY != format.fmt.pix.pixelformat) {
			format.fmt.pix.pixelformat = V4L2_PIX_FMT_GREY;
			format.fmt.pix.bytesperline = 0; /* minimum */
			format.fmt.pix.sizeimage = 0; /* minimum */

			r = xioctl (info, VIDIOC_S_FMT, &format);
			if (-1 == r)
				return -1;
		}

		/* Simulate BAYER format device. */
		format.fmt.pix.pixelformat = V4L2_PIX_FMT_SBGGR8;
	} else if (TVENG25_HM12_TEST) {
		unsigned int req_width;
		unsigned int req_height;

		if (720 == TVENG25_HM12_TEST) {
			req_width = 720;
			req_height = 480;
		} else {
			req_width = format.fmt.pix.width;
			req_height = format.fmt.pix.height;
		}

		if (V4L2_PIX_FMT_YVU420 != format.fmt.pix.pixelformat
		    || req_width != format.fmt.pix.width
		    || req_height != format.fmt.pix.height) {
			format.fmt.pix.pixelformat = V4L2_PIX_FMT_YVU420;
			format.fmt.pix.width = req_width;
			format.fmt.pix.height = req_height;
			format.fmt.pix.bytesperline = 0; /* minimum */
			format.fmt.pix.sizeimage = 0; /* minimum */

			r = xioctl (info, VIDIOC_S_FMT, &format);
			if (-1 == r)
				return -1;
		}

		/* Simulate what ivtv 0.6 returns. */
		format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV; /* wrong */
		format.fmt.pix.bytesperline = 0; /* wrong */
		format.fmt.pix.sizeimage =
			format.fmt.pix.width
			* format.fmt.pix.height * 3 / 2;
	}

	*f = format;

	return r;
}

static int
s_fmt				(tveng_device_info *	info,
				 struct v4l2_format *	f,
				 tv_bool			may_fail)
{
	struct v4l2_format format;
	int r;

	format = *f;

	if (TVENG25_BAYER_TEST) {
		format.fmt.pix.pixelformat = V4L2_PIX_FMT_GREY;

		format.fmt.pix.width	= 352;
		format.fmt.pix.height	= 288;
	} else if (TVENG25_HM12_TEST) {
		if (720 == TVENG25_HM12_TEST
		    && (720 != format.fmt.pix.width
			|| 480 != format.fmt.pix.height)) {
			if (NULL != info->log_fp) {
				fprintf (info->log_fp,
					 "-1 = simVIDIOC_S_FMT (");
				fprint_ioctl_arg (info->log_fp,
						  VIDIOC_S_FMT,
						  /* r & w */ 3,
						  &format);
				fprintf (info->log_fp,
					 "), errno = %d, %s\n",
					 EBUSY, strerror (EBUSY));
			}
	
			errno = EBUSY;

			return -1;
		}

		format.fmt.pix.pixelformat = V4L2_PIX_FMT_YVU420;
	}

	if (may_fail) {
		r = xioctl_may_fail (info, VIDIOC_S_FMT, &format);
	} else {
		r = xioctl (info, VIDIOC_S_FMT, &format);
	}

	if (-1 == r)
		return -1;

	if (TVENG25_BAYER_TEST) {
		format.fmt.pix.pixelformat = V4L2_PIX_FMT_SBGGR8;
	} else if (TVENG25_HM12_TEST) {
		format.fmt.pix.pixelformat = FOURCC_HM12;

		assert (0 == (format.fmt.pix.width & 15));
		assert (0 == (format.fmt.pix.height & 15));
	}

	*f = format;

	return r;
}

static tv_bool
streamoff_or_reopen		(tveng_device_info *	info)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);
	int fd;

	/* Some drivers may recognize VIDIOC_STREAMOFF to stop
	   capturing in read() mode, to permit format changes etc.
	   The ioctl succeeds with ivtv but subsequent read select()
	   calls time out? */
	if (0 == p_info->ivtv_driver) {
		int buf_type;

		buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (0 == xioctl_may_fail (info, VIDIOC_STREAMOFF, &buf_type))
			goto success;

		if (p_info->caps.capabilities & V4L2_CAP_STREAMING) {
			/* VIDIOC_STREAMOFF should work. */
			ioctl_failure (info,
				       __FILE__,
				       __FUNCTION__,
				       __LINE__,
				       "VIDIOC_STREAMOFF");
			return FALSE;
		}
	}

	/* Reopening the device should also work. */
	/* XXX According to the V4L2 spec this shouldn't reset any
	   parameters, but it may be wiser to check that. */
	fd = device_open (info->log_fp,
			  p_info->reopen_name,
			  p_info->reopen_flags,
			  /* mode */ 0);
	if (-1 == fd) {
		return FALSE;
	}

	device_close (info->log_fp, info->fd);

	info->fd = fd;

 success:
	p_info->reading = FALSE;
	p_info->capturing = FALSE;

	return TRUE;
}

static tv_bool
queue_xbuffers			(tveng_device_info *	info);
static tv_bool
unmap_xbuffers			(tveng_device_info *	info,
				 tv_bool		ignore_errors);

static __inline__ char *
xstrndup			(const __u8 *		s,
				 size_t			size)
{
	/* strndup is a GNU extension. */
	return _tv_strndup ((const char *) s, size);
}

/* xstrndup a __u8 array. */
#undef XSTRADUP
#define XSTRADUP(s) xstrndup ((s), N_ELEMENTS (s))

static __inline__ int
xstrncmp			(const __u8 *		s1,
				 const char *		s2,
				 size_t			size)
{
	return strncmp ((const char *) s1, s2, size);
}

/* xstrncmp a __u8 array. */
#undef XSTRACMP
#define XSTRACMP(s1, s2) xstrncmp ((s1), (s2), N_ELEMENTS (s1))


/*
	Audio matrix
 */

static unsigned int
tv_audio_mode_to_audmode	(tv_audio_mode		mode)
{
	switch (mode) {
	case TV_AUDIO_MODE_AUTO:
	case TV_AUDIO_MODE_LANG1_MONO:
		return V4L2_TUNER_MODE_MONO;

	case TV_AUDIO_MODE_LANG1_STEREO:
		return V4L2_TUNER_MODE_STEREO;

	case TV_AUDIO_MODE_LANG2_MONO:
		return V4L2_TUNER_MODE_LANG2;
	}

	assert (!"reached");

	return 0;
}

static unsigned int
tv_audio_mode_to_v4l_mode	(tv_audio_mode		mode)
{
	switch (mode) {
	case TV_AUDIO_MODE_AUTO:
		return 0;

	case TV_AUDIO_MODE_LANG1_MONO:
		return VIDEO_SOUND_MONO;

	case TV_AUDIO_MODE_LANG1_STEREO:
		return VIDEO_SOUND_STEREO;

	case TV_AUDIO_MODE_LANG2_MONO:
		return VIDEO_SOUND_LANG2;
	}

	assert (!"reached");

	return 0;
}

#if 0 /* Not used yet. */

static tv_audio_mode
tv_audio_mode_from_audmode	(unsigned int		mode)
{
	switch (mode) {
	case V4L2_TUNER_MODE_MONO:
	case V4L2_TUNER_MODE_LANG1:
	default:
		return TV_AUDIO_MODE_LANG1_MONO;

	case V4L2_TUNER_MODE_STEREO:
		return TV_AUDIO_MODE_LANG1_STEREO;

	case V4L2_TUNER_MODE_LANG2:
		return TV_AUDIO_MODE_LANG2_MONO;
	}
}

#endif

static void
store_audio_capability		(tveng_device_info *	info)
{
	static const tv_audio_capability lang2 =
		TV_AUDIO_CAPABILITY_SAP |
		TV_AUDIO_CAPABILITY_BILINGUAL;
	tv_audio_capability cap;

	cap = TV_AUDIO_CAPABILITY_EMPTY;

	if (info->panel.cur_video_input
	    && IS_TUNER_LINE (info->panel.cur_video_input)) {
		cap = VI(info->panel.cur_video_input)->audio_capability;

		if (cap & lang2) {
			const tv_video_standard *s;

			cap &= ~lang2;

			s = info->panel.cur_video_standard;

			if (s && (s->videostd_set
				  & TV_VIDEOSTD_SET (TV_VIDEOSTD_NTSC_M))) {
				cap |= TV_AUDIO_CAPABILITY_SAP;
			} else {
				cap |= TV_AUDIO_CAPABILITY_BILINGUAL;
			}
		}
	}

	if (info->panel.audio_capability != cap) {
		info->panel.audio_capability = cap;

		tv_callback_notify (info, info, info->panel.audio_callback);
	}
}

static void
store_audio_reception		(tveng_device_info *	info,
				 unsigned int		rxsubchans)
{
	unsigned int rec[2];

	rec[0] = 0;
	rec[1] = 0;

	if (rxsubchans & V4L2_TUNER_SUB_STEREO)
		rec[0] = 2;
	else if (rxsubchans & V4L2_TUNER_SUB_MONO)
		rec[0] = 1;

	if (rxsubchans & V4L2_TUNER_SUB_LANG2)
		rec[1] = 1;

	if (info->panel.audio_reception[0] != rec[0]
	    || info->panel.audio_reception[1] != rec[1]) {
		info->panel.audio_reception[0] = rec[0];
		info->panel.audio_reception[1] = rec[1];

		tv_callback_notify (info, info, info->panel.audio_callback);
	}
}

static tv_bool
set_audio_mode			(tveng_device_info *	info,
				 tv_audio_mode		mode)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);

	if (info->panel.cur_video_input
	    && IS_TUNER_LINE (info->panel.cur_video_input)) {
		if (p_info->use_v4l_audio) {
			struct video_audio audio;

			if (-1 == xioctl (info, VIDIOCGAUDIO, &audio)) {
				return FALSE;
			}

			if (p_info->mute && p_info->mute->value)
				audio.flags |= VIDEO_AUDIO_MUTE;
			else
				audio.flags &= ~VIDEO_AUDIO_MUTE;

			audio.mode = tv_audio_mode_to_v4l_mode (mode);

			if (-1 == xioctl (info, VIDIOCSAUDIO, &audio)) {
				return FALSE;
			}
		} else {
			struct v4l2_tuner tuner;
			unsigned int audmode;

			CLEAR (tuner);
			tuner.index = VI(info->panel.cur_video_input)->tuner;

			if (-1 == xioctl (info, VIDIOC_G_TUNER, &tuner))
				return FALSE;

			store_audio_reception (info, tuner.rxsubchans);

			audmode = tv_audio_mode_to_audmode (mode);

			if (tuner.audmode != audmode) {
				tuner.audmode = audmode;

				if (-1 == xioctl (info, VIDIOC_S_TUNER,
						  &tuner)) {
					return FALSE;
				}
			}
		}
	}

	if (info->panel.audio_mode != mode) {
		info->panel.audio_mode = mode;

		tv_callback_notify (info, info, info->panel.audio_callback);
	}

	return TRUE;
}

/*
	Controls
*/

static tv_bool
get_xcontrol			(tveng_device_info *	info,
				 struct xcontrol *	c)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);
	struct v4l2_control ctrl;

	if (c->pub.id == TV_CONTROL_ID_AUDIO_MODE)
		return TRUE; /* XXX */

	if (c->id == V4L2_CID_AUDIO_MUTE) {
		/* Doesn't seem to work with bttv.
		   FIXME we should check at runtime. */
		return TRUE;
	}

	CLEAR (ctrl);
	ctrl.id = c->id;

	if (-1 == xioctl (&p_info->info, VIDIOC_G_CTRL, &ctrl))
		return FALSE;

	if (c->pub.value != ctrl.value) {
		c->pub.value = ctrl.value;
		tv_callback_notify (&p_info->info, &c->pub, c->pub._callback);
	}

	return TRUE;
}

static tv_bool
get_control			(tveng_device_info *	info,
				 tv_control *		c)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);

	if (c)
		return get_xcontrol (info, C(c));

	for_all (c, p_info->info.panel.controls)
		if (c->_parent == info)
			if (!get_xcontrol (info, C(c)))
				return FALSE;

	return TRUE;
}

static tv_bool
s_ctrl				(tveng_device_info *	info,
				 const struct v4l2_control *c)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);
	struct v4l2_control ctrl;

	ctrl = *c;

	if (p_info->use_s_ctrl_old) {
		/* Incorrectly defined as _IOW. */
		if (-1 == xioctl (info, VIDIOC_S_CTRL_OLD, &ctrl)) {
			return FALSE;
		}
	} else if (-1 == xioctl_may_fail (info, VIDIOC_S_CTRL, &ctrl)) {
		if (EINVAL != errno) {
			ioctl_failure (info,
				       __FILE__,
				       __PRETTY_FUNCTION__,
				       __LINE__,
				       "VIDIOC_S_CTRL");

			return FALSE;
		}

		ctrl = *c;

		if (-1 == xioctl (info, VIDIOC_S_CTRL_OLD, &ctrl)) {
			return FALSE;
		}

		/* We call this function quite often,
		   next time take a shortcut. */
		p_info->use_s_ctrl_old = TRUE;
	}

	return TRUE;
}

static tv_bool
set_control			(tveng_device_info *	info,
				 tv_control *		c,
				 int			value)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);
	struct v4l2_control ctrl;

	if (c->id == TV_CONTROL_ID_AUDIO_MODE)
		return set_audio_mode_control (info, c, value);

	CLEAR (ctrl);
	ctrl.id = C(c)->id;
	ctrl.value = value;

	if (!s_ctrl (info, &ctrl)) {
		return FALSE;
	}

	if (p_info->read_back_controls) {
		/* Doesn't seem to work with bttv.
		   FIXME we should check at runtime. */
		if (ctrl.id == V4L2_CID_AUDIO_MUTE) {
			if (c->value != value) {
				c->value = value;
				tv_callback_notify (info, c, c->_callback);
			}
			return TRUE;
		}

		return get_xcontrol (info, C(c));
	} else {
		if (c->value != value) {
			c->value = value;
			tv_callback_notify (info, c, c->_callback);
		}

		return TRUE;
	}
}

static const struct {
	unsigned int		cid;
	const char *		label;
	tv_control_id		id;
} controls [] = {
	{ V4L2_CID_BRIGHTNESS,		N_("Brightness"),	TV_CONTROL_ID_BRIGHTNESS },
	{ V4L2_CID_CONTRAST,		N_("Contrast"),		TV_CONTROL_ID_CONTRAST },
	{ V4L2_CID_SATURATION,		N_("Saturation"),	TV_CONTROL_ID_SATURATION },
	{ V4L2_CID_HUE,			N_("Hue"),		TV_CONTROL_ID_HUE },
	{ V4L2_CID_WHITENESS,		N_("Whiteness"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_BLACK_LEVEL,		N_("Black level"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_AUTO_WHITE_BALANCE,	N_("White balance"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_DO_WHITE_BALANCE,	N_("White balance"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_RED_BALANCE,		N_("Red balance"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_BLUE_BALANCE,	N_("Blue balance"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_GAMMA,		N_("Gamma"),		TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_EXPOSURE,		N_("Exposure"),		TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_AUTOGAIN,		N_("Auto gain"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_GAIN,		N_("Gain"),		TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_HCENTER,		N_("HCenter"),		TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_VCENTER,		N_("VCenter"),		TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_HFLIP,		N_("Hor. flipping"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_VFLIP,		N_("Vert. flipping"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_AUDIO_VOLUME,	N_("Volume"),		TV_CONTROL_ID_VOLUME },
	{ V4L2_CID_AUDIO_MUTE,		N_("Mute"),		TV_CONTROL_ID_MUTE },
	{ V4L2_CID_AUDIO_MUTE,		N_("Audio Mute"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_AUDIO_BALANCE,	N_("Balance"),		TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_AUDIO_BALANCE,	N_("Audio Balance"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_AUDIO_TREBLE,	N_("Treble"),		TV_CONTROL_ID_TREBLE },
	{ V4L2_CID_AUDIO_LOUDNESS, 	N_("Loudness"),		TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_AUDIO_BASS,		N_("Bass"),		TV_CONTROL_ID_BASS },
};

static tv_bool
add_control			(tveng_device_info *	info,
				 unsigned int		id)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);
	struct v4l2_queryctrl qc;
	struct xcontrol *xc;
	tv_control *tc;
	unsigned int i;

	CLEAR (qc);
	qc.id = id;

	/* XXX */
	if (-1 == xioctl_may_fail (info, VIDIOC_QUERYCTRL, &qc)) {
		if (EINVAL != errno)
			ioctl_failure (info,
				       __FILE__,
				       __PRETTY_FUNCTION__,
				       __LINE__,
				       "VIDIOC_QUERYCTRL");

		return FALSE;
	}

	if (qc.flags & (V4L2_CTRL_FLAG_DISABLED | V4L2_CTRL_FLAG_GRABBED))
		return TRUE;

	if (!(xc = calloc (1, sizeof (*xc))))
		return TRUE;

	for (i = 0; i < N_ELEMENTS (controls); ++i)
		if (qc.id == controls[i].cid)
			break;

	if (i < N_ELEMENTS (controls)) {
		xc->pub.label = strdup (_(controls[i].label));
		xc->pub.id = controls[i].id;
	} else {
		xc->pub.label = XSTRADUP (qc.name);
		xc->pub.id = TV_CONTROL_ID_UNKNOWN;
	}

	if (!xc->pub.label)
		goto failure;

	xc->pub.minimum	= qc.minimum;
	xc->pub.maximum	= qc.maximum;
	xc->pub.step	= qc.step;
	xc->pub.reset	= qc.default_value;
	xc->id		= qc.id;

	switch (qc.type) {
	case V4L2_CTRL_TYPE_INTEGER:
		xc->pub.type = TV_CONTROL_TYPE_INTEGER;
		break;

	case V4L2_CTRL_TYPE_BOOLEAN:
		xc->pub.type = TV_CONTROL_TYPE_BOOLEAN;

		/* Some drivers do not properly initialize these. */
		xc->pub.minimum	= 0;
		xc->pub.maximum	= 1;
		xc->pub.step	= 1;

		/* Just in case. */
		xc->pub.reset	= !!xc->pub.reset;

		break;

	case V4L2_CTRL_TYPE_BUTTON:
		xc->pub.type = TV_CONTROL_TYPE_ACTION;

		xc->pub.minimum	= INT_MIN;
		xc->pub.maximum	= INT_MAX;
		xc->pub.step	= 0;
		xc->pub.reset	= 0;

		break;

	case V4L2_CTRL_TYPE_MENU:
	{
		unsigned int n_items;
		unsigned int i;

		xc->pub.type = TV_CONTROL_TYPE_CHOICE;

		n_items = (unsigned int) qc.maximum;

		if (!(xc->pub.menu = calloc (n_items + 2, sizeof (char *))))
			goto failure;

		for (i = 0; i <= n_items; ++i) {
			struct v4l2_querymenu qm;

			CLEAR (qm);
			qm.id = qc.id;
			qm.index = i;

			if (0 == xioctl (info, VIDIOC_QUERYMENU, &qm)) {
				xc->pub.menu[i] = XSTRADUP (qm.name);
				if (NULL == xc->pub.menu[i])
					goto failure;
			} else {
				goto failure;
			}
		}

		break;
	}

	default:
		fprintf (stderr, "V4L2: Unknown control type 0x%x (%s)\n",
			 qc.type, qc.name);
		goto failure;
	}

	if (!(tc = append_panel_control (info,
					 &xc->pub, /* allocate size */ 0)))
		goto failure;

	get_xcontrol (info, C(tc));

	if (qc.id == V4L2_CID_AUDIO_MUTE) {
		p_info->mute = tc;
	}

	return TRUE;

 failure:
	tv_control_delete (&xc->pub);

	return TRUE; /* no control, but not end of enum */
}

static tv_bool
get_control_list		(tveng_device_info *	info)
{
	unsigned int cid;

	free_panel_controls (info);

	for (cid = V4L2_CID_BASE; cid < V4L2_CID_LASTP1; ++cid) {
		/* EINVAL ignored */
		add_control (info, cid);
	}

	/* Artificial control (preliminary) */
	/* XXX don't add if device has no audio. */
	append_audio_mode_control (info, (TV_AUDIO_CAPABILITY_AUTO |
					  TV_AUDIO_CAPABILITY_MONO |
					  TV_AUDIO_CAPABILITY_STEREO |
					  TV_AUDIO_CAPABILITY_SAP |
					  TV_AUDIO_CAPABILITY_BILINGUAL));

	for (cid = V4L2_CID_PRIVATE_BASE;
	     cid < V4L2_CID_PRIVATE_BASE + 100; ++cid) {
		if (!add_control (info, cid))
			break; /* end of enumeration */
	}

	return TRUE;
}

/*
 *  Video standards
 */

static tv_bool
get_video_standard		(tveng_device_info *	info)
{
	v4l2_std_id std_id;
	tv_video_standard *s;

	if (!info->panel.video_standards)
		return TRUE;

	std_id = 0;

	if (-1 == xioctl (info, VIDIOC_G_STD, &std_id))
		return FALSE;

	for_all (s, info->panel.video_standards)
		if (s->videostd_set == std_id)
			break;

	/* s = NULL = unknown. */

	store_cur_video_standard (info, s);
	store_audio_capability (info);

	return TRUE;
}

static tv_bool
set_video_standard		(tveng_device_info *	info,
				 tv_video_standard *	s)
{
	v4l2_std_id std_id;
	tv_videostd_set videostd_set;
	int r;

	std_id = 0;

	if (!TVENG25_BAYER_TEST
	    && 0 == xioctl_may_fail (info, VIDIOC_G_STD, &std_id)) {
		const tv_video_standard *t;

		for_all (t, info->panel.video_standards)
			if (t->videostd_set == std_id)
				break;

		if (t == s)
			return TRUE;
	}

	if (P_INFO (info)->capturing)
		return FALSE;

	videostd_set = s->videostd_set;

	r = -1;
	if (!TVENG25_BAYER_TEST)
		r = xioctl (info, VIDIOC_S_STD, &videostd_set);

	if (0 == r) {
		store_cur_video_standard (info, s);

		store_audio_capability (info);
	}

	return (0 == r);
}

static tv_bool
get_video_standard_list		(tveng_device_info *	info)
{
	unsigned int i;

	free_video_standards (info);

	if (!info->panel.cur_video_input)
		return TRUE;

	if (TVENG25_BAYER_TEST)
		goto failure;

	/* FIXME limit number of iterations in case the driver is broken. */
	for (i = 0;; ++i) {
		struct v4l2_standard standard;
		tv_video_standard *s;

		/* bttv 0.9.5 bug: Always returns standard.index = 0. */
		CLEAR (standard);
		standard.index = i;

		if (-1 == xioctl_may_fail (info, VIDIOC_ENUMSTD, &standard)) {
			if (EINVAL == errno)
				break; /* end of enumeration */

			ioctl_failure (info,
				       __FILE__,
				       __PRETTY_FUNCTION__,
				       __LINE__,
				       "VIDIOC_ENUMSTD");

			goto failure;
		}

		/* FIXME check if the standard is supported by the current input */

		if (!(s = append_video_standard (&info->panel.video_standards,
						 standard.id,
						 (const char *) standard.name,
						 (const char *) standard.name,
						 sizeof (*s))))
			goto failure;

		s->frame_rate =
			standard.frameperiod.denominator
			/ (double) standard.frameperiod.numerator;

		s->frame_ticks =
			90000 * standard.frameperiod.numerator
			/ standard.frameperiod.denominator;
	}

	if (get_video_standard (info))
		return TRUE;

 failure:
	free_video_standard_list (&info->panel.video_standards);
	return FALSE;
}

/*
 *  Video inputs
 */

#define SCALE_FREQUENCY(vi, freq)					\
	((((freq) & ~ (unsigned long) vi->step_shift)			\
	   * vi->pub.u.tuner.step) >> vi->step_shift)

static void
store_frequency			(tveng_device_info *	info,
				 struct video_input *	vi,
				 unsigned long		freq)
{
	unsigned int freq_hz = SCALE_FREQUENCY (vi, freq);

	if (vi->pub.u.tuner.frequency != freq_hz) {
		vi->pub.u.tuner.frequency = freq_hz;
		tv_callback_notify (info, &vi->pub, vi->pub._callback);
	}
}

static tv_bool
g_freq				(tveng_device_info *	info,
				 unsigned long *	curr_freq,
				 tv_video_line *	l)
{
	struct private_tveng25_device_info * p_info = P_INFO (info);
	unsigned long v4l_freq;

	if (!p_info->use_v4l_freq) {
		struct v4l2_frequency v4l2_freq;

		CLEAR (v4l2_freq);
		v4l2_freq.tuner = VI (l)->tuner;
		v4l2_freq.type = V4L2_TUNER_ANALOG_TV;
	
		if (0 == xioctl (info, VIDIOC_G_FREQUENCY, &v4l2_freq)) {
			*curr_freq = v4l2_freq.frequency;
			return TRUE;
		}

		if (sizeof (long) > 4)
			return FALSE;
	}

	/* v4l_compat_ioctl32 bug: Does not handle VIDIOC_G/S_FREQUENCY. */

	if (0 != VI (l)->tuner)
		return FALSE;

	v4l_freq = 0;

	if (-1 == xioctl (info, VIDIOCGFREQ, &v4l_freq))
		return FALSE;

	*curr_freq = v4l_freq;

	p_info->use_v4l_freq = TRUE;

	return TRUE;
}

static tv_bool
s_freq				(tveng_device_info *	info,
				 tv_video_line *	l,
				 unsigned long		new_freq)
{
	struct private_tveng25_device_info * p_info = P_INFO (info);
	unsigned long v4l_freq;

	if (!p_info->use_v4l_freq) {
		struct v4l2_frequency v4l2_freq;

		CLEAR (v4l2_freq);
		v4l2_freq.tuner = VI (l)->tuner;
		v4l2_freq.type = V4L2_TUNER_ANALOG_TV;
		v4l2_freq.frequency = new_freq;

		if (0 == xioctl (info, VIDIOC_S_FREQUENCY, &v4l2_freq))
			return TRUE;

		if (sizeof (long) > 4)
			return FALSE;
	}

	/* v4l_compat_ioctl32 bug: Does not handle VIDIOC_G/S_FREQUENCY. */

	if (0 != VI (l)->tuner)
		return FALSE;

	v4l_freq = new_freq;

	if (-1 == xioctl (info, VIDIOCSFREQ, &v4l_freq))
		return FALSE;

	p_info->use_v4l_freq = TRUE;

	return TRUE;
}

static tv_bool
get_tuner_frequency		(tveng_device_info *	info,
				 tv_video_line *	l)
{
	unsigned long freq;

	if (!g_freq (info, &freq, l))
		return FALSE;

	store_frequency (info, VI (l), freq);

	return TRUE;
}

static tv_bool
set_tuner_frequency		(tveng_device_info *	info,
				 tv_video_line *	l,
				 unsigned int		frequency)
{
	struct private_tveng25_device_info * p_info = P_INFO (info);
	struct video_input *vi = VI (l);
	unsigned long new_freq;
	int buf_type;
	tv_bool restart;
	tv_bool success;

	new_freq = (frequency << vi->step_shift) / vi->pub.u.tuner.step;

	buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	restart = (info->freq_change_restart
		   && p_info->capturing
		   && info->panel.cur_video_input == l);

	if (restart) {
		unsigned long curr_freq;

		if (g_freq (info, &curr_freq, l)) {
			if (new_freq == curr_freq) {
				store_frequency (info, vi, new_freq);
				return TRUE;
			}
		}

		if (p_info->caps.capabilities & V4L2_CAP_STREAMING) {
			if (-1 == xioctl (info, VIDIOC_STREAMOFF, &buf_type))
				return FALSE;
		} else if (p_info->reading) {
			/* Error ignored. */
			xioctl_may_fail (info, VIDIOC_STREAMOFF, &buf_type);

			p_info->reading = FALSE;
		}

		p_info->capturing = FALSE;
	}

	success = s_freq (info, l, new_freq);
	if (success) {
		/* Error ignored. */
		g_freq (info, &new_freq, l);

		store_frequency (info, vi, new_freq);
	}

	if (restart && (p_info->caps.capabilities & V4L2_CAP_STREAMING)) {
		unsigned int i;

		/* Some drivers do not completely fill buffers when
		   tuning to an empty channel. */
		for (i = 0; i < info->capture.n_buffers; ++i)
			p_info->buffers[i].clear = TRUE;

		if (!queue_xbuffers (info)) {
			/* XXX what now? */
			return FALSE;
		}

		if (0 == xioctl (info, VIDIOC_STREAMON, &buf_type)) {
			p_info->capturing = TRUE;
		} else {
			/* XXX what now? */
			success = FALSE;
		}
	}

	return success;
}


static tv_bool
get_signal_strength		(tveng_device_info *	info,
				 int *			strength,
				 int *			afc)
{
  struct v4l2_tuner tuner;

  CLEAR (tuner);
  tuner.index = VI(info->panel.cur_video_input)->tuner;

  /* XXX bttv 0.9.14 returns invalid .name? */
  if (-1 == xioctl (info, VIDIOC_G_TUNER, &tuner))
      return FALSE;

  if (strength)
    {
      /*
	Properly we should only return the signal field, but it doesn't
	always work :-/
	This has the advantage that it will find most stations (with a
	good reception) and the disadvantage that it will find too
	many stations... but better too many than too few :-)
      */
#if 0
      /* update: bttv2 does use the signal field, so lets use it
	 instead */
      if (tuner.signal)
	*strength = tuner.signal;
      else if (tuner.afc == 0)
	*strength = 65535;
      else
	*strength = 0;
#else
      /* This is the correct method, but it doesn't always work */
      *strength = tuner.signal;
#endif
    }

  if (afc)
    *afc = tuner.afc;

  return TRUE;
}

static tv_bool
get_video_input			(tveng_device_info *	info)
{
	tv_video_line *l;

	if (info->panel.video_inputs) {
		int index;

		/* sn9c102 bug: */
		index = 0;

		if (TVENG25_BAYER_TEST)
			return FALSE;

		if (-1 == xioctl (info, VIDIOC_G_INPUT, &index))
			return FALSE;

		if (info->panel.cur_video_input)
			if (VI (info->panel.cur_video_input)->index == index)
				return TRUE;

		for_all (l, info->panel.video_inputs)
			if (VI (l)->index == index)
				break;
	} else {
		l = NULL;

		if (info->panel.cur_video_input == NULL)
			return TRUE;
	}

	info->panel.cur_video_input = l;
	tv_callback_notify (info, info, info->panel.video_input_callback);

	if (l) {
		/* Implies get_video_standard() and store_audio_capability(). */
		get_video_standard_list (info);
	} else {
		free_video_standards (info);
	}

	return TRUE;
}

static tv_bool
set_video_input			(tveng_device_info *	info,
				 tv_video_line *	l)
{
	int index;

	index = 0;

	if (info->panel.cur_video_input) {
		if (0 == xioctl (info, VIDIOC_G_INPUT, &index))
			if (CVI (l)->index == index)
				return TRUE;
	}

	if (P_INFO (info)->capturing)
		return FALSE;

	index = CVI (l)->index;

	if (-1 == xioctl (info, VIDIOC_S_INPUT, &index))
		return FALSE;

	store_cur_video_input (info, l);

	/* Implies get_video_standard() and store_audio_capability(). */
	/* XXX error? */
	get_video_standard_list (info);

	if (IS_TUNER_LINE (l)) {
		/* Set tuner audmode. */
		set_audio_mode (info, info->panel.audio_mode);
	}

	return TRUE;
}

static tv_bool
tuner_bounds_audio_capability	(tveng_device_info *	info,
				 struct video_input *	vi)
{
	struct v4l2_tuner tuner;

	CLEAR (tuner);
	tuner.index = vi->tuner;

	/* XXX bttv 0.9.14 returns invalid .name? */
	if (-1 == xioctl (info, VIDIOC_G_TUNER, &tuner))
		return FALSE;

	assert (tuner.rangelow <= tuner.rangehigh);

	if (tuner.capability & V4L2_TUNER_CAP_LOW) {
		/* Actually step is 62.5 Hz, but why
		   unecessarily complicate things. */
		vi->pub.u.tuner.step = 125;
		vi->step_shift = 1;

		/* A check won't do, some drivers report (0, INT_MAX). */
		tuner.rangelow = MIN (tuner.rangelow, UINT_MAX / 125);
		tuner.rangehigh = MIN (tuner.rangehigh, UINT_MAX / 125);
	} else {
		vi->pub.u.tuner.step = 62500;
		vi->step_shift = 0;

		tuner.rangelow = MIN (tuner.rangelow, UINT_MAX / 62500);
		tuner.rangehigh = MIN (tuner.rangehigh, UINT_MAX / 62500);
	}

	vi->pub.u.tuner.minimum = SCALE_FREQUENCY (vi, tuner.rangelow);
	vi->pub.u.tuner.maximum = SCALE_FREQUENCY (vi, tuner.rangehigh);

	vi->audio_capability = TV_AUDIO_CAPABILITY_MONO;

	if (tuner.capability & V4L2_TUNER_CAP_STEREO) {
		vi->audio_capability |= TV_AUDIO_CAPABILITY_STEREO;
	}

	if (tuner.capability & V4L2_TUNER_CAP_LANG2) {
		vi->audio_capability |=
			TV_AUDIO_CAPABILITY_SAP |
			TV_AUDIO_CAPABILITY_BILINGUAL;
	}

	return get_tuner_frequency (info, &vi->pub);
}

static tv_bool
get_video_input_list		(tveng_device_info *	info)
{
	unsigned int i;

	free_video_inputs (info);

	for (i = 0;; ++i) {
		struct v4l2_input input;
		char buf[sizeof (input.name) + 1];
		struct video_input *vi;
		tv_video_line_type type;

		CLEAR (input);
		input.index = i;

		if (-1 == xioctl_may_fail (info, VIDIOC_ENUMINPUT, &input)) {
			/* i > 0 because video devices
			   cannot have zero video inputs. */
			if (EINVAL == errno && i > 0)
				break; /* end of enumeration */

			ioctl_failure (info,
				       __FILE__,
				       __PRETTY_FUNCTION__,
				       __LINE__,
				       "VIDIOC_ENUMINPUT");

			free_video_line_list (&info->panel.video_inputs);

			return FALSE;
		}

		/* Sometimes NUL is missing. */
		z_strlcpy (buf, (const char *) input.name,
			   MIN (sizeof (buf), sizeof (input.name)));

		if (input.type & V4L2_INPUT_TYPE_TUNER)
			type = TV_VIDEO_LINE_TYPE_TUNER;
		else
			type = TV_VIDEO_LINE_TYPE_BASEBAND;

		if (!(vi = VI(append_video_line (&info->panel.video_inputs,
						 type, buf, buf,
						 sizeof (*vi)))))
			goto failure;

		vi->pub._parent = info;

		vi->index = input.index;
		vi->tuner = input.tuner;

		if (input.type & V4L2_INPUT_TYPE_TUNER)
			if (!tuner_bounds_audio_capability (info, vi))
				goto failure;
	}

	get_video_input (info);

	return TRUE;

 failure:
	free_video_line_list (&info->panel.video_inputs);
	return FALSE;
}

/*
 *  Audio inputs
 */

static tv_bool
get_audio_input			(tveng_device_info *	info)
{
	tv_audio_line *l;

	if (info->panel.audio_inputs) {
		struct v4l2_audio audio;

		CLEAR (audio);

		if (-1 == xioctl (info, VIDIOC_G_AUDIO, &audio))
			return FALSE;

		if (info->panel.cur_audio_input)
			if (CAI (info->panel.cur_audio_input)->index
			    == audio.index)
				return TRUE;

		for_all (l, info->panel.audio_inputs)
			if (CAI (l)->index == audio.index)
				break;
	} else {
		l = NULL;
	}

	store_cur_audio_input (info, l);

	return TRUE;
}

static tv_bool
set_audio_input			(tveng_device_info *	info,
				 tv_audio_line *	l)
{
	struct v4l2_audio audio;

	CLEAR (audio);
	audio.index = CAI (l)->index;

	if (-1 == xioctl (info, VIDIOC_S_AUDIO, &audio))
		return FALSE;

	store_cur_audio_input (info, l);

	return TRUE;
}

static tv_bool
get_audio_input_list		(tveng_device_info *	info)
{
	unsigned int i;

	free_audio_inputs (info);

	for (i = 0;; ++i) {
		struct v4l2_audio audio;
		char buf[sizeof (audio.name) + 1];
		struct audio_input *ai;

		CLEAR (audio);
		audio.index = i;

		if (-1 == xioctl_may_fail (info, VIDIOC_ENUMAUDIO, &audio)) {
			if (EINVAL == errno)
				break; /* end of enumeration */

			ioctl_failure (info,
				       __FILE__,
				       __PRETTY_FUNCTION__,
				       __LINE__,
				       "VIDIOC_ENUMAUDIO");

			free_audio_line_list (&info->panel.audio_inputs);

			return FALSE;
		}

		/* Sometimes NUL is missing. */
		z_strlcpy (buf, (const char *) audio.name,
			   MIN (sizeof (buf), sizeof (audio.name)));

		if (!(ai = AI(append_audio_line (&info->panel.audio_inputs,
						 TV_AUDIO_LINE_TYPE_NONE,
						 buf, buf,
						 /* minimum */ 0,
						 /* maximum */ 0,
						 /* step */ 0,
						 /* reset */ 0,
						 sizeof (*ai)))))
			goto failure;

		ai->pub._parent = info;

		ai->index = audio.index;
	}

	get_audio_input (info);

	return TRUE;

 failure:
	free_audio_line_list (&info->panel.audio_inputs);
	return FALSE;
}


static tv_pixfmt
pixelformat_to_pixfmt		(unsigned int		pixelformat)
{
	switch (pixelformat) {
	  /* Note defines and Spec (0.4) are wrong: r <-> b */
	case V4L2_PIX_FMT_RGB332:	return TV_PIXFMT_BGR8;
	case V4L2_PIX_FMT_RGB555:	return TV_PIXFMT_BGRA16_LE;
	case V4L2_PIX_FMT_RGB565:	return TV_PIXFMT_BGR16_LE;
	case V4L2_PIX_FMT_RGB555X:	return TV_PIXFMT_BGRA16_BE;
	case V4L2_PIX_FMT_RGB565X:	return TV_PIXFMT_BGR16_BE;

		/* Note Spec (0.4) is wrong: r <-> b. */
		/* bttv 0.9 bug: RGB32 is wrong. */
	case V4L2_PIX_FMT_BGR24:	return TV_PIXFMT_BGR24_LE;
	case V4L2_PIX_FMT_RGB24:	return TV_PIXFMT_BGR24_BE;
	case V4L2_PIX_FMT_BGR32:	return TV_PIXFMT_BGRA32_LE;
	case V4L2_PIX_FMT_RGB32:	return TV_PIXFMT_BGRA32_BE;

	case V4L2_PIX_FMT_GREY:		return TV_PIXFMT_Y8;
	case V4L2_PIX_FMT_YUYV:		return TV_PIXFMT_YUYV;
	case V4L2_PIX_FMT_UYVY:		return TV_PIXFMT_UYVY;
	case V4L2_PIX_FMT_YVU420:	return TV_PIXFMT_YVU420;
	case V4L2_PIX_FMT_YUV420:	return TV_PIXFMT_YUV420;
	case V4L2_PIX_FMT_YVU410:	return TV_PIXFMT_YVU410;
	case V4L2_PIX_FMT_YUV410:	return TV_PIXFMT_YUV410;
	case V4L2_PIX_FMT_YUV422P:	return TV_PIXFMT_YUV422;
	case V4L2_PIX_FMT_YUV411P:	return TV_PIXFMT_YUV411;

	case V4L2_PIX_FMT_SBGGR8:	return TV_PIXFMT_SBGGR;
	case V4L2_PIX_FMT_NV12:		return TV_PIXFMT_NV12;
	case FOURCC_HM12:		return TV_PIXFMT_HM12;

	default:			return TV_PIXFMT_UNKNOWN;
	}
}

static unsigned int
pixfmt_to_pixelformat		(tv_pixfmt		pixfmt)
{
	switch (pixfmt) {
	case TV_PIXFMT_BGR8:		return V4L2_PIX_FMT_RGB332;	
	case TV_PIXFMT_BGRA16_LE:	return V4L2_PIX_FMT_RGB555;	
	case TV_PIXFMT_BGR16_LE:	return V4L2_PIX_FMT_RGB565;	
	case TV_PIXFMT_BGRA16_BE:	return V4L2_PIX_FMT_RGB555X;	
	case TV_PIXFMT_BGR16_BE:	return V4L2_PIX_FMT_RGB565X;	

	case TV_PIXFMT_BGR24_LE:	return V4L2_PIX_FMT_BGR24;	
	case TV_PIXFMT_BGR24_BE:	return V4L2_PIX_FMT_RGB24;	
	case TV_PIXFMT_BGRA32_LE:	return V4L2_PIX_FMT_BGR32;	
	case TV_PIXFMT_BGRA32_BE:	return V4L2_PIX_FMT_RGB32;	

	case TV_PIXFMT_Y8:		return V4L2_PIX_FMT_GREY;
	case TV_PIXFMT_YUYV:		return V4L2_PIX_FMT_YUYV;
	case TV_PIXFMT_UYVY:		return V4L2_PIX_FMT_UYVY;
	case TV_PIXFMT_YVU420:		return V4L2_PIX_FMT_YVU420;	
	case TV_PIXFMT_YUV420:		return V4L2_PIX_FMT_YUV420;	
	case TV_PIXFMT_YVU410:		return V4L2_PIX_FMT_YVU410;	
	case TV_PIXFMT_YUV410:		return V4L2_PIX_FMT_YUV410;	
	case TV_PIXFMT_YUV422:		return V4L2_PIX_FMT_YUV422P;	
	case TV_PIXFMT_YUV411:		return V4L2_PIX_FMT_YUV411P;	

	case TV_PIXFMT_SBGGR:		return V4L2_PIX_FMT_SBGGR8;
	case TV_PIXFMT_NV12:		return V4L2_PIX_FMT_NV12;
	case TV_PIXFMT_HM12:		return FOURCC_HM12;

	default:			return 0;
	}
}

/*
	Overlay
*/

static unsigned int
tv_to_v4l2_chromakey		(unsigned int		chromakey)
{
	switch (Z_BYTE_ORDER) {
	case Z_LITTLE_ENDIAN:
		/* XXX correct? 0xAARRGGBB */
		return chromakey & 0xFFFFFF;

	case Z_BIG_ENDIAN:
		/* XXX correct? 0xBBGGRRAA */
		return (((chromakey & 0xFF) << 24) |
			((chromakey & 0xFF00) << 8) |
			((chromakey & 0xFF0000) >> 8));

	default:
		assert (0);
	}

	return 0;
}

static unsigned int
v4l2_to_tv_chromakey		(unsigned int		chromakey)
{
	switch (Z_BYTE_ORDER) {
	case Z_LITTLE_ENDIAN:
		/* XXX correct? 0xAARRGGBB */
		return chromakey & 0xFFFFFF;

	case Z_BIG_ENDIAN:
		/* XXX correct? 0xBBGGRRAA */
		return (((chromakey & 0xFF00) << 8) |
			((chromakey & 0xFF0000) >> 8) |
			((chromakey & 0xFF000000) >> 24));

	default:
		assert (0);
	}

	return 0;
}

static tv_bool
get_overlay_buffer		(tveng_device_info *	info)
{
	struct v4l2_framebuffer fb;

	CLEAR (info->overlay.buffer);

	CLEAR (fb);

	if (-1 == xioctl (info, VIDIOC_G_FBUF, &fb))
		return FALSE;

	info->caps.flags &= ~(TVENG_CAPS_CHROMAKEY |
			      TVENG_CAPS_CLIPPING |
			      TVENG_CAPS_FRAMERAM);

	if (fb.capability & V4L2_FBUF_CAP_CHROMAKEY)
		info->caps.flags |= TVENG_CAPS_CHROMAKEY;

	/* XXX */
	if (fb.capability & (V4L2_FBUF_CAP_LIST_CLIPPING
			     | V4L2_FBUF_CAP_BITMAP_CLIPPING))
		info->caps.flags |= TVENG_CAPS_CLIPPING;

	/* XXX */
	if (!(fb.capability & V4L2_FBUF_CAP_EXTERNOVERLAY))
		info->caps.flags |= TVENG_CAPS_FRAMERAM;

	/* XXX get elsewhere
	   if ((fb.flags & V4L2_FBUF_CAP_SCALEUP) ||
	   (fb.flags & V4L2_FBUF_CAP_SCALEDOWN))
	   info->caps.flags |= TVENG_CAPS_SCALES; */

	info->overlay.buffer.base = (unsigned long) fb.base;

	if (!tv_image_format_init (&info->overlay.buffer.format,
				   fb.fmt.width,
				   fb.fmt.height,
				   fb.fmt.bytesperline,
				   pixelformat_to_pixfmt (fb.fmt.pixelformat),
				   TV_COLSPC_UNKNOWN)) {
		tv_error_msg (info, _("Driver %s returned an unknown or "
				      "invalid frame buffer format."),
			      info->node.label);

		info->tveng_errno = EINVAL;
	}

	return TRUE;
}

/* XXX we should support set_overlay_buffer with an EPERM check in tveng.c,
   so we don't have to run zapping_setup_fb when the user is privileged. */

static tv_bool
get_overlay_window		(tveng_device_info *	info)
{
	struct private_tveng25_device_info * p_info = P_INFO (info);
	struct v4l2_format format;

	CLEAR (format);
	format.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;

	if (p_info->buffers)
		unmap_xbuffers (info, /* ignore_errors */ TRUE);

	if (-1 == g_fmt (info, &format))
		return FALSE;

	info->overlay.window.x		= format.fmt.win.w.left;
	info->overlay.window.y		= format.fmt.win.w.top;
	info->overlay.window.width	= format.fmt.win.w.width;
	info->overlay.window.height	= format.fmt.win.w.height;

	/* Clips cannot be read back, we assume no change.
	   tveng.c takes care of info->overlay.clip_vector. */

	info->overlay.chromakey =
		v4l2_to_tv_chromakey (format.fmt.win.chromakey);

	return TRUE;
}

static tv_bool
set_overlay_window		(tveng_device_info *	info,
				 const tv_window *	w,
				 const tv_clip_vector *	v,
				 unsigned int		chromakey)
{
	struct private_tveng25_device_info * p_info = P_INFO (info);
	struct v4l2_format format;
	struct v4l2_clip *clips;

	if (v->size > 0) {
		struct v4l2_clip *vc;
		const tv_clip *tc;
		unsigned int i;

		clips = malloc (v->size * sizeof (*clips));
		if (NULL == clips) {
			info->tveng_errno = ENOMEM;
			t_error("malloc", info);
			return FALSE;
		}

		vc = clips;
		tc = v->vector;

		for (i = 0; i < v->size; ++i) {
			vc->next	= vc + 1; /* just in case */
			vc->c.left	= tc->x1;
			vc->c.top	= tc->y1;
			vc->c.width	= tc->x2 - tc->x1;
			vc->c.height	= tc->y2 - tc->y1;
			++vc;
			++tc;
		}

		vc[-1].next = NULL;
	} else {
		clips = NULL;
	}

	CLEAR (format);

	format.type			= V4L2_BUF_TYPE_VIDEO_OVERLAY;

	format.fmt.win.w.left		= w->x - info->overlay.buffer.x;
	format.fmt.win.w.top		= w->y - info->overlay.buffer.y;

	format.fmt.win.w.width		= w->width;
	format.fmt.win.w.height		= w->height;

	format.fmt.win.clips		= clips;
	format.fmt.win.clipcount	= v->size;

	format.fmt.win.chromakey	= tv_to_v4l2_chromakey (chromakey);

	if (p_info->buffers)
		unmap_xbuffers (info, /* ignore_errors */ TRUE);

	if (-1 == s_fmt (info, &format, /* may_fail */ FALSE)) {
		free (clips);
		return FALSE;
	}

	free (clips);

	/* Actual window size. */

	info->overlay.window.x		= format.fmt.win.w.left;
	info->overlay.window.y		= format.fmt.win.w.top;
	info->overlay.window.width	= format.fmt.win.w.width;
	info->overlay.window.height	= format.fmt.win.w.height;

	info->overlay.chromakey =
		v4l2_to_tv_chromakey (format.fmt.win.chromakey);

	return TRUE;
}

#if defined (HAVE_XV_EXTENSION) && TVENG25_XV_TEST

static tv_bool
set_overlay_xwindow		(tveng_device_info *	info,
				 Window			window,
				 GC			gc,
				 unsigned int		chromakey)
{
	struct private_tveng25_device_info * p_info = P_INFO (info);

	/* The XVideo V4L wrapper supports only clip-list overlay. */
	/* XXX tell the caller. */
	chromakey = chromakey;

	if (!p_info->grabbed_xv_port) {
		/* XXX shouldn't attach_device report this? */
		info->tveng_errno = EBUSY;
		t_error("XvGrabPort", info);
		return FALSE;
	}

	p_info->xwindow = window;
	p_info->xgc = gc;

	return TRUE;
}

#endif

static tv_bool
enable_overlay			(tveng_device_info *	info,
				 tv_bool		on)
{
	struct private_tveng25_device_info * p_info = P_INFO (info);
	int value = on;

	if (on && p_info->buffers)
		unmap_xbuffers (info, /* ignore_errors */ TRUE);

#if defined (HAVE_XV_EXTENSION) && TVENG25_XV_TEST
	if (p_info->grabbed_xv_port
	    && 0 != p_info->xwindow
	    && 0 != p_info->xgc) {
		if (on) {
			const tv_video_standard *s;
			unsigned int src_width, src_height;

			src_width = 640;
			src_height = 480;
			s = p_info->info.panel.cur_video_standard;
			if (NULL != s) {
				src_width = s->frame_width;
				src_height = s->frame_height;
			}

			return _tv_xv_put_video (info,
						 p_info->xwindow,
						 p_info->xgc,
						 /* src_x, y */ 0, 0,
						 src_width, src_height);
		} else {
			return _tv_xv_stop_video (info, p_info->xwindow);
		}
	} else
#endif
	{
		if (0 == xioctl (info, VIDIOC_OVERLAY, &value)) {
			/* Caller shall use a timer instead. */
			/* usleep (50000); */
			return TRUE;
		} else {
			return FALSE;
		}
	}
}

/*
	Capture
*/

static tv_bool
image_format_from_format	(tveng_device_info *	info _unused_,
				 tv_image_format *	f,
				 const struct v4l2_format *vfmt)
{
	struct private_tveng25_device_info * p_info = P_INFO (info);
	unsigned long bytes_per_line;
	tv_pixfmt pixfmt;

	CLEAR (*f);

	pixfmt = pixelformat_to_pixfmt (vfmt->fmt.pix.pixelformat);

	if (TV_PIXFMT_UNKNOWN == pixfmt)
		return FALSE;

	bytes_per_line = vfmt->fmt.pix.bytesperline;

	/* bttv 0.9.12 bug:
	   returns bpl = width * bpp, w/ bpp > 1 if planar YUV. */
	if (p_info->bttv_driver > 0) {
		bytes_per_line = vfmt->fmt.pix.width
			* tv_pixfmt_bytes_per_pixel (pixfmt);
	}

	/* FIXME check if width, height and bpl were rounded up. */
	tv_image_format_init (f,
			      vfmt->fmt.pix.width,
			      vfmt->fmt.pix.height,
			      bytes_per_line,
			      pixfmt,
			      TV_COLSPC_UNKNOWN);

	/* bttv 0.9.5 bug:
	   assert (f->fmt.pix.sizeimage >= info->capture.format.size); */

	if (vfmt->fmt.pix.sizeimage > f->size)
		f->size = vfmt->fmt.pix.sizeimage;

	return TRUE;
}

static void
ivtv_v4l2_format_fix		(tveng_device_info *	info,
				 struct v4l2_format *	format)
{
	format->fmt.pix.pixelformat = FOURCC_HM12;
	format->fmt.pix.bytesperline = format->fmt.pix.width;
	format->fmt.pix.sizeimage =
		format->fmt.pix.width
		* format->fmt.pix.height * 3 / 2;

	/* XXX not verified, may be _BOTTOM or _INTERLACED. */
	if (info->panel.cur_video_standard
	    && (format->fmt.pix.height
		<= info->panel.cur_video_standard->frame_height / 2)) {
		format->fmt.pix.field = V4L2_FIELD_TOP;
	}
}

static tv_bool
get_capture_format		(tveng_device_info *	info)
{
	struct v4l2_format format;

	CLEAR (format);

	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == g_fmt (info, &format))
		return FALSE;

	if (P_INFO (info)->ivtv_driver > 0) {
		/* Did we open the YUV capture device? */
		assert (V4L2_PIX_FMT_MPEG != format.fmt.pix.pixelformat);

		/* ivtv 0.2.0-rc1a bug: returns pixelformat = 0,
		   bytes_per_line = 0, sizeimage = 720 * 720, colorspace =
		   SMPTE170M regardless of video standard, field =
		   INTERLACED regardless of image size.

		   ivtv 0.3.6o, 0.3.7, 0.6.x bug: returns pixelformat =
		   V4L2_PIX_FMT_UYVY, bytes_per_line = 0, sizeimage =
		   width * height * 1.5, colorspace and field as above. */
		ivtv_v4l2_format_fix (info, &format);
	}

	return image_format_from_format (info, &info->capture.format, &format);
}

static tv_bool
set_capture_format		(tveng_device_info *	info,
				 const tv_image_format *fmt)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);
	struct v4l2_format format;
	tv_pixfmt req_pixfmt;
	unsigned int pixelformat;
	tv_bool success;

	if (p_info->capturing)
		return FALSE;

	success = FALSE;

	CLEAR (format);

	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	/* bttv 0.9.14 bug: with YUV 4:2:0 activation of VBI
	   can cause a SCERR & driver reset, DQBUF -> EIO. */
	req_pixfmt = fmt->pixel_format->pixfmt;
	pixelformat = pixfmt_to_pixelformat (req_pixfmt);

	if (0 == pixelformat) {
		info->tveng_errno = -1; /* unknown */
		tv_error_msg (info, "Bad pixfmt %u %s",
			     fmt->pixel_format->pixfmt,
			     fmt->pixel_format->name);
		return FALSE;
	}

	if (p_info->ivtv_driver > 0) {
		format.fmt.pix.pixelformat = pixelformat;

		/* For tests make sure we always get the same size. */
		if (info->panel.cur_video_standard) {
			format.fmt.pix.width	= 720;
			format.fmt.pix.height	=
				info->panel.cur_video_standard->frame_height;
		} else {
			format.fmt.pix.width	= 720;
			format.fmt.pix.height	= 480;
		}
	} else {
		format.fmt.pix.pixelformat = pixelformat;

		format.fmt.pix.width	= fmt->width;
		format.fmt.pix.height	= fmt->height;
	}

	format.fmt.pix.bytesperline	= 0; /* minimum please */
	format.fmt.pix.sizeimage	= 0; /* ditto */

	format.fmt.pix.field = V4L2_FIELD_INTERLACED;
	if (info->panel.cur_video_standard
	    && (format.fmt.pix.height
		<= info->panel.cur_video_standard->frame_height / 2)) {
		format.fmt.pix.field = V4L2_FIELD_BOTTOM;
	}

	if (p_info->buffers)
		unmap_xbuffers (info, /* ignore_errors */ TRUE);

	if (-1 == s_fmt (info, &format, /* may_fail */ TRUE)) {
		/* ivtv feature: The image size cannot change when any
		   capturing is in progress (including VBI), but the
		   driver returns EBUSY instead of the current (closest
		   possible) size. */
		if (EBUSY == errno
		    && p_info->ivtv_driver > 0) {
			success = get_capture_format (info);
			goto finish;
		}

		ioctl_failure (info,
			       __FILE__,
			       __FUNCTION__,
			       __LINE__,
			       "VIDIOC_S_FMT");

		return FALSE;
	}

	if (p_info->ivtv_driver > 0) {
		/* ivtv 0.2.0-rc1a, 0.3.6o, 0.3.7 bug: Ignores (does
		   not read or write) pixelformat, bytes_per_line, sizeimage,
		   field. */

		ivtv_v4l2_format_fix (info, &format);
	}

	/* Actual image size granted. */
	success = image_format_from_format
		(info, &info->capture.format, &format);

 finish:
	/* Drivers may return a different pixel format if that's
	   the only supported one. */
	success &= (req_pixfmt == info->capture.format.pixel_format->pixfmt);

	return success;
}

static void
init_format_generic		(struct v4l2_format *	format,
				 unsigned int		pixelformat)
{
	CLEAR (*format);

	format->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	format->fmt.pix.width		= 352;
	format->fmt.pix.height		= 288;
	format->fmt.pix.field		= V4L2_FIELD_BOTTOM;
	format->fmt.pix.bytesperline	= 0; /* minimum please */
	format->fmt.pix.sizeimage	= 0; /* ditto */
	format->fmt.pix.colorspace	= 0;
	format->fmt.pix.pixelformat	= pixelformat;
}

static tv_pixfmt_set
get_supported_pixfmt_set	(tveng_device_info *	info)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);

	tv_pixfmt_set pixfmt_set;
	tv_pixfmt pixfmt;
	unsigned int i;

	if (TVENG25_BAYER_TEST) {
		return TV_PIXFMT_SET (TV_PIXFMT_SBGGR);
	}

	/* ivtv 0.2.0-rc1a, 0.3.6o, 0.3.7 bug: VIDIOC_ENUMFMT is not
	   supported and VIDIOC_TRY_FMT returns pixelformat = UYVY. */
	if (p_info->ivtv_driver > 0
	    || TVENG25_HM12_TEST) {
		return TV_PIXFMT_SET (TV_PIXFMT_HM12);
	}

	pixfmt_set = TV_PIXFMT_SET_EMPTY;

	for (i = 0;; ++i) {
		struct v4l2_fmtdesc fmtdesc;

		CLEAR (fmtdesc);
		fmtdesc.index = i;

		if (-1 == xioctl_may_fail (info, VIDIOC_ENUM_FMT, &fmtdesc))
			break;

		pixfmt = pixelformat_to_pixfmt (fmtdesc.pixelformat);

		if (TV_PIXFMT_UNKNOWN != pixfmt)
			pixfmt_set |= TV_PIXFMT_SET (pixfmt);
	}

	if (0 != pixfmt_set)
		return pixfmt_set;

	/* Gross! Let's see if TRY_FMT gives more information. */

	/* sn9c102 1.0.8 bug: TRY_FMT doesn't copy format back to user. */
	if (0 == XSTRACMP (p_info->caps.driver, "sn9c102")) {
		return TV_PIXFMT_SET (TV_PIXFMT_SBGGR); /* is an USB camera */
	}

	for (pixfmt = 0; pixfmt < TV_MAX_PIXFMTS; ++pixfmt) {
		struct v4l2_format format;
		unsigned int pixelformat;

 		/* bttv 0.9.14 bug: with YUV 4:2:0 activation of VBI
		   can cause a SCERR & driver reset, DQBUF -> EIO. */
		pixelformat = pixfmt_to_pixelformat (pixfmt);
		if (0 == pixelformat)
			continue;

		init_format_generic (&format, pixelformat);

		if (-1 == xioctl_may_fail (info, VIDIOC_TRY_FMT, &format))
			continue;

		/* sn9c102 1.0.1 feature: changes pixelformat to SBGGR.
		   Though unexpected this is not prohibited by the spec. */
		if (format.fmt.pix.pixelformat == pixelformat)
			pixfmt_set |= TV_PIXFMT_SET (pixfmt);
	}

	if (0 != pixfmt_set)
		return pixfmt_set;

	/* TRY_FMT is optional, let's see if S_FMT works. */

	for (pixfmt = 0; pixfmt < TV_MAX_PIXFMTS; ++pixfmt) {
		struct v4l2_format format;
		unsigned int pixelformat;

 		/* bttv 0.9.14 bug: with YUV 4:2:0 activation of VBI
		   can cause a SCERR & driver reset, DQBUF -> EIO. */
		pixelformat = pixfmt_to_pixelformat (pixfmt);
		if (0 == pixelformat)
			continue;

		init_format_generic (&format, pixelformat);

		if (-1 == s_fmt (info, &format, /* may fail */ TRUE))
			continue;

		/* Error ignored. */
		image_format_from_format (info, &info->capture.format,
					  &format);

		if (format.fmt.pix.pixelformat == pixelformat)
			pixfmt_set |= TV_PIXFMT_SET (pixfmt);
	}

	return pixfmt_set;
}

static tv_bool
queue_xbuffer			(tveng_device_info *	info,
				 struct xbuffer *	b)
{
	struct v4l2_buffer buffer;

	if (b->clear) {
		if (!tv_clear_image (b->cb.data, &info->capture.format))
			return FALSE;

		b->clear = FALSE;
	}

	CLEAR (buffer);
	buffer.type = b->vb.type;
	buffer.memory = V4L2_MEMORY_MMAP;
	buffer.index = b->vb.index;

	if (-1 == xioctl (info, VIDIOC_QBUF, &buffer)) {
		return FALSE;
	}

	b->vb.flags = buffer.flags;

	return TRUE;
}

static tv_bool
queue_xbuffers			(tveng_device_info *	info)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);
	unsigned int i;

	for (i = 0; i < info->capture.n_buffers; ++i) {
		if (p_info->buffers[i].dequeued)
			continue;

		if (!queue_xbuffer (info, &p_info->buffers[i]))
			return FALSE;
	}

	return TRUE;
}

static tv_bool
queue_buffer			(tveng_device_info *	info,
				 tv_capture_buffer *	buffer)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);
	struct xbuffer *b;
	int index;

	b = PARENT (buffer, struct xbuffer, cb);

	index = b - p_info->buffers;
	assert ((unsigned int) index < info->capture.n_buffers);
	assert (b == &p_info->buffers[index]);

	assert (b->dequeued);

	if (!queue_xbuffer (info, b))
		return FALSE;

	b->dequeued = FALSE;

	return TRUE;
}

/* bttv returns EIO on internal timeout and dequeues
   the buffer it was about to write, does not
   dequeue on subsequent timeouts.  On bt8x8 SCERR it
   also resets.  To be safe we restart now, although
   really the caller should do that. */
static tv_bool
restart				(tveng_device_info *	info)
{
	struct private_tveng25_device_info * p_info = P_INFO (info);
	int saved_errno;
	int buf_type;

	saved_errno = errno;

	if (!streamoff_or_reopen (info))
		return FALSE;

	if (!queue_xbuffers (info))
		return FALSE;

	buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == xioctl (info, VIDIOC_STREAMON, &buf_type))
		return FALSE;

	p_info->capturing = TRUE;

	errno = saved_errno;

	return TRUE;
}

static int
dequeue_buffer			(tveng_device_info *	info,
				 tv_capture_buffer **	buffer,
				 const struct timeval *	timeout)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);
	struct v4l2_buffer vbuf;
	struct xbuffer *b;
	struct timeval start;
	struct timeval now;
	struct timeval tv;

	*buffer = NULL;

	if (CAPTURE_MODE_READ != info->capture_mode) {
		info->tveng_errno = -1;
		tv_error_msg(info, "Current capture mode is not READ (%d)",
			     info->capture_mode);
		return -1;
	}

	start.tv_sec = 0;
	start.tv_usec = 0;

	/* When timeout is zero elapsed time doesn't matter. */
	if (timeout && (timeout->tv_sec | timeout->tv_usec) > 0)
		gettimeofday (&start, /* timezone */ NULL);

	now = start;

 select_again:
	timeout_subtract_elapsed (&tv, timeout, &now, &start);

	if ((tv.tv_sec | tv.tv_usec) > 0) {
		fd_set set;
		int r;

 select_again_with_timeout:
		FD_ZERO (&set);
		FD_SET (info->fd, &set);

		r = device_select (info->log_fp,
				   info->fd + 1,
				   /* readable */ &set,
				   /* writeable */ NULL,
				   /* in exception */ NULL,
				   timeout ? &tv : NULL);

		switch (r) {
		case -1: /* error */
			switch (errno) {
			case EINTR:
				if (!timeout) /* infinite */
					goto select_again_with_timeout;

				gettimeofday (&now, /* timezone */ NULL);

				goto select_again;

			default:
				restart (info);

				info->tveng_errno = errno;
				t_error("select()", info);

				return -1;
			}

		case 0: /* timeout */
			return 0;

		default:
			break;
		}
	}

/* read_again: */
	CLEAR (vbuf);
	vbuf.type = p_info->buffers[0].vb.type;
	vbuf.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl (info, VIDIOC_DQBUF, &vbuf)) {
		switch (errno) {
		case EAGAIN:
		case EBUSY:
			if (!timeout) /* infinite */
				goto select_again_with_timeout;

			if ((timeout->tv_sec | timeout->tv_usec) <= 0)
				return 0; /* timeout */

			gettimeofday (&now, /* timezone */ NULL);
			timeout_subtract_elapsed (&tv, timeout, &now, &start);

			if ((tv.tv_sec | tv.tv_usec) <= 0)
				return 0; /* timeout */

			goto select_again_with_timeout;

		/* xioctl/device_ioctl() handles EINTR. */
		/* case EINTR:
			goto read_again; */

		default:
			restart (info);

			info->tveng_errno = errno;
			t_error("VIDIOC_DQBUF", info);

			return -1;
		}
	}

	assert (vbuf.index < info->capture.n_buffers);

	b = p_info->buffers + vbuf.index; 

	b->cb.sample_time = vbuf.timestamp;

	if (info->panel.cur_video_standard) {
		b->cb.stream_time = vbuf.sequence
			* info->panel.cur_video_standard->frame_ticks;
	} else {
		b->cb.stream_time = 0;
	}

	p_info->last_timestamp = b->cb.sample_time;

	b->vb.flags	= vbuf.flags;
	b->vb.field	= vbuf.field;
	b->vb.timestamp	= vbuf.timestamp;
	b->vb.timecode	= vbuf.timecode;
	b->vb.sequence	= vbuf.sequence;
 
	b->dequeued = TRUE;

	*buffer = &b->cb;

	return 1;
}

static tv_bool
flush_buffers			(tveng_device_info *	info)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);
	unsigned int count;

	if (!p_info->capturing) {
		return FALSE;
	}

	for (count = info->capture.n_buffers; count-- > 0;) {
		struct v4l2_buffer buffer;

		CLEAR (buffer);
		buffer.type = p_info->buffers[0].vb.type;
		buffer.memory = V4L2_MEMORY_MMAP;

		if (-1 == xioctl (info, VIDIOC_DQBUF, &buffer)) {
			switch (errno) {
			case EAGAIN:
			case EBUSY:
				return TRUE;

			default:
				restart (info);

				info->tveng_errno = errno;
				t_error ("VIDIOC_DQBUF", info);

				return FALSE;
			}
		}

		if (-1 == xioctl (&p_info->info, VIDIOC_QBUF, &buffer)) {
			/* XXX what now? */
			return FALSE;
		}
	}

	return TRUE;
}

static int
read_buffer			(tveng_device_info *	info,
				 tv_capture_buffer *	buffer,
				 const struct timeval *	timeout)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);
	struct timeval start;
	struct timeval now;
	struct timeval tv;
	ssize_t actual;

	if (CAPTURE_MODE_READ != info->capture_mode) {
		info->tveng_errno = -1;
		tv_error_msg(info, "Current capture mode is not READ (%d)",
			     info->capture_mode);
		return -1;
	}

	/* TODO buffer->size
	   assert (buffer->size >= info->capture.format.size); */

	start.tv_sec = 0;
	start.tv_usec = 0;

	/* When timeout is zero elapsed time doesn't matter. */
	if (timeout && (timeout->tv_sec | timeout->tv_usec) > 0)
		gettimeofday (&start, /* timezone */ NULL);

	now = start;

 select_again:
	timeout_subtract_elapsed (&tv, timeout, &now, &start);

	if ((tv.tv_sec | tv.tv_usec) > 0) {
		fd_set set;
		int r;

 select_again_with_timeout:
		FD_ZERO (&set);
		FD_SET (info->fd, &set);

		r = device_select (info->log_fp,
				   info->fd + 1,
				   /* readable */ &set,
				   /* writeable */ NULL,
				   /* in exception */ NULL,
				   timeout ? &tv : NULL);

		switch (r) {
		case -1: /* error */
			switch (errno) {
			case EINTR:
				if (!timeout) /* infinite */
					goto select_again_with_timeout;

				gettimeofday (&now, /* timezone */ NULL);

				goto select_again;

			default:
				restart (info);

				info->tveng_errno = errno;
				t_error("select()", info);

				return -1;
			}

		case 0: /* timeout */
			return 0;

		default:
			break;
		}
	}

 read_again:
	if (0
	    && p_info->ivtv_driver > 0
	    && 0x7e900 == info->capture.format.size) {
		/* Work-around for bug
		   http://www.poptix.net/ivtv/Nov-2004/msg00551.html
		   which has been fixed in newer versions. */
		actual = device_read (info->log_fp, info->fd,
				      buffer->data, 0x7c600);
	} else {
		actual = device_read (info->log_fp, info->fd,
				      buffer->data,
				      info->capture.format.size);
	}

	if ((ssize_t) 0 == actual) {
		/* EOF? */

		info->tveng_errno = 0;
		t_error ("read", info);

		return -1;
	}

	if ((ssize_t) -1 == actual) {
		switch (errno) {
		case EAGAIN:
		case EBUSY:
			if (!timeout) /* infinite */
				goto select_again_with_timeout;

			if ((timeout->tv_sec | timeout->tv_usec) <= 0)
				return 0; /* timeout */

			gettimeofday (&now, /* timezone */ NULL);
			timeout_subtract_elapsed (&tv, timeout, &now, &start);

			if ((tv.tv_sec | tv.tv_usec) <= 0)
				return 0; /* timeout */

			goto select_again_with_timeout;

		case EINTR:
			goto read_again;

		default:
			info->tveng_errno = errno;
			t_error("read", info);

			return -1;
		}
	}

	gettimeofday (&buffer->sample_time, /* timezone */ NULL);

	p_info->reading = TRUE;

	buffer->stream_time = 0;

	p_info->last_timestamp = buffer->sample_time;

	return 1;
}

static int
read_frame			(tveng_device_info *	info,
				 tv_capture_buffer *	buffer,
				 const struct timeval *	timeout)
{
	int r;

	if (P_INFO (info)->caps.capabilities & V4L2_CAP_STREAMING) {
		tv_capture_buffer *qbuffer;

		r = dequeue_buffer (info, &qbuffer, timeout);
		if (r <= 0) /* error or timeout */
			return r;

		assert (NULL != qbuffer);

		if (buffer) {
			const tv_image_format *dst_format;
			tv_bool success;

			dst_format = buffer->format;
			if (!dst_format)
				dst_format = &info->capture.format;

			success = tv_copy_image (buffer->data, dst_format,
						 qbuffer->data,
						 &info->capture.format);
			assert (success);

			buffer->sample_time = qbuffer->sample_time;
			buffer->stream_time = qbuffer->stream_time;
		}

		/* Queue the buffer again for processing */
		if (!queue_buffer (info, qbuffer))
			return -1; /* error */
	} else {
		if (buffer) {
			r = read_buffer (info, buffer, timeout);
			if (r <= 0) /* error or timeout */
				return r;
		} else {
			/* XXX read & discard. */
		}
	}

	return 1; /* success */
}

static tv_bool
unmap_xbuffers			(tveng_device_info *	info,
				 tv_bool		ignore_errors)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);
	unsigned int i;
	tv_bool success;

	if (NULL == p_info->buffers)
		return TRUE;

	success = TRUE;

	for (i = info->capture.n_buffers; i-- > 0;) {
		if (-1 == device_munmap (info->log_fp,
					 p_info->buffers[i].cb.data,
					 p_info->buffers[i].vb.length)) {
			if (!ignore_errors) {
				info->tveng_errno = errno;
				t_error("munmap()", info);

				ignore_errors = TRUE;
			}

			success = FALSE;
		}
	}

	memset (p_info->buffers, 0,
		info->capture.n_buffers * sizeof (p_info->buffers[0]));

	free (p_info->buffers);

	p_info->buffers = NULL;
	info->capture.n_buffers = 0;

	return success;
}

static tv_bool
map_xbuffers			(tveng_device_info *	info,
				 enum v4l2_buf_type	buf_type,
				 unsigned int		n_buffers)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);
	struct v4l2_requestbuffers rb;
	unsigned int i;

	p_info->buffers = NULL;
	info->capture.n_buffers = 0;

	CLEAR (rb);

	rb.type = buf_type;
	rb.memory = V4L2_MEMORY_MMAP;
	rb.count = n_buffers;

	if (-1 == xioctl (info, VIDIOC_REQBUFS, &rb))
		return FALSE;

	if (rb.count <= 0)
		return FALSE;

	p_info->buffers = calloc (rb.count, sizeof (* p_info->buffers));
	info->capture.n_buffers = 0;

	for (i = 0; i < rb.count; ++i) {
		struct xbuffer *b;
		void *data;

		b = p_info->buffers + i;

		b->vb.index = i;
		b->vb.type = buf_type;
		b->vb.memory = V4L2_MEMORY_MMAP;

		if (-1 == xioctl (info, VIDIOC_QUERYBUF, &b->vb))
			goto failure;

		data = device_mmap (info->log_fp,
				    /* start: any */ NULL,
				    b->vb.length,
				    PROT_READ | PROT_WRITE,
				    MAP_SHARED,
				    info->fd,
				    (off_t) b->vb.m.offset);

		if (MAP_FAILED == data)
			goto failure;

		b->cb.data = data;
		b->cb.size = b->vb.length;

		b->cb.sample_time.tv_sec = 0;
		b->cb.sample_time.tv_usec = 0;
		b->cb.stream_time = 0;

		b->cb.format = &info->capture.format;

		b->dequeued = FALSE;
		b->clear = TRUE;

		++info->capture.n_buffers;
	}

	if (queue_xbuffers (info))
		return TRUE;

 failure:
	info->tveng_errno = errno;
	t_error("mmap()", info);

	if (info->capture.n_buffers > 0) {
		/* Driver bug, out of memory? May still work. */

		if (queue_xbuffers (info))
			return TRUE;
	}

	unmap_xbuffers (info, /* ignore_errors */ TRUE);

	return FALSE;
}

static tv_bool
set_capture_buffers		(tveng_device_info *	info,
				 tv_capture_buffer *	buffers,
				 unsigned int		n_buffers)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);

	assert (NULL == buffers);

	if (p_info->capturing
	    || !(p_info->caps.capabilities & V4L2_CAP_STREAMING)
	    || CAPTURE_MODE_NONE != info->capture_mode) {
		info->tveng_errno = -1;
		tv_error_msg (info, "Internal error (%s:%d)",
			      __FUNCTION__, __LINE__);
		return FALSE;
	}

	if (p_info->buffers)
		unmap_xbuffers (info, /* ignore_errors */ TRUE);

	if (0 == n_buffers) {
		/* Mission accomplished. */
		return TRUE;
	}

	assert (0 == info->capture.n_buffers);
	assert (NULL == p_info->buffers);

	/* sn9c102 1.0.8 bug: mmap fails for > 1 buffers, apparently
	   because m.offset isn't page aligned. */
	if (0 == XSTRACMP (p_info->caps.driver, "sn9c102"))
		n_buffers = 1;

	if (!map_xbuffers (info,
			   V4L2_BUF_TYPE_VIDEO_CAPTURE,
			   n_buffers))
		return FALSE;

	if (info->capture.n_buffers < 1) {
		unmap_xbuffers (info, /* ignore_errors */ TRUE);

		info->tveng_errno = -1;
		tv_error_msg(info, "Not enough buffers");

		return FALSE;
	}

	return TRUE;
}

static tv_bool
enable_capture			(tveng_device_info *	info,
				 tv_bool		enable)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);

	if (enable == p_info->capturing)
		return TRUE;

	if (enable) {
		gboolean dummy;
		int buf_type;

		p_tveng_stop_everything(info,&dummy);

		if (p_info->caps.capabilities & V4L2_CAP_STREAMING) {
			if (!p_info->buffers) {
				if (!set_capture_buffers (info, NULL, 8))
					return FALSE;
			}

			buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

			if (-1 == xioctl (info, VIDIOC_STREAMON, &buf_type)) {
				unmap_xbuffers (info,
						/* ignore_errors */ TRUE);
				return FALSE;
			}
		}

		p_info->capturing = TRUE;
		info->capture_mode = CAPTURE_MODE_READ;

		CLEAR (p_info->last_timestamp);

		return TRUE;
	} else {
		assert (CAPTURE_MODE_READ == info->capture_mode);

		if (!streamoff_or_reopen (info))
			return FALSE;

		if (!unmap_xbuffers (info, /* ignore_errors */ FALSE)) {
			return FALSE;
		}

		info->capture_mode = CAPTURE_MODE_NONE;

		return TRUE;
	}
}

static void
grab_xv_port			(tveng_device_info *	info)
{
#if defined (HAVE_XV_EXTENSION) && TVENG25_XV_TEST
  struct private_tveng25_device_info *p_info = P_INFO (info);

  if (NO_PORT == info->overlay.xv_port_id)
    return;

  if (_tv_xv_grab_port (info))
    {
      p_info->grabbed_xv_port = TRUE;
      p_info->xwindow = 0;
      p_info->xgc = 0;
#if 0
      /* XXX XVideo-V4L does not support chroma-keying by the
	 V4L driver, but the graphics chip may have a XV_COLORKEY
	 attribute. Implications? XXX we still caps.flags the V4L
	 chroma-keying support although it has no effect here. */
      p_info->xa_colorkey = XInternAtom (p_info->info.display,
					 "XV_COLORKEY",
					 /* only_if_exists */ False);
#endif
    }
#endif
}

static tv_bool
get_capabilities		(tveng_device_info *	info)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);

	CLEAR (p_info->caps);

	if (-1 == xioctl_may_fail (info, VIDIOC_QUERYCAP, &p_info->caps))
		goto failure;

	if (TVENG25_NOVERLAY_TEST) {
		p_info->caps.capabilities &= ~V4L2_CAP_VIDEO_OVERLAY;
	}

	p_info->bttv_driver = 0;
	if (0 == XSTRACMP (p_info->caps.driver, "bttv")) {
		p_info->bttv_driver = p_info->caps.version;
	}

	p_info->ivtv_driver = 0;
	if (0 == XSTRACMP (p_info->caps.driver, "ivtv")) {
		p_info->ivtv_driver = p_info->caps.version;
	}

	if (TVENG25_HM12_TEST) {
		p_info->caps.capabilities &= ~V4L2_CAP_STREAMING;
		p_info->bttv_driver = 0;
		p_info->ivtv_driver = 1;
	}

	/* bttv 0.9.14 bug: S_TUNER ignores v4l2_tuner.audmode. */
	p_info->use_v4l_audio = p_info->bttv_driver;

	info->caps.flags = 0;

	if (!(p_info->caps.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		info->tveng_errno = -1;
		snprintf(info->error, 256, 
			 _("%s doesn't look like a valid capture device"), info
			 -> file_name);
		goto failure;
	}

	info->caps.flags |= TVENG_CAPS_CAPTURE;

	if (p_info->caps.capabilities & V4L2_CAP_STREAMING) {
		/* XXX REQBUFS must be checked too */
		info->caps.flags |= TVENG_CAPS_QUEUE;
	} else if (p_info->caps.capabilities & V4L2_CAP_READWRITE) {
		/* Nothing. */
	} else {
		info -> tveng_errno = -1;
		snprintf(info->error, 256,
			 _("Sorry, but \"%s\" cannot do streaming"),
			 info -> file_name);
		goto failure;
	}

	/* We copy in case the string array lacks a NUL. */
	/* XXX localize (encoding). */
	if (!(info->node.label = XSTRADUP (p_info->caps.card)))
		goto failure;

	if (!(info->node.bus = XSTRADUP (p_info->caps.bus_info)))
		goto failure;

	if (!(info->node.driver = XSTRADUP (p_info->caps.driver)))
		goto failure;

	if (_tv_asprintf (&info->node.version,
			  "%u.%u.%u",
			  (p_info->caps.version >> 16) & 0xFF,
			  (p_info->caps.version >> 8) & 0xFF,
			  (p_info->caps.version >> 0) & 0xFF) < 0)
		goto failure;

	_tv_strlcpy (info->caps.name,
		     (const char *) p_info->caps.card,
		     MIN (sizeof (info->caps.name),
			  sizeof (p_info->caps.card)));
	/* FIXME determine actual number of video inputs. */
	info->caps.channels = 0; 
	info->caps.audios = 0;
	/* FIXME determine actual limits.
	   Attention: may be different for capture and overlay.
	   Attention: all conversion routines cannot handle
	   arbitrary widths yet. */
	info->caps.maxwidth = 768;
	info->caps.minwidth = 16;
	info->caps.maxheight = 576;
	info->caps.minheight = 16;

	if (p_info->caps.capabilities & V4L2_CAP_TUNER)
		info->caps.flags |= TVENG_CAPS_TUNER;
	if (p_info->caps.capabilities & V4L2_CAP_VBI_CAPTURE)
		info->caps.flags |= TVENG_CAPS_TELETEXT;

	/* XXX get elsewhere */
	if (p_info->caps.capabilities & 0)
		info->caps.flags |= TVENG_CAPS_MONOCHROME;

	return TRUE;

 failure:
	free (info->node.label);
	info->node.label = NULL;

	free (info->node.bus);
	info->node.bus = NULL;

	free (info->node.driver);
	info->node.driver = NULL;

	free (info->node.version);
	info->node.version = NULL;

	return FALSE;
}

/* ivtv 0.2.0-rc1a, 0.3.6o, 0.3.7 feature: /dev/video0+N (N = 0 ... 7)
   returns MPEG data, S_FMT will not switch the device to HM12. To get
   HM12 images one has to read from /dev/video32+N instead. */
static tv_bool
reopen_ivtv_capture_device	(tveng_device_info *	info,
				 int			flags)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);
	char *name;
	unsigned int i;
	unsigned long num;
	int fd;
	struct v4l2_capability caps;

	name = NULL;
	fd = -1;

	/* XXX use fstat minor numbers instead? */ 

	for (i = strlen (info->file_name); i > 0; --i)
		if (!isdigit (info->file_name[i - 1]))
			break;

	num = strtoul (info->file_name + i, NULL, 0);
	if (num >= 0x20 && num < 0x24)
		return TRUE;

	if (num >= 8)
		num = 0;

	_tv_asprintf (&name, "%.*s%lu", i, info->file_name, num + 32);
	if (NULL == name) {
		goto failure;
	}

	/* XXX see zapping_setup_fb for a safer version. */
	fd = device_open (info->log_fp, name, flags, 0);

	if (-1 == fd) {
		info->tveng_errno = errno;
		snprintf (info->error, 256, 
			  "Cannot open ivtv capture device %s",
			  name);
		goto failure;
	}

	CLEAR (caps);

	if (-1 == device_ioctl (info->log_fp, fprint_ioctl_arg, fd,
				VIDIOC_QUERYCAP, &caps)
	    || 0 != XSTRACMP (caps.driver, "ivtv")) {
		info->tveng_errno = -1;
		snprintf (info->error, 256, "%s is no ivtv capture device",
			  name);
		goto failure;
	}

	/* Error ignored. */
	device_close (info->log_fp, info->fd);

	info->fd = fd;

	free (p_info->reopen_name);
	p_info->reopen_name = name;

	p_info->reopen_flags = flags;

	return TRUE;

 failure:
	free (name);
	name = NULL;

	if (-1 != fd) {
		/* Error ignored. */
		device_close (info->log_fp, fd);
		fd = -1;
	}

	return FALSE;
}

#if TVENG25_XV_TEST

#ifdef HAVE_XV_EXTENSION

static tv_bool
test_xv_port			(tveng_device_info *	info,
				 tv_bool *		restore_brightness)
{
	static const unsigned int magic = 0x16; /* 010110 */
	struct private_tveng25_device_info *p_info = P_INFO (info);
	unsigned int sensed;
	unsigned int i;
	int last;

	if (!_tv_xv_grab_port (info)) {
		return FALSE;
	}

	sensed = 0;
	last = 0;

	for (i = 0; i < 6; ++i) {
		unsigned int bit = magic & (1 << i);
		struct v4l2_control ctrl;

		*restore_brightness = TRUE;

		if (!_tv_xv_set_port_attribute (info,
						p_info->xa_xv_brightness,
						bit ? +1000 : -1000,
						/* sync */ TRUE)) {
			sensed = 0;
			goto failure;
		}

		CLEAR (ctrl);
		ctrl.id = V4L2_CID_BRIGHTNESS;

		if (-1 == xioctl (info, VIDIOC_G_CTRL, &ctrl)) {
			sensed = 0;
			goto failure;
		}

		sensed |= (ctrl.value >= last) << i;
		last = ctrl.value;
	}

 failure:
	/* Error ignored. */
	_tv_xv_ungrab_port (info);

	return (0 == ((sensed ^ magic) & ~1));
}

static tv_bool
xvideo_probe			(tveng_device_info *	info)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);
	XErrorHandler old_error_handler;
	unsigned int version;
	unsigned int revision;
	unsigned int major_opcode;
	unsigned int event_base;
	unsigned int error_base;
	Window root_window;
	XvAdaptorInfo *adaptors;
	unsigned int n_adaptors;
	struct v4l2_control brightness;
	tv_bool restore_brightness;
	unsigned int i;

	adaptors = NULL;

	p_info->info.overlay.xv_port_id = NO_PORT;

	old_error_handler = XSetErrorHandler (x11_error_handler);

	p_info->xa_xv_brightness = XInternAtom (p_info->info.display,
						"XV_BRIGHTNESS",
						/* only_if_exists */ False);
	if (None == p_info->xa_xv_brightness) {
		goto failure;
	}

	if (Success != XvQueryExtension (p_info->info.display,
					 &version, &revision,
					 &major_opcode,
					 &event_base, &error_base)) {
		goto failure;
	}

	if (version < 2 || (version == 2 && revision < 2)) {
		goto failure;
	}

	root_window = DefaultRootWindow (p_info->info.display);

	if (Success != XvQueryAdaptors (p_info->info.display,
					root_window,
					&n_adaptors, &adaptors)) {
		goto failure;
	}

	if (0 == n_adaptors) {
		goto failure;
	}

	CLEAR (brightness);
	brightness.id = V4L2_CID_BRIGHTNESS;

	/* XXX this is probably redundant. Can't we just take the
	   value from info->panel.controls? */
	if (-1 == xioctl (&p_info->info, VIDIOC_G_CTRL, &brightness)) {
		goto failure;
	}

	restore_brightness = FALSE;

	for (i = 0; i < n_adaptors; ++i) {
		unsigned int j;

		if (0 != strcmp (adaptors[i].name, "video4linux")) {
			continue;
		}

		for (j = 0; j < adaptors[i].num_ports; ++j) {
			p_info->info.overlay.xv_port_id =
				(XvPortID)(adaptors[i].base_id + j);

			if (test_xv_port (info, &restore_brightness)) {
				goto found;
			}
		}
	}

	p_info->info.overlay.xv_port_id = NO_PORT;

 found:
	if (restore_brightness) {
		/* Error ignored. */
		s_ctrl (info, &brightness);
	}

	if (io_debug_msg > 0) {
		fprintf (stderr, "Found Xv port %d.\n",
			 (int) p_info->info.overlay.xv_port_id);
	}

 failure:
	if (NULL != adaptors) {
		XvFreeAdaptorInfo (adaptors);

		adaptors = NULL;
		n_adaptors = 0;
	}

	XSetErrorHandler (old_error_handler);
}

#else /* !HAVE_XV_EXTENSION */

static void
xvideo_probe			(tveng_device_info *	info)
{
	/* Nothing to do. */
}

#endif /* !HAVE_XV_EXTENSION */

#endif /* 0 */

static int
do_open	(int flags, tveng_device_info * info)
{
  struct private_tveng25_device_info *p_info = P_INFO (info);

  free (info->node.device);
  info->node.device = strdup (info->file_name);
  if (NULL == info->node.device)
    return -1;

  free (p_info->reopen_name);
  p_info->reopen_name = strdup (info->file_name);
  if (NULL == p_info->reopen_name)
    {
      free (info->node.device);
      info->node.device = NULL;
      return -1;
    }

  /* sn9c102 1.0.8 bug: NONBLOCK open fails with EAGAIN if the device has
     users, and it seems the counter is never decremented when the USB device
     is disconnected at close time.
     Besides, non-blocking i/o is what we want because we use select() with
     a timeout. */
  if (0 != strcmp ((char *) p_info->caps.driver, "sn9c102"))
    flags |= O_NONBLOCK;

  p_info->reopen_flags = flags;

  /* XXX see zapping_setup_fb for a safer version. */
  info -> fd = device_open(info->log_fp, info -> file_name, flags, 0);
  if (-1 == info -> fd)
    {
      info->tveng_errno = errno;
      t_error("open()", info);
      free (p_info->reopen_name);
      p_info->reopen_name = NULL;
      free (info->node.device);
      info->node.device = NULL;
      return -1;
    }

  return 0;
}

/*
  Return fd for the device file opened. Checks if the device is a
  valid video device. -1 on error.
  Flags will be used for open()'ing the file 
*/
extern int disable_overlay;
static int p_tveng25_open_device_file(int flags, tveng_device_info * info)
{
  struct private_tveng25_device_info *p_info = P_INFO (info);
  struct v4l2_framebuffer fb;

  assert (info != NULL);
  assert (info->file_name != NULL);

  if (-1 == do_open (flags, info))
    return -1;

  if (!get_capabilities (info))
    {
      /* Error ignored. */
      device_close(info->log_fp, info->fd);
      free (p_info->reopen_name);
      p_info->reopen_name = NULL;
      free (info->node.device);
      info->node.device = NULL;
      return -1;
    }

	if (p_info->ivtv_driver > 0
	    && !TVENG25_HM12_TEST) {
		if (!reopen_ivtv_capture_device (info, flags)) {
			/* Error ignored. */
			device_close(info->log_fp, info->fd);
			free (p_info->reopen_name);
			p_info->reopen_name = NULL;
			free (info->node.device);
			info->node.device = NULL;
			return -1;
		}
	}

  if (TVENG25_BAYER_TEST || TVENG25_HM12_TEST)
	  p_info->caps.capabilities &= ~V4L2_CAP_VIDEO_OVERLAY;

  CLEAR (fb);

  if (!disable_overlay && (p_info->caps.capabilities & V4L2_CAP_VIDEO_OVERLAY))
    {
      info->caps.flags |= TVENG_CAPS_OVERLAY;

      /* More capabilities determined later
	 when we call xvideo_probe() and get_overlay_buffer(). */
    }

  info -> current_controller = TVENG_CONTROLLER_V4L2;
  
  /* Everything seems to be OK with this device */
  return (info -> fd);
}

static void
stop				(tveng_device_info *	info)
{
  struct private_tveng25_device_info *p_info = P_INFO (info);
  gboolean dummy;

  p_tveng_stop_everything(info,&dummy);

  if (p_info->buffers)
    unmap_xbuffers (info, /* ignore_errors */ TRUE);

#if defined (HAVE_XV_EXTENSION) && TVENG25_XV_TEST
	if (p_info->grabbed_xv_port) {
		if (!_tv_xv_ungrab_port (info)) {
			p_info->info.overlay.xv_port_id = NO_PORT;
			p_info->info.caps.flags &= ~TVENG_CAPS_XVIDEO;
		}

		p_info->grabbed_xv_port = FALSE;
	}
#endif

	if (-1 != info->fd) {
		/* Error ignored. */
		device_close (info->log_fp, info->fd);
		info -> fd = -1;
	}
}

static void free_all(tveng_device_info * info)
{
  struct private_tveng25_device_info *p_info = P_INFO (info);

  if (info -> file_name)
    {
      free(info -> file_name);
      info->file_name = NULL;
    }

  free_panel_controls (info);
  free_video_standards (info);
  free_video_inputs (info);
  free_audio_inputs (info);

  free (p_info->reopen_name);
  p_info->reopen_name = NULL;

  free (info->node.label);
  free (info->node.bus);
  free (info->node.driver);
  free (info->node.version);
  free (info->node.device);

  CLEAR (info->node);
}

/* Closes a device opened with tveng_init_device */
static void tveng25_close_device(tveng_device_info * info)
{
  stop (info);

  info -> current_controller = TVENG_CONTROLLER_NONE;

  free_all (info);
}

static void
reset_crop_rect			(tveng_device_info *	info)
{
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;

	CLEAR (cropcap);
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == xioctl_may_fail (info, VIDIOC_CROPCAP, &cropcap)) {
		if (EINVAL != errno) {
			/* Error ignored. */
			return;
		}

		/* Incorrectly defined as _IOR. */
		if (-1 == xioctl_may_fail (info, VIDIOC_CROPCAP_OLD,
					   &cropcap)) {
			/* Error ignored. */
			return;
		}
	}

	CLEAR (crop);
	crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	crop.c = cropcap.defrect;

	if (-1 == xioctl_may_fail (info, VIDIOC_S_CROP, &crop)) {
		switch (errno) {
		case EINVAL:
			/* Cropping not supported. */
			return;
		default:
			/* Errors ignored. */
			return;
		}
	}
}

static tv_bool
init_panel			(tveng_device_info *	info)
{
	CLEAR (info->panel);

	info->panel.set_video_input = set_video_input;
	info->panel.get_video_input = get_video_input;
	info->panel.set_tuner_frequency = set_tuner_frequency;
	info->panel.get_tuner_frequency = get_tuner_frequency;
	info->panel.get_signal_strength = get_signal_strength;
	info->panel.set_video_standard = set_video_standard;
	info->panel.get_video_standard = get_video_standard;
	info->panel.set_audio_input = set_audio_input;
	info->panel.get_audio_input = get_audio_input;
	info->panel.set_control = set_control;
	info->panel.get_control = get_control;
	info->panel.set_audio_mode = set_audio_mode;

	/* Video inputs & standards */

	info->panel.video_inputs = NULL;
	info->panel.cur_video_input = NULL;

	info->panel.video_standards = NULL;
	info->panel.cur_video_standard = NULL;

	if (!get_video_input_list (info))
		return FALSE;

	/* Audio inputs */

	info->panel.audio_inputs = NULL;
	info->panel.cur_audio_input = NULL;

	if (!get_audio_input_list (info))
		return FALSE;

	/* Controls */

	info->panel.controls = NULL;

	if (!get_control_list (info))
		return FALSE;

	return TRUE;
}

static tv_bool
init_overlay			(tveng_device_info *	info)
{
	CLEAR (info->overlay);

	if (0 == (info->caps.flags & TVENG_CAPS_OVERLAY))
		return TRUE;

	/* TODO: set_buffer. */
	info->overlay.get_buffer = get_overlay_buffer;
	info->overlay.set_window = set_overlay_window;
	info->overlay.get_window = get_overlay_window;
	info->overlay.enable = enable_overlay;

	/* Current parameters and additional capability flags. */
	if (!get_overlay_buffer (info)) {
		if (EINVAL == errno) {
			/* cx88 0.0.5 sets the overlay capability
			   flag but doesn't really support overlay. */
			CLEAR (info->overlay);

			info->caps.flags &= ~(TVENG_CAPS_OVERLAY |
					      TVENG_CAPS_XVIDEO);
			return TRUE;
		}

		return FALSE;
	}

	if (!get_overlay_window (info))
		return FALSE;

#if defined (HAVE_XV_EXTENSION) && TVENG25_XV_TEST
	/* The XVideo V4L wrapper supports only clip-list overlay. */
	if (info->caps.flags & TVENG_CAPS_CLIPPING) {
		xvideo_probe (info);

		if (NO_PORT != info->overlay.xv_port_id) {
			p_info->info.caps.flags |= TVENG_CAPS_XVIDEO;

			info->overlay.set_xwindow = set_overlay_xwindow;
		}
	}
#endif
	return TRUE;
}

static tv_bool
init_capture			(tveng_device_info *	info)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);

	CLEAR (info->capture);

	p_info->buffers = NULL;

	if (0 == (info->caps.flags & TVENG_CAPS_CAPTURE))
		return TRUE;

	info->capture.get_format = get_capture_format;
	info->capture.set_format = set_capture_format;
	info->capture.read_frame = read_frame;
	info->capture.enable = enable_capture;

	if (info->caps.flags & TVENG_CAPS_QUEUE) {
		info->capture.set_buffers = set_capture_buffers;
		info->capture.queue_buffer = queue_buffer;
		info->capture.dequeue_buffer = dequeue_buffer;
		info->capture.flush_buffers = flush_buffers;
	}

	if (!get_capture_format (info)) {
		if (EINVAL == errno) {
			CLEAR (info->capture);

			info->caps.flags &= ~(TVENG_CAPS_CAPTURE |
					      TVENG_CAPS_QUEUE);
			return TRUE;
		}

		return FALSE;
	}

	info->capture.supported_pixfmt_set =
		get_supported_pixfmt_set (info);

	return TRUE;
}

static int
tveng25_change_attach_mode	(tveng_device_info * info,
				 Window window,
				 enum tveng_attach_mode attach_mode)
{
  window = window; /* unused */

  stop (info);

  switch (attach_mode)
    {
    case TVENG_ATTACH_CONTROL:
    case TVENG_ATTACH_VBI:
      attach_mode = TVENG_ATTACH_CONTROL;
      if (-1 == (info -> fd = do_open (0, info)))
	{
	  free_all (info);
	  return -1;
	}
      break;

    case TVENG_ATTACH_XV: /* FIXME this is overlay, not read. */
    case TVENG_ATTACH_READ:
      /* NB must be RDWR since client may write mmapped buffers. */
      if (-1 == (info -> fd = do_open (O_RDWR, info)))
	{
	  free_all (info);
	  return -1;
	}
     if (TVENG_ATTACH_XV == attach_mode)
	grab_xv_port (info);
      attach_mode = TVENG_ATTACH_READ;
      break;
    default:
      tv_error_msg(info, "Unknown attach mode for the device");
      return -1;
    };

  info -> attach_mode = attach_mode;
  /* Current capture mode is no capture at all */
  info -> capture_mode = CAPTURE_MODE_NONE;

  return 0; /* ok */
}

/*
  Associates the given tveng_device_info with the given video
  device. On error it returns -1 and sets info->tveng_errno, info->error to
  the correct values.
  device_file: The file used to access the video device (usually
  /dev/video)
  attach_mode: Specifies the mode to open the device file
  depth: The color depth the capture will be in, -1 means let tveng
  decide based on the current display depth.
  info: The structure to be associated with the device
*/
static
int tveng25_attach_device(const char* device_file,
			  Window window _unused_,
			  enum tveng_attach_mode attach_mode,
			 tveng_device_info * info)
{
  struct private_tveng25_device_info * p_info =
    (struct private_tveng25_device_info*) info;

  assert (device_file != NULL);
  assert (info != NULL);

  memset ((char *) p_info + sizeof (p_info->info), 0,
	  sizeof (*p_info) - sizeof (*info));

  if (-1 != info -> fd) /* If the device is already attached, detach it */
    tveng_close_device(info);

  info->audio_mutable = 0;

  info -> file_name = strdup(device_file);
  if (!(info -> file_name))
    {
      info -> tveng_errno = errno;
      t_error("strdup()", info);
      return -1;
    }

  switch (attach_mode)
    {
    case TVENG_ATTACH_CONTROL:
    case TVENG_ATTACH_VBI:
      attach_mode = TVENG_ATTACH_CONTROL;
      info -> fd = p_tveng25_open_device_file(0, info);
      break;

    case TVENG_ATTACH_XV: /* FIXME this is overlay, not read. */
    case TVENG_ATTACH_READ:
      /* NB must be RDWR since client may write mmapped buffers.
	 It's required by some drivers and the V4L2 spec too. */
      info -> fd = p_tveng25_open_device_file(O_RDWR, info);
      if (TVENG_ATTACH_XV == attach_mode)
	grab_xv_port (info);
      attach_mode = TVENG_ATTACH_READ;
      break;
    default:
      tv_error_msg(info, "Unknown attach mode for the device");
      free(info->file_name);
      info->file_name = NULL;
      return -1;
    };

  /*
    Errors (if any) are already aknowledged when we reach this point,
    so we don't show them again
  */
  if (-1 == info -> fd)
    {
      free(info->file_name);
      info->file_name = NULL;
      return -1;
    }
  
  info -> attach_mode = attach_mode;
  /* Current capture mode is no capture at all */
  info -> capture_mode = CAPTURE_MODE_NONE;

	if (!init_panel (info))
		goto failure;

	if (info->caps.flags & (TVENG_CAPS_OVERLAY |
				TVENG_CAPS_CAPTURE)) {
		reset_crop_rect	(info);
	}

	if (!init_overlay (info))
		goto failure;

	if (!init_capture (info))
		goto failure;

  return info -> fd;

 failure:
  tveng25_close_device (info);
  return -1;
}

static void
destroy_devnode			(tv_device_node *	n,
				 tv_bool		restore)
{
	restore = restore;

	if (NULL == n)
		return;

	free (n->device);
	free (n->version);
	free (n->driver);
	free (n->bus);
	free (n->label);

	CLEAR (*n);

	free (n);
}

static tv_bool
append_devnode			(tv_device_node **	list,
				 const char *		api,
				 const __u8 *		label,
				 size_t			label_size,
				 const __u8 *		bus,
				 size_t			bus_size,
				 const __u8 *		driver,
				 size_t			driver_size,
				 unsigned int		version,
				 const char *		device)
{
	tv_device_node *n;

	n = calloc (1, sizeof (*n));
	if (NULL == n) {
		return FALSE;
	}

	n->label = xstrndup (label, label_size);
	if (NULL == n->label)
		goto failure;

	if (NULL != bus) {
		n->bus = xstrndup (bus, bus_size);
		if (NULL == n->bus)
			goto failure;
	}

	if (driver) {
		char *s;

		s = xstrndup (driver, driver_size);
		if (NULL != s) {
			_tv_asprintf (&n->driver, "%s (%s)", s, api);
			free (s);
			s = NULL;
		}
	} else {
		n->driver = strdup (api);
	}

	if (NULL == n->label)
		goto failure;

	if (0 != version) {
		_tv_asprintf (&n->version, "%u.%u.%u",
			      (version >> 16) & 0xFF,
			      (version >> 8) & 0xFF,
			      (version >> 0) & 0xFF);
		if (NULL == n->version)
			goto failure;
	}

	n->device = strdup (device);
	if (NULL == n->device)
		goto failure;

	n->destroy = destroy_devnode;

	tv_device_node_add (list, n);

	return TRUE;

 failure:
	destroy_devnode (n, /* restore */ TRUE);

	return FALSE;
}

tv_device_node *
tveng25_device_scan		(FILE *			log)
{
	static const char *video_devices [] = {
		"/dev/video",
		"/dev/video0",
		"/dev/v4l/video0",
		"/dev/v4l/video",
		"/dev/video1",
		"/dev/video2",
		"/dev/video3",
		"/dev/v4l/video1",
		"/dev/v4l/video2",
		"/dev/v4l/video3"
	};
	tv_device_node *list;
	unsigned int i;

	list = NULL;

	if (NULL != log) {
		fprintf (log, "V4L2 device scan\n");
	}

	for (i = 0; i < N_ELEMENTS (video_devices); ++i) {
		struct video_capability v4l_cap;
		struct v4l2_capability v4l2_cap;
		tv_bool success;
		int fd;

		/* XXX see zapping_setup_fb for a safer version. */
		fd = device_open (log, video_devices[i],
				  /* flags */ O_RDWR,
				  /* mode */ 0);
		if (-1 == fd) {
			continue;
		}

		success = TRUE;

		if (0 == device_ioctl (log, fprint_ioctl_arg, fd,
				       VIDIOC_QUERYCAP, &v4l2_cap)) {
			success = append_devnode (&list, "V4L2",
						  v4l2_cap.card,
						  sizeof (v4l2_cap.card),
						  v4l2_cap.bus_info,
						  sizeof (v4l2_cap.bus_info),
						  v4l2_cap.driver,
						  sizeof (v4l2_cap.driver),
						  v4l2_cap.version,
						  video_devices[i]);
		} else if (0 == device_ioctl (log, fprint_ioctl_arg, fd,
					      VIDIOCGCAP, &v4l_cap)) {
			success = append_devnode (&list, "V4L",
						  (const __u8 *) v4l_cap.name,
						  sizeof (v4l_cap.name),
						  /* bus */ NULL, 0,
						  /* driver */ NULL, 0,
						  /* version */ 0,
						  video_devices[i]);
		}

		/* Error ignored. */
		device_close (log, fd);
		fd = -1;

		if (!success)
			break;
	}

	return list;
}

static struct tveng_module_info tveng25_module_info = {
  .attach_device =		tveng25_attach_device,
  .close_device =		tveng25_close_device,
  .change_mode =		tveng25_change_attach_mode,

  .interface_label		= "Video4Linux 2",

  .private_size =		sizeof(struct private_tveng25_device_info)
};

/*
  Inits the V4L2 module, and fills in the given table.
*/
void tveng25_init_module(struct tveng_module_info *module_info)
{
  assert (module_info != NULL);

  memcpy(module_info, &tveng25_module_info,
	 sizeof(struct tveng_module_info)); 
}

#else /* !ENABLE_V4L */

#include "tveng25.h"

void tveng25_init_module(struct tveng_module_info *module_info)
{
  assert (module_info != NULL);

  CLEAR (module_info);
}

#endif /* ENABLE_V4L */

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
