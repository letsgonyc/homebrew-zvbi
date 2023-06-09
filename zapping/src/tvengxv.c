/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000-2001 I�aki Garc�a Etxebarria
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

/*
  This is the library in charge of simplyfying Video Access API (I
  don't want to use thirteen lines of code with ioctl's every time I
  want to change tuning freq).
  the name is TV Engine, since it is intended mainly for TV viewing.
  This file is separated so zapping doesn't need to know about V4L[2]
*/
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include <tveng.h>

#ifdef USE_XV
#define TVENGXV_PROTOTYPES 1
#include "tvengxv.h"

#include "globals.h" /* xv_video_port, disable_overlay */
#include "zmisc.h"

struct video_input {
	tv_video_line		pub;
	char			name[64];
	unsigned int		num;		/* random standard */
};

#define VI(l) PARENT (l, struct video_input, pub)
#define CVI(l) CONST_PARENT (l, struct video_input, pub)

struct standard {
	tv_video_standard	pub;
	char			name[64];
	unsigned int		num;
};

#define S(l) PARENT (l, struct standard, pub)
#define CS(l) CONST_PARENT (l, struct standard, pub)

struct control {
	tv_control		pub;
	Atom			atom;
};

#define C(l) PARENT (l, struct control, pub)

struct private_tvengxv_device_info
{
  tveng_device_info info; /* Info field, inherited */

	/** List of encodings, i.e. standards and inputs. */
	XvEncodingInfo *	ei;
	unsigned int		n_encodings;
	unsigned int		cur_encoding;

  /* This atoms define the controls */
	Atom			xa_encoding;
  int encoding_max, encoding_min, encoding_gettable;
  	Atom			xa_freq;
  int freq_max, freq_min;
	Atom			xa_mute;
	Atom			xa_volume;
	Atom			xa_colorkey;
  int colorkey_max, colorkey_min;
	Atom			xa_signal_strength;

	tv_bool			active;

	Window			window;
	GC			gc;

  Window last_win;
  GC last_gc;
  int last_w, last_h;
};

#define P_INFO(p) PARENT (p, struct private_tvengxv_device_info, info)

static int
find_encoding			(tveng_device_info *	info,
				 const char *		input,
				 const char *		standard)
{
	struct private_tvengxv_device_info * p_info = P_INFO (info);
	char encoding[200];
	unsigned int i;

	snprintf (encoding, 199, "%s-%s", standard, input);
	encoding[199] = 0;

	for (i = 0; i < p_info->n_encodings; ++i)
		if (0 == strcasecmp (encoding, p_info->ei[i].name))
			return i;

	return -1;
}

/* Splits "standard-input" storing "standard" in buffer d of size
   (can be 0), returning pointer to "input". Returns NULL on error. */
static const char *
split_encoding			(char *			d,
				 size_t			d_size,
				 const char *		s1)
{
	const char *s;

	/* We must parse the string backwards because some clown wrote
	   standards in the v4l Xv driver as "PAL-M", "PAL-Nc" etc not
	   realizing "-" is a separator. */

	s = s1 + strlen (s1);

	for (; s > s1 && s[-1] != '-'; --s)
		;

	if (s <= s1)
		return NULL;

	if (d_size > 0) {
		size_t std_len;
		
		std_len = s - s1;
		_tv_strlcpy (d, s1, MIN (std_len, d_size));
	}

	return s;
}

static tv_bool
xv_error			(tveng_device_info *	info,
				 const char *		function,
				 int			status)
{
	if (Success != x11_error_code) {
		/* XXX better error message */
		/* XvBadPort,
		   XvBadEncoding,
		   XvBadControl + error_base.
		   XvNumErrors = 3 */
		info->tveng_errno = 1500 + x11_error_code;
		t_error(function, info);
		return FALSE;
	}

	switch (status) {
	case Success:
		return TRUE;

	case XvBadExtension:
	case XvAlreadyGrabbed:
	case XvInvalidTime:
	case XvBadReply:
	case XvBadAlloc:
	default:
		info->tveng_errno = 160 + status;
		t_error(function, info);
		return FALSE;
	}
}

tv_bool
_tv_xv_stop_video		(tveng_device_info *	info,
				 Window			window)
{
	XErrorHandler old_error_handler;
	tv_bool success;
	int status;

	success = FALSE;

	old_error_handler = XSetErrorHandler (x11_error_handler);
	x11_error_code = Success;

	status = XvStopVideo (info->display,
			      info->overlay.xv_port_id,
			      window);

	if (io_debug_msg > 0) {
		unsigned long saved_error_code = x11_error_code;

		fprintf (stderr,
			 "%d = XvStopVideo (display=\"%s\" "
			 "port=%d window=%d)\n",
			 status, XDisplayString (info->display),
			 (int) info->overlay.xv_port_id,
			 (int) window);

		x11_error_code = saved_error_code;
	}

	if (Success == (status | x11_error_code))
		XSync (info->display, /* discard events */ False);

	success = xv_error (info, "XvStopVideo", status);

	XSetErrorHandler (old_error_handler);

	return success;
}

tv_bool
_tv_xv_put_video		(tveng_device_info *	info,
				 Window			window,
				 GC			gc,
				 int			src_x,
				 int			src_y,
				 unsigned int		src_width,
				 unsigned int		src_height)
{
	XErrorHandler old_error_handler;
	Window root_window;
	int x, y;
	unsigned int width, height;
	unsigned int border_width;
	unsigned int depth;
	tv_bool success;
	int status;

	success = FALSE;

	old_error_handler = XSetErrorHandler (x11_error_handler);
	x11_error_code = Success;

	if (!XGetGeometry (info->display,
			   window,
			   &root_window,
			   &x, &y,
			   &width, &height,
			   &border_width,
			   &depth)) {
		/* XXX better error message */
		info->tveng_errno = -1;
		t_error("XGetGeomentry", info);

		if (io_debug_msg > 0) {
			fprintf (stderr, "%s: XGetGeometry() failed\n",
				 __FUNCTION__);
		}

		goto failure;
	}

	/* 1x1 freezes the X server (Xorg 6.8.0, bttv 0.9.5) */
	if (width < 32 || height < 24) {
		/* XXX better error message */
		info->tveng_errno = -1;
		t_error("", info);

		if (io_debug_msg > 0) {
			fprintf (stderr, "%s: window size %ux%u too small\n",
				 __FUNCTION__, width, height);
		}

		goto failure;
	}

	status = XvPutVideo (info->display,
			     info->overlay.xv_port_id,
			     window, gc,
			     src_x, src_y,
			     src_width, src_height,
			     /* dst_x, y */ 0, 0,
			     /* dst */ width, height);

	if (io_debug_msg > 0) {
		unsigned long saved_error_code = x11_error_code;

		fprintf (stderr,
			 "%d = XvPutVideo (display=\"%s\" "
			 "port=%d window=%d gc=%d "
			 "src=%ux%u%+d%+d dst=%ux%u%+d%+d)\n",
			 status, XDisplayString (info->display),
			 (int) info->overlay.xv_port_id,
			 (int) window, (int) gc,
			 src_width, src_height, src_x, src_y,
			 width, height, 0, 0);

		x11_error_code = saved_error_code;
	}

	if (Success == (status | x11_error_code))
		XSync (info->display, /* discard events */ False);

	success = xv_error (info, "XvPutVideo", status);

 failure:
	XSetErrorHandler (old_error_handler);

	return success;
}

tv_bool
_tv_xv_get_port_attribute	(tveng_device_info *	info,
				 Atom			atom,
				 int *			value)		
{
	XErrorHandler old_error_handler;
	tv_bool success;
	int status;

	success = FALSE;

	old_error_handler = XSetErrorHandler (x11_error_handler);
	x11_error_code = Success;

	status = XvGetPortAttribute (info->display,
				     info->overlay.xv_port_id,
				     atom, value);

	if (io_debug_msg > 0) {
		unsigned long saved_error_code = x11_error_code;
		char *atom_name;

		atom_name = XGetAtomName (info->display, atom);

		fprintf (stderr,
			 "%d = XvGetPortAttribute (display=\"%s\" "
			 "port=%d atom=\"%s\") -> (value=%d=0x%x)\n",
			 status, XDisplayString (info->display),
			 (int) info->overlay.xv_port_id,
			 atom_name, *value, *value);

		if (NULL != atom_name)
			XFree (atom_name);

		x11_error_code = saved_error_code;
	}

	success = xv_error (info, "XvGetPortAttribute", status);

	XSetErrorHandler (old_error_handler);

	return success;
}

tv_bool
_tv_xv_set_port_attribute	(tveng_device_info *	info,
				 Atom			atom,
				 int			value,
				 tv_bool		sync)
{
	XErrorHandler old_error_handler;
	tv_bool success;
	int status;

	success = FALSE;

	old_error_handler = XSetErrorHandler (x11_error_handler);
	x11_error_code = Success;

	status = XvSetPortAttribute (info->display,
				     info->overlay.xv_port_id,
				     atom, value);

	if (io_debug_msg > 0) {
		unsigned long saved_error_code = x11_error_code;
		char *atom_name;

		atom_name = XGetAtomName (info->display, atom);

		fprintf (stderr,
			 "%d = XvSetPortAttribute (display=\"%s\" "
			 "port=%d atom=\"%s\" value=%d=0x%x)\n",
			 status, XDisplayString (info->display),
			 (int) info->overlay.xv_port_id,
			 atom_name, value, value);

		if (NULL != atom_name)
			XFree (atom_name);

		x11_error_code = saved_error_code;
	}

	if (Success == (status | x11_error_code) && sync)
		XSync (info->display, /* discard events */ False);

	success = xv_error (info, "XvSetPortAttribute", status);

	XSetErrorHandler (old_error_handler);

	return success;
}

tv_bool
_tv_xv_ungrab_port		(tveng_device_info *	info)
{
	XErrorHandler old_error_handler;
	tv_bool success;
	Time time;
	int status;

	success = FALSE;

	old_error_handler = XSetErrorHandler (x11_error_handler);
	x11_error_code = Success;

	time = CurrentTime;
	status = XvUngrabPort (info->display,
			       info->overlay.xv_port_id,
			       time);

	if (io_debug_msg > 0) {
		unsigned long saved_error_code = x11_error_code;

		fprintf (stderr,
			 "%d = XvUngrabPort (display=\"%s\" "
			 "port=%d time=%d)\n",
			 status,
			 XDisplayString (info->display),
			 (int) info->overlay.xv_port_id,
			 (int) time);

		x11_error_code = saved_error_code;
	}

	if (Success == (status | x11_error_code))
		XSync (info->display, /* discard events */ False);

	success = xv_error (info, "XvUngrabPort", status);

	XSetErrorHandler (old_error_handler);

	return success;
}

tv_bool
_tv_xv_grab_port		(tveng_device_info *	info)
{
	XErrorHandler old_error_handler;
	tv_bool success;
	Time time;
	int status;

	if (NO_PORT == info->overlay.xv_port_id) {
		return FALSE;
	}

	success = FALSE;

	old_error_handler = XSetErrorHandler (x11_error_handler);
	x11_error_code = Success;

	time = CurrentTime;
	status = XvGrabPort (info->display,
			     info->overlay.xv_port_id,
			     time);

	if (io_debug_msg > 0) {
		fprintf (stderr,
			 "%d = XvGrabPort (display=\"%s\" "
			 "port=%d time=%d)\n",
			 status, XDisplayString (info->display),
			 (int) info->overlay.xv_port_id,
			 (int) time);
	}

	success = xv_error (info, "XvGrabPort", status);

	XSetErrorHandler (old_error_handler);

	return success;
}

static unsigned int
adaptor_type			(const XvAdaptorInfo *	adaptor)
{
	unsigned int type = adaptor->type;

	if (0 == strcmp (adaptor->name, "NVIDIA Video Interface Port")
	    && type == (XvInputMask | XvVideoMask)) {
		/* Bug. This is TV out. */
		type = XvOutputMask | XvVideoMask;
	}

	return type;
}

static tv_bool
grab_port			(tveng_device_info *	info,
				 const XvAdaptorInfo *	pAdaptors,
				 int			nAdaptors,
				 XvPortID		port_id)
{
  int i;

  for (i = 0; i < nAdaptors; ++i)
    {
      const XvAdaptorInfo *pAdaptor;
      unsigned int type;

      pAdaptor = pAdaptors + i;

      type = adaptor_type (pAdaptor);

      if ((XvInputMask | XvVideoMask)
	  == (type & (XvInputMask | XvVideoMask)))
	{
	  if (ANY_PORT == port_id)
	    {
	      unsigned int i;

	      for (i = 0; i < pAdaptor->num_ports; ++i)
		{
		  info->overlay.xv_port_id =
		    (XvPortID)(pAdaptor->base_id + i);

		  if (_tv_xv_grab_port (info))
		    return TRUE;

		  info->overlay.xv_port_id = NO_PORT;
		}
	    }
	  else
	    {
	      if (port_id >= pAdaptor->base_id
		  && port_id < (pAdaptor->base_id + pAdaptor->num_ports))
		{
		  info->overlay.xv_port_id = port_id;

		  if (_tv_xv_grab_port (info))
		    return TRUE;

		  info->overlay.xv_port_id = NO_PORT;

		  return FALSE;
		}
	    }
	}
    }

  return FALSE;
}

static int
p_tvengxv_open_device(tveng_device_info *info,
		      Window window)
{
  struct private_tvengxv_device_info *p_info = P_INFO (info);
  unsigned int version;
  unsigned int revision;
  unsigned int major_opcode;
  unsigned int event_base;
  unsigned int error_base;
  XvAdaptorInfo *adaptor_info;
  unsigned int num_adaptors;
  XvAttribute *attribute;
  int num_attributes;

  printv ("xv_video_port 0x%x\n", xv_video_port);

  adaptor_info = NULL;
  num_adaptors = 0;

  attribute = NULL;
  num_attributes = 0;

  p_info->info.overlay.xv_port_id = NO_PORT;

  p_info->ei = NULL;
  p_info->n_encodings = 0;

  if (Success != XvQueryExtension (info->display,
				   &version,
				   &revision,
				   &major_opcode,
				   &event_base,
				   &error_base))
    {
      printv ("XVideo extension not available\n");
      goto failure;
    }

  printv ("XVideo opcode %d, base %d, %d, version %d.%d\n",
	  major_opcode,
	  event_base, error_base,
	  version, revision);

  if (version < 2 || (version == 2 && revision < 2))
    {
      printv ("XVideo extension not usable\n");
      goto failure;
    }

  /* We query adaptors which can render into this window. */
  if (None == window)
    window = DefaultRootWindow (info->display);

  if (Success != XvQueryAdaptors (info->display,
				  window,
				  &num_adaptors,
				  &adaptor_info))
    {
      printv ("XvQueryAdaptors failed\n");
      goto failure;
    }

  if (0 == num_adaptors)
    {
      printv ("No XVideo adaptors\n");
      goto failure;
    }

  p_info->info.overlay.xv_port_id = NO_PORT;

  if (!grab_port (info, adaptor_info, num_adaptors,
		  (XvPortID) xv_video_port)
      && ANY_PORT != xv_video_port)
    {
      printv ("XVideo video input port 0x%x not found\n",
	      (unsigned int) xv_video_port);

      grab_port (info, adaptor_info, num_adaptors, ANY_PORT);
    }

  if (NO_PORT == p_info->info.overlay.xv_port_id)
    {
      printv ("No XVideo input port found\n");
      goto failure;
    }

  printv ("Using XVideo video input port 0x%x\n",
	  (unsigned int) p_info->info.overlay.xv_port_id);

  /* Check that it supports querying controls and encodings */
  if (Success != XvQueryEncodings (info->display,
				   info->overlay.xv_port_id,
				   &p_info->n_encodings,
				   &p_info->ei))
    goto failure;

  if (0 == p_info->n_encodings)
    {
      info->tveng_errno = -1;
      tv_error_msg(info, "You have no encodings available");
      goto failure;
    }

  /* Create the atom that handles the encoding. */
  if (!(attribute = XvQueryPortAttributes (info->display,
					   info->overlay.xv_port_id,
					   &num_attributes)))
    goto failure;

  if (num_attributes <= 0)
    goto failure;

  XFree (attribute);
  attribute = NULL;

  /* XXX error ignored. */
  _tv_xv_ungrab_port (info);

  XvFreeAdaptorInfo (adaptor_info);
  adaptor_info = NULL;

  return 0xbeaf; /* the port seems to work ok, success */

 failure:
  if (attribute)
    XFree (attribute);

  if (p_info->ei)
    {
      XvFreeEncodingInfo (p_info->ei);
      p_info->ei = NULL;
      p_info->n_encodings = 0;
    }

  if (NO_PORT != p_info->info.overlay.xv_port_id)
    {
      /* Error ignored. */
      _tv_xv_ungrab_port (info);
      p_info->info.overlay.xv_port_id = NO_PORT;
    }

  if (adaptor_info)
    XvFreeAdaptorInfo (adaptor_info);

  return -1; /* failure */
}








/*
 *  Overlay
 */

static tv_bool
set_overlay_xwindow		(tveng_device_info *	info,
				 Window			window,
				 GC			gc,
				 unsigned int		chromakey)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);

	assert (!p_info->active);

	if (p_info->xa_colorkey != None) {
		if (!_tv_xv_set_port_attribute (info,
						p_info->xa_colorkey,
						(int) chromakey,
						/* sync */ TRUE)) {
			return FALSE;
		}
	}

	p_info->window = window;
	p_info->gc = gc;

	return TRUE;
}

static tv_bool
enable_overlay			(tveng_device_info *	info,
				 tv_bool		on)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);
	tv_bool success;

	success = FALSE;

  	if (p_info->window == 0 || p_info->gc == 0) {
		info->tveng_errno = -1;
		tv_error_msg(info, "The window value hasn't been set");
		return FALSE;
	}

	if (on) {
		int num;

		num = 0;

		if (p_info->xa_encoding != None
		    && p_info->encoding_gettable) {
			/* Error ignored. */
			_tv_xv_get_port_attribute (info, p_info->xa_encoding,
						   &num);
		}

		success = _tv_xv_put_video (info,
					    p_info->window,
					    p_info->gc,
					    /* src_x, y */ 0, 0,
					    /* src */ p_info->ei[num].width,
					    /* src */ p_info->ei[num].height);
	} else {
		success = _tv_xv_stop_video (info, p_info->window);
	}

	if (success)
		p_info->active = on;

	return success;
}

static tv_bool
get_overlay_window (tveng_device_info *info)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);
	int result;

	if (p_info->xa_colorkey == None)
		return FALSE;

	if (!_tv_xv_get_port_attribute (info, p_info->xa_colorkey, &result))
		return FALSE;

	info->overlay.chromakey = result;

	return TRUE;
}




/*
 *  Controls
 */

static tv_bool
do_get_control			(struct private_tvengxv_device_info *p_info,
				 struct control *	c)
{
	int value;

	/* XXX check at runtime */
	if (c->atom == p_info->xa_mute)
		return TRUE; /* no read-back (bttv bug) */

	if (!_tv_xv_get_port_attribute (&p_info->info, c->atom, &value))
		return FALSE;

	if (c->pub.value != value) {
		c->pub.value = value;
		tv_callback_notify (&p_info->info, &c->pub, c->pub._callback);
	}

	return TRUE;
}

static tv_bool
get_control			(tveng_device_info *	info,
				 tv_control *		c)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);

	if (c)
		return do_get_control (p_info, C(c));

	for_all (c, p_info->info.panel.controls)
		if (c->_parent == info)
			if (!do_get_control (p_info, C(c)))
				return FALSE;
	return TRUE;
}

static int
set_control			(tveng_device_info *	info,
				 tv_control *		c,
				 int			value)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);

	if (!_tv_xv_set_port_attribute (info, C(c)->atom,
					value, /* sync */ FALSE))
		return FALSE;

	if (C(c)->atom == p_info->xa_mute) {
		if (c->value != value) {
			c->value = value;
			tv_callback_notify (info, c, c->_callback);
		}

		return TRUE;
	}

	return do_get_control (p_info, C(c));
}

static const struct {
	const char *		atom;
	const char *		label;
	tv_control_id		id;
	tv_control_type		type;
} xv_attr_meta [] = {
	{ "XV_BRIGHTNESS", N_("Brightness"), TV_CONTROL_ID_BRIGHTNESS, TV_CONTROL_TYPE_INTEGER },
	{ "XV_CONTRAST",   N_("Contrast"),   TV_CONTROL_ID_CONTRAST,   TV_CONTROL_TYPE_INTEGER },
	{ "XV_SATURATION", N_("Saturation"), TV_CONTROL_ID_SATURATION, TV_CONTROL_TYPE_INTEGER },
	{ "XV_HUE",	   N_("Hue"),	     TV_CONTROL_ID_HUE,	       TV_CONTROL_TYPE_INTEGER },
	{ "XV_COLOR",	   N_("Color"),	     TV_CONTROL_ID_UNKNOWN,    TV_CONTROL_TYPE_INTEGER },
	{ "XV_INTERLACE",  N_("Interlace"),  TV_CONTROL_ID_UNKNOWN,    TV_CONTROL_TYPE_CHOICE },
	{ "XV_MUTE",	   N_("Mute"),	     TV_CONTROL_ID_MUTE,       TV_CONTROL_TYPE_BOOLEAN },
	{ "XV_VOLUME",	   N_("Volume"),     TV_CONTROL_ID_VOLUME,     TV_CONTROL_TYPE_INTEGER },
};

static tv_bool
add_control			(struct private_tvengxv_device_info *p_info,
				 const char *		atom,
				 const char *		label,
				 tv_control_id		id,
				 tv_control_type	type,
				 int			minimum,
				 int			maximum,
				 int			step)
{
	struct control c;
	Atom xatom;

	CLEAR (c);

	xatom = XInternAtom (p_info->info.display, atom, False);

	if (xatom == None)
		return TRUE;

	c.pub.type	= type;
	c.pub.id	= id;

	if (!(c.pub.label = strdup (_(label))))
		goto failure;

	c.pub.minimum	= minimum;
	c.pub.maximum	= maximum;
	c.pub.step	= step;

	c.atom		= xatom;

	if (0 == strcmp (atom, "XV_INTERLACE")) {
		if (!(c.pub.menu = calloc (4, sizeof (char *))))
			goto failure;

		if (!(c.pub.menu[0] = strdup (_("No")))
		    || !(c.pub.menu[1] = strdup (_("Yes")))
		    || !(c.pub.menu[2] = strdup (_("Doublescan"))))
			goto failure;
	}

	if (append_panel_control (&p_info->info, &c.pub, sizeof (c)))
		return TRUE;

 failure:
	if (c.pub.menu) {
		free (c.pub.menu[0]);
		free (c.pub.menu[1]);
		free (c.pub.menu[2]);
		free (c.pub.menu);
	}
	
	free (c.pub.label);
	
	return FALSE;
}

/*
 *  Video standards
 */

static tv_bool
set_video_standard		(tveng_device_info *	info,
				 tv_video_standard *	s)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);
	int num;

	num = CS(s)->num;

	if (!_tv_xv_set_port_attribute (info, p_info->xa_encoding,
					num, /* sync */ TRUE))
		return FALSE;

	p_info->cur_encoding = num;

	store_cur_video_standard (info, s);

	return TRUE;
}

/* Encodings we can translate to tv_video_standard_id. Other
   encodings will be flagged as custom standard. */
static const struct {
	const char *		name;
	const char *		label;
	tv_videostd_set		set;
} standards [] = {
	{ "pal",	"PAL",	   TV_VIDEOSTD_SET_PAL },
	{ "ntsc",	"NTSC",	   TV_VIDEOSTD_SET (TV_VIDEOSTD_NTSC_M) },
	{ "secam",	"SECAM",   TV_VIDEOSTD_SET_SECAM },
	{ "palnc",	"PAL-NC",  TV_VIDEOSTD_SET (TV_VIDEOSTD_PAL_NC) },
	{ "palm",	"PAL-M",   TV_VIDEOSTD_SET (TV_VIDEOSTD_PAL_M) },
	{ "paln",	"PAL-N",   TV_VIDEOSTD_SET (TV_VIDEOSTD_PAL_N) },
	{ "ntscjp",	"NTSC-JP", TV_VIDEOSTD_SET (TV_VIDEOSTD_NTSC_M_JP) },
};

static int
stdcmp				(const char *		s1,
				 const char *		s2)
{
	for (;;) {
		int c = *s1;

		if (0 == c)
			return 0 - *s2;

		if (!isalnum (c)) {
			++s1;
			continue;
		}

		if (tolower (c) != *s2)
			return -1;

		++s1;
		++s2;
	}

	return -1;
}

static tv_bool
get_video_standard_list		(tveng_device_info *	info)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);
	const XvEncodingInfo *cur_ei;
	const char *cur_input;
	unsigned int custom;
	unsigned int i;

	free_video_standards (info);

	if (p_info->xa_encoding == None)
		return TRUE;

	cur_ei = &p_info->ei[p_info->cur_encoding];
	cur_input = split_encoding (NULL, 0, cur_ei->name);
	if (!cur_input)
		return TRUE;

	custom = 32;

	for (i = 0; i < p_info->n_encodings; ++i) {
		struct standard *s;
		char buf[sizeof (s->name)];
		const char *input;
		unsigned int j;

		if (!(input = split_encoding (buf, sizeof (buf),
					      p_info->ei[i].name)))
			continue;

		if (0 != strcmp (input, cur_input))
			continue;

		if (buf[0] == 0)
			continue;

		for (j = 0; j < N_ELEMENTS (standards); ++j)
			if (0 == stdcmp (buf, standards[j].name))
				break;

		if (j < N_ELEMENTS (standards)) {
			s = S(append_video_standard (&info->panel.video_standards,
						     standards[j].set,
						     standards[j].label,
						     standards[j].name,
						     sizeof (*s)));
		} else {
			char up[sizeof (buf)];

			if (custom >= TV_MAX_VIDEOSTDS)
				continue;

			for (j = 0; buf[j]; ++j)
				up[j] = toupper (buf[j]);

			up[j] = 0;

			s = S(append_video_standard
			      (&info->panel.video_standards,
			       TV_VIDEOSTD_SET (1) << (custom++),
			       up, buf,
			       sizeof (*s)));
		}

		if (s == NULL)
			goto failure;

		z_strlcpy (s->name, buf, sizeof (s->name));

		s->num = i;
	}

	return TRUE;

 failure:
	free_video_standard_list (&info->panel.video_standards);
	return FALSE;
}

/*
 *  Video inputs
 */

static tv_bool
get_video_input			(tveng_device_info *	info);

static void
store_frequency			(tveng_device_info *	info,
				 struct video_input *	vi,
				 int			freq)
{
	unsigned int frequency = freq * 62500;

	if (vi->pub.u.tuner.frequency != frequency) {
		vi->pub.u.tuner.frequency = frequency;
		tv_callback_notify (info, &vi->pub, vi->pub._callback);
	}
}

static tv_bool
get_tuner_frequency		(tveng_device_info *	info,
				 tv_video_line *	l)
{
	struct private_tvengxv_device_info * p_info = P_INFO (info);
	int freq;

	if (None == p_info->xa_freq)
		return FALSE;

	if (!get_video_input (info))
		return FALSE;

	if (info->panel.cur_video_input == l) {
		if (!_tv_xv_get_port_attribute (info, p_info->xa_freq, &freq))
			return FALSE;

		store_frequency (info, VI (l), freq);
	}

	return TRUE;
}

static tv_bool
set_tuner_frequency		(tveng_device_info *	info,
				 tv_video_line *	l,
				 unsigned int		frequency)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);
	int freq;

	if (p_info->xa_freq == None)
		return FALSE;

	if (!get_video_input (info))
		return FALSE;

	freq = frequency / 62500;

	if (info->panel.cur_video_input != l)
		goto store;

	if (!_tv_xv_set_port_attribute (info, p_info->xa_freq,
					freq, /* sync */ TRUE))
		return FALSE;

 store:
	store_frequency (info, VI (l), freq);
	return TRUE;
}

static tv_bool
get_signal_strength		(tveng_device_info *	info,
				 int *			strength,
				 int *			afc _unused_)
{
	struct private_tvengxv_device_info * p_info = P_INFO (info);

	if (NULL == strength)
		return TRUE;

	if (None == p_info->xa_signal_strength)
		return TRUE;

	return _tv_xv_get_port_attribute (info, p_info->xa_signal_strength,
					  strength);
}



static struct video_input *
find_video_input		(tv_video_line *	list,
				 const char *		input)
{
	for_all (list, list) {
		struct video_input *vi = VI(list);

		if (0 == strcmp (vi->name, input))
			return vi;
	}

	return NULL;
}

/* Cannot use the generic helper functions, we must set video
   standard and input at the same time. */
static void
set_source			(tveng_device_info *	info,
				 tv_video_line *	input,
				 tv_video_standard *	standard)
{
	tv_video_line *old_input;
	tv_video_standard *old_standard;

	old_input = info->panel.cur_video_input;
	info->panel.cur_video_input = input;

	old_standard = info->panel.cur_video_standard;
	info->panel.cur_video_standard = standard;

	if (old_input != input)
		tv_callback_notify (info, info,
				    info->panel.video_input_callback);

	if (old_standard != standard)
		tv_callback_notify (info, info,
				    info->panel.video_standard_callback);
}

static tv_bool
get_video_input			(tveng_device_info *	info)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);
	struct video_input *vi;
	const char *input;
	tv_video_standard *ts;
	int enc;

	if (p_info->xa_encoding == None
	    || !p_info->encoding_gettable) {
		set_source (info, NULL, NULL);
		return TRUE;
	}

	if (!_tv_xv_get_port_attribute (info, p_info->xa_encoding, &enc))
		return FALSE;

	/* XXX Xv/v4l BUG? */
	if (enc < 0 || enc > 10 /*XXX*/)
		p_info->cur_encoding = 0;
	else
		p_info->cur_encoding = enc;

	input = split_encoding (NULL, 0,
				p_info->ei[p_info->cur_encoding].name);

	vi = find_video_input (info->panel.video_inputs, input);

	assert (NULL != vi);

	get_video_standard_list (info);

	for_all (ts, info->panel.video_standards)
		if (S(ts)->num == p_info->cur_encoding)
			break;

	set_source (info, &vi->pub, ts);

	return TRUE;
}

static tv_bool
set_video_input			(tveng_device_info *	info,
				 tv_video_line *	tl)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);
	struct video_input *vi;
	tv_video_standard *ts;
	int num;

	vi = VI(tl);
	num = -1;

	if (info->panel.cur_video_standard) {
		struct standard *s;

		/* Keep standard if possible. */

		s = S(info->panel.cur_video_standard);
		num = find_encoding (info, vi->name, s->name);
	}

	if (-1 == num) {
		num = vi->num; /* random standard */

		/* XXX error ignored */
		_tv_xv_set_port_attribute (info, p_info->xa_encoding,
					   num, /* sync */ TRUE);

		get_video_standard_list (info);
	} else {
		/* XXX error ignored */
		_tv_xv_set_port_attribute (info, p_info->xa_encoding,
					   num, /* sync */ TRUE);
	}

	p_info->cur_encoding = num;

	for_all (ts, info->panel.video_standards)
		if (CS(ts)->num == p_info->cur_encoding)
			break;

	set_source (info, tl, ts);

	/* Xv does not promise per-tuner frequency setting as we do.
	   XXX ignores the possibility that a third
	   party changed the frequency from the value we know. */
	if (IS_TUNER_LINE (tl))
		set_tuner_frequency (info, info->panel.cur_video_input,
				     info->panel.cur_video_input
				     ->u.tuner.frequency);

	return TRUE;
}

static tv_bool
get_video_input_list		(tveng_device_info *	info)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);
	unsigned int i;

	free_video_inputs (info);

	if (p_info->xa_encoding == None)
		return TRUE;

	for (i = 0; i < p_info->n_encodings; ++i) {
		struct video_input *vi;
		char buf[100];
		const char *input;
		tv_video_line_type type;
		int freq;

		if (info->debug_level > 0)
			fprintf (stderr, "  TVeng Xv input #%d: %s\n",
				 i, p_info->ei[i].name);

		if (!(input = split_encoding (NULL, 0,
					      p_info->ei[i].name)))
			continue;

		if (find_video_input (info->panel.video_inputs, input))
			continue;

		/* FIXME */
		if (p_info->xa_freq != None)
			type = TV_VIDEO_LINE_TYPE_TUNER;
		else
			type = TV_VIDEO_LINE_TYPE_BASEBAND;

		z_strlcpy (buf, input, sizeof (buf));
		buf[0] = toupper (buf[0]);

		if (!(vi = VI(append_video_line (&info->panel.video_inputs,
						 type, buf, input, sizeof (*vi)))))
			goto failure;

		vi->pub._parent = info;

		z_strlcpy (vi->name, input, sizeof (vi->name));

		vi->num = i;

		if (TV_VIDEO_LINE_TYPE_TUNER == vi->pub.type) {
			/* FIXME */
#if 0 /* Xv/v4l reports bogus maximum */
			vi->pub.u.tuner.minimum = p_info->freq_min * 1000;
			vi->pub.u.tuner.maximum = p_info->freq_max * 1000;
#else
			vi->pub.u.tuner.minimum = 0;
			vi->pub.u.tuner.maximum = INT_MAX - (INT_MAX % 62500);
			/* NB freq attr is int */
#endif
			vi->pub.u.tuner.step = 62500;

			/* FIXME gets frequency of current input. */
			/* XXX error ignored. */
			_tv_xv_get_port_attribute (info,
						   p_info->xa_freq,
						   &freq);

			store_frequency (info, vi, freq);
		}
	}

	if (!get_video_input (info))
		goto failure;

	return TRUE;

 failure:
	free_video_line_list (&info->panel.video_inputs);
	return FALSE;
}

#if 0
      /* The XVideo extension provides very little info about encodings,
	 we must just make something up */
      if (p_info->freq != None)
        {
	  /* this encoding may refer to a baseband input though */
          info->inputs[info->num_inputs].tuners = 1;
          info->inputs[info->num_inputs].flags |= TVENG_INPUT_TUNER;
          info->inputs[info->num_inputs].type = TVENG_INPUT_TYPE_TV;
	}
      else
        {
          info->inputs[info->num_inputs].tuners = 0;
          info->inputs[info->num_inputs].type = TVENG_INPUT_TYPE_CAMERA;
	}
      if (p_info->volume != None || p_info->mute != None)
        info->inputs[info->num_inputs].flags |= TVENG_INPUT_AUDIO;
      snprintf(info->inputs[info->num_inputs].name, 32, input);
      info->inputs[info->num_inputs].name[31] = 0;
      info->inputs[info->num_inputs].hash =
	tveng_build_hash(info->inputs[info->num_inputs].name);
#endif

















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
int tvengxv_attach_device(const char* device_file _unused_,
			  Window window,
			  enum tveng_attach_mode attach_mode,
			  tveng_device_info * info)
{
  struct private_tvengxv_device_info * p_info =
    (struct private_tvengxv_device_info *)info;
  Display *dpy;
  XvAttribute *at;
  int num_attributes;
  XvPortID port_id;
  int i;
  unsigned int j;

  assert (NULL != info);

  memset ((char *) p_info + sizeof (p_info->info), 0,
	  sizeof (*p_info) - sizeof (*info));

  if (info->disable_xv_video || disable_overlay)
    {
      info->tveng_errno = -1;
      tv_error_msg(info, "XVideo support has been disabled");
      return -1;
    }

  dpy = info->display;

  if (-1 != info -> fd) /* If the device is already attached, detach it */
    tveng_close_device(info);

  /* clear the atoms */
  p_info->xa_encoding = None;
  p_info->xa_freq = None;
  p_info->xa_mute = None;
  p_info->xa_volume = None;
  p_info->xa_colorkey = None;
  p_info->xa_signal_strength = None;
  p_info->ei = NULL;
  p_info->cur_encoding = 0;

  p_info->active = FALSE;
  p_info->window = 0;
  p_info->gc = 0;
  p_info->last_win = 0;
  p_info->last_gc = 0;
  p_info->last_w = 0;
  p_info->last_h = 0;

  /* In this module, the given device file doesn't matter */
  info -> file_name = strdup("XVideo");
  if (!(info -> file_name))
    {
      perror("strdup");
      info->tveng_errno = errno;
      snprintf(info->error, 256, "Cannot duplicate device name");
      goto error1;
    }

  switch (attach_mode)
    {
      /* In V4L there is no control-only mode */
    case TVENG_ATTACH_XV:
      info -> fd = p_tvengxv_open_device(info,window);
      break;
    default:
      tv_error_msg(info, "This module only supports TVENG_ATTACH_XV");
      goto error1;
    };

  if (-1 == info -> fd)
    goto error1;

  info -> attach_mode = attach_mode;
  /* Current capture mode is no capture at all */
  info -> capture_mode = CAPTURE_MODE_NONE;

  info->caps.flags = (TVENG_CAPS_OVERLAY |
		      TVENG_CAPS_CLIPPING |
		      TVENG_CAPS_XVIDEO);
  info->caps.audios = 0;

  /* Atoms & controls */

  info->panel.controls = NULL;

  at = XvQueryPortAttributes (dpy, p_info->info.overlay.xv_port_id, &num_attributes);

  for (i = 0; i < num_attributes; i++) {
	  if (info->debug_level > 0)
		  fprintf(stderr, "  TVeng Xv atom: %s %c/%c (%i -> %i)\n",
			  at[i].name,
			  (at[i].flags & XvGettable) ? 'r' : '-',
			  (at[i].flags & XvSettable) ? 'w' : '-',
			  at[i].min_value, at[i].max_value);

	  if (!strcmp("XV_ENCODING", at[i].name)) {
		  if (!(at[i].flags & XvSettable))
			  continue;

		  p_info->xa_encoding = XInternAtom (dpy, "XV_ENCODING", False);
		  p_info->encoding_max = at[i].max_value;
		  p_info->encoding_min = at[i].min_value;
		  p_info->encoding_gettable = at[i].flags & XvGettable;
		  continue;
	  } else if (!strcmp("XV_SIGNAL_STRENGTH", at[i].name)) {
		  if (!(at[i].flags & XvGettable))
			  continue;

		  p_info->xa_signal_strength =
		  	XInternAtom (dpy, "XV_SIGNAL_STRENGTH", False);
		  continue;
	  }

	  if ((at[i].flags & (XvGettable | XvSettable)) != (XvGettable | XvSettable))
		  continue;
		  
	  if (!strcmp("XV_FREQ", at[i].name)) {
		  info->caps.flags |= TVENG_CAPS_TUNER;

		  p_info->xa_freq = XInternAtom (dpy, "XV_FREQ", False);
		  p_info->freq_max = at[i].max_value;
		  p_info->freq_min = at[i].min_value;
		  continue;
	  } else if (!strcmp("XV_COLORKEY", at[i].name)) {
		  info->caps.flags |= TVENG_CAPS_CHROMAKEY;

		  p_info->xa_colorkey = XInternAtom (dpy, "XV_COLORKEY", False);
		  p_info->colorkey_max = at[i].max_value;
		  p_info->colorkey_min = at[i].min_value;
		  continue;
	  }

	  if (0 == strcmp ("XV_MUTE", at[i].name)) {
		  p_info->xa_mute = XInternAtom (dpy, "XV_MUTE", False);
	  } else if (0 == strcmp ("XV_VOLUME", at[i].name)) {
		  p_info->xa_volume = XInternAtom (dpy, "XV_VOLUME", False);
	  }

	  for (j = 0; j < N_ELEMENTS (xv_attr_meta); j++) {
		  if (0 == strcmp (xv_attr_meta[j].atom, at[i].name)) {
			  int step;

			  /* Not reported, let's make something up. */
			  step = (at[i].max_value - at[i].min_value) / 100;

			  /* Error ignored */
			  add_control (p_info,
				       xv_attr_meta[j].atom,
				       xv_attr_meta[j].label,
				       xv_attr_meta[j].id,
				       xv_attr_meta[j].type,
				       at[i].min_value,
				       at[i].max_value,
				       step);
		  }
	  }
  }

  /* Glint bug - XV_ENCODING not listed */
  if (p_info->n_encodings > 0
      && None == p_info->xa_encoding)
    {
      if (info->debug_level > 0)
	fprintf(stderr, "  TVeng Xv atom: XV_ENCODING (hidden) (%i -> %i)\n",
		0, p_info->n_encodings - 1);

      p_info->xa_encoding = XInternAtom (dpy, "XV_ENCODING", False);
      p_info->encoding_max = p_info->n_encodings - 1;
      p_info->encoding_min = 0;
      p_info->encoding_gettable = TRUE;
    }

  info->panel.set_video_input = set_video_input;
  info->panel.get_video_input = get_video_input;
  info->panel.get_tuner_frequency = get_tuner_frequency;
  info->panel.set_tuner_frequency = set_tuner_frequency;
  info->panel.get_signal_strength = get_signal_strength;


  /* Video input and standard combine as "encoding", get_video_input
     also determines the current standard, hence no get_video_standard. */
  info->panel.set_video_standard = set_video_standard;
  info->panel.set_control = set_control;
  info->panel.get_control = get_control;

      /* Set the mute control to OFF (workaround for BTTV bug) */
      /* tveng_set_control(&control, 0, info); */

  /* fill in with the proper values */
  get_control (info, NULL /* all */);

  /* We have a valid device, get some info about it */
  info->current_controller = TVENG_CONTROLLER_XV;

	/* Video inputs & standards */

	info->panel.video_inputs = NULL;
	info->panel.cur_video_input = NULL;

	info->panel.video_standards = NULL;
	info->panel.cur_video_standard = NULL;

	if (!get_video_input_list (info))
	  goto error1; /* XXX*/

	port_id = info->overlay.xv_port_id;
	CLEAR (info->overlay);
	info->overlay.xv_port_id = port_id;
	info->overlay.set_xwindow = set_overlay_xwindow;
	info->overlay.get_window = get_overlay_window;
	info->overlay.enable = enable_overlay;

	CLEAR (info->capture);


  /* fill in capabilities info */
	info->caps.channels = 0; /* FIXME info->num_inputs;*/
  /* Let's go creative! */
  snprintf(info->caps.name, 32, "XVideo device");
#if 0
  info->caps.minwidth = 1;
  info->caps.minheight = 1;
  info->caps.maxwidth = 32768;
  info->caps.maxheight = 32768;
#else
  /* XXX conservative limits. */
  info->caps.minwidth = 16;
  info->caps.minheight = 16;
  info->caps.maxwidth = 768;
  info->caps.maxheight = 576;
#endif

  return info->fd;

 error1:
  if (info->file_name)
    free(info->file_name);
  info->file_name = NULL;
  return -1;
}


/* Closes a device opened with tveng_init_device */
static void tvengxv_close_device(tveng_device_info * info)
{
  struct private_tvengxv_device_info *p_info=
    (struct private_tvengxv_device_info*) info;
  gboolean dummy;

  assert (NULL != info);

  p_tveng_stop_everything(info, &dummy);

  if (p_info->ei)
    XvFreeEncodingInfo(p_info->ei);
  p_info->ei = NULL;

  info -> fd = -1;
  info -> current_controller = TVENG_CONTROLLER_NONE;

  if (info -> file_name)
    free(info -> file_name);

	free_video_standards (info);
	free_video_inputs (info);
	free_panel_controls (info);

  /* clear the atoms */

  info -> file_name = NULL;

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
append_devnodes			(tv_device_node **	list,
				 const XvAdaptorInfo *	adaptor,
				 const char *		display_name,
				 const char *		version_str)
{
	unsigned int i;

	for (i = 0; i < adaptor->num_ports; ++i) {
		tv_device_node *n;

		n = calloc (1, sizeof (*n));
		if (NULL == n) {
			return FALSE;
		}

		if (1 == adaptor->num_ports) {
			n->label = strdup (adaptor->name);
		} else {
			_tv_asprintf (&n->label, "%s port %u",
				      adaptor->name, i);
		}

		n->bus = strdup (display_name);
		n->driver = strdup ("XVideo");
		n->version = strdup (version_str);

		/* XvPortID */
		_tv_asprintf (&n->device, "%lu",
			      adaptor->base_id + i);

		if (NULL == n->label
		    || NULL == n->bus
		    || NULL == n->driver
		    || NULL == n->version
		    || NULL == n->device) {
			destroy_devnode (n, /* restore */ TRUE);
			return FALSE;
		}

		n->destroy = destroy_devnode;

		tv_device_node_add (list, n);
	}

	return TRUE;
}

tv_device_node *
tvengxv_port_scan		(Display *		display,
				 FILE *			log)
{
	char version_str[32];
	XErrorHandler old_error_handler;
	tv_device_node *list;
	XvAdaptorInfo *adaptors;
	unsigned int n_adaptors;
	const char *display_name;
	unsigned int version;
	unsigned int revision;
	unsigned int major_opcode;
	unsigned int event_base;
	unsigned int error_base;
	Window root_window;
	unsigned int i;
	int status;

	assert (NULL != display);

	list = NULL;
	adaptors = NULL;

	old_error_handler = XSetErrorHandler (x11_error_handler);

	display_name = XDisplayString (display);

	if (NULL != log) {
		fprintf (log, "XVideo video port scan on display %s\n",
			 display_name);
	}

	if (Success != XvQueryExtension (display,
					 &version, &revision,
					 &major_opcode,
					 &event_base, &error_base)) {
		if (NULL != log) {
			fprintf (log, "XVideo extension not available\n");
		}

		goto done;
	}

	if (NULL != log) {
		fprintf (log, "XVideo opcode=%u event_base=%u "
			 "error_base=%u version=%u.%u\n",
			 major_opcode,
			 event_base, error_base,
			 version, revision);
	}

	if (version < 2 || (version == 2 && revision < 2)) {
		if (NULL != log) {
			fprintf (log, "XVideo extension not usable\n");
		}

		goto done;
	}

	snprintf (version_str, sizeof (version_str),
		  "%u.%u", version, revision);

	root_window = DefaultRootWindow (display);

	x11_error_code = Success;
	status = XvQueryAdaptors (display, root_window,
				  &n_adaptors, &adaptors);
	if (Success != status) {
		if (log) {
			fprintf (log, "XvQueryAdaptors failed, "
				 "status=%d error=%lu\n",
				 status, x11_error_code);
		}

		goto done;
	}

	for (i = 0; i < n_adaptors; ++i) {
		unsigned int type;

		type = adaptor_type (&adaptors[i]);

		if ((XvInputMask | XvVideoMask)
		    != (type & (XvInputMask | XvVideoMask)))
			continue;

		if (!append_devnodes (&list, &adaptors[i],
				      display_name, version_str)) {
			goto done;
		}
	}

 done:
	if (NULL != adaptors) {
		XvFreeAdaptorInfo (adaptors);

		adaptors = NULL;
		n_adaptors = 0;
	}

	XSetErrorHandler (old_error_handler);

	return list;
}

static struct tveng_module_info tvengxv_module_info = {
  .attach_device =		tvengxv_attach_device,
  .close_device =		tvengxv_close_device,

  .interface_label		= "XVideo extension",

  .private_size =		sizeof(struct private_tvengxv_device_info)
};

/*
  Inits the XV module, and fills in the given table.
*/
void tvengxv_init_module(struct tveng_module_info *module_info)
{
  assert (NULL != module_info);

  memcpy(module_info, &tvengxv_module_info,
	 sizeof(struct tveng_module_info));
}
#else /* do not use the XVideo extension */
#include "tvengxv.h"
void tvengxv_init_module(struct tveng_module_info *module_info)
{
  assert (NULL != module_info);

  CLEAR (*module_info);
}
#endif

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
