/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000 I�aki Garc�a Etxebarria
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

#include "zmisc.h"

#ifdef ENABLE_V4L
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/mman.h>
#undef WNOHANG
#undef WUNTRACED
#include <linux/kernel.h>
#include <linux/fs.h>
#include <errno.h>
#include <math.h>
#include <endian.h>

#include "../common/fifo.h" /* current_time() */

/* 
   This works around a bug bttv appears to have with the mute
   property. Comment out the line if your V4L driver isn't buggy.
*/
#define TVENG1_BTTV_MUTE_BUG_WORKAROUND 1

#define TVENG1_PROTOTYPES 1
#include "tveng1.h"

/*
 *  Kernel interface
 */
#include "../common/videodev.h"
#include "../common/fprintf_videodev.h"

#define LOG_FP 0 /* stderr */

#define v4l_ioctl(fd, cmd, arg)						\
(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),					\
 device_ioctl (LOG_FP, fprintf_ioctl_arg, fd, cmd, arg))

/*
 *  Private control IDs. In v4l the control concept doesn't exist.
 */
typedef enum {
	CONTROL_BRIGHTNESS	= (1 << 0),
	CONTROL_CONTRAST	= (1 << 1),
	CONTROL_COLOUR		= (1 << 2),
	CONTROL_HUE		= (1 << 3),
	CONTROL_MUTE		= (1 << 16),
	CONTROL_VOLUME		= (1 << 17),
	CONTROL_BASS		= (1 << 18),
	CONTROL_TREBLE		= (1 << 19),
	CONTROL_BALANCE		= (1 << 20),
	CONTROL_AUDIO_DECODING	= (1 << 21)
} control_id;

static const control_id VIDEO_CONTROLS =
	CONTROL_BRIGHTNESS |
	CONTROL_CONTRAST |
	CONTROL_COLOUR |
	CONTROL_HUE;

static const control_id AUDIO_CONTROLS =
	CONTROL_MUTE |
	CONTROL_VOLUME |
	CONTROL_BASS |
	CONTROL_TREBLE |
	CONTROL_BALANCE |
	CONTROL_AUDIO_DECODING;

#define ALL_CONTROLS (VIDEO_CONTROLS | AUDIO_CONTROLS)

struct control {
	tv_control		pub;
	control_id		id;
};

#define C(l) PARENT (l, struct control, pub)

/*
  If this is enabled, some specific features of the bttv driver are
  enabled, but they are non-standard
*/
#define TVENG1_BTTV_PRESENT 1

struct private_tveng1_device_info
{
  tveng_device_info info; /* Info field, inherited */
#ifdef TVENG1_BTTV_MUTE_BUG_WORKAROUND
//  int muted; /* 0 if the device is muted, 1 otherwise. A workaround
//		for a bttv problem. */
#endif
  int audio_mode; /* auto, mono, stereo, ... */
  char * mmaped_data; /* A pointer to the data mmap() returned */
  struct video_mbuf mmbuf; /* Info about the location of the frames */
  int queued, dequeued; /* The index of the [de]queued frames */
  double last_timestamp; /* Timestamp of the last frame captured */

  double capture_time;
  double frame_period_near;
  double frame_period_far;

  uint32_t chroma; /* Pixel value for the chromakey */
  uint32_t r, g, b; /* 0-65535 components for the chroma */

  /* OV511 camera */
  int ogb_fd;

	unsigned int		all_controls;		/* CONTROL_ set */

	tv_control *		control_mute;
	tv_control *		control_audio_dec;

	unsigned		read_back_controls	: 1;
	unsigned		mute_flag_readable	: 1;
	unsigned		audio_mode_reads_rx	: 1;
};

#define P_INFO(p) PARENT (p, struct private_tveng1_device_info, info)

/* Private, builds the controls structure */
static int
p_tveng1_build_controls(tveng_device_info * info);

/*
  Return fd for the device file opened. Checks if the device is a
  valid video device. -1 on error.
  Flags will be used for open()'ing the file 
*/
static
int p_tveng1_open_device_file(int flags, tveng_device_info * info)
{
  struct private_tveng1_device_info *p_info = P_INFO (info);
  struct video_capability caps;
  
  t_assert(info != NULL);
  t_assert(info -> file_name != NULL);

  info -> fd = device_open (LOG_FP, info -> file_name, flags, 0);
  if (info -> fd < 0)
    {
      info->tveng_errno = errno; /* Just to put something other than 0 */
      t_error("open()", info);
      return -1;
    }

  /* We check the capabilities of this video device */
  if (v4l_ioctl(info -> fd, VIDIOCGCAP, &caps))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGCAP", info);
      device_close (0, info -> fd);
      return -1;
    }

  /* Check if this device is convenient for capturing */
  if ( !(caps.type & VID_TYPE_CAPTURE) )
    {
      info->tveng_errno = -1;
      snprintf(info->error, 256, 
	       "%s doesn't look like a valid capture device", info
	       -> file_name);
      close(info -> fd);
      return -1;
    }

  /* Copy capability info*/
  snprintf(info->caps.name, 32, caps.name);
  info->caps.channels = caps.channels;
  info->caps.audios = caps.audios;
  info->caps.maxwidth = caps.maxwidth;
  info->caps.minwidth = caps.minwidth;
  info->caps.maxheight = caps.maxheight;
  info->caps.minheight = caps.minheight;
  info->caps.flags = 0;

  /* BTTV doesn't return properly the maximum width */
#ifdef TVENG1_BTTV_PRESENT
  if (info->caps.maxwidth > 768)
    info->caps.maxwidth = 768;
#endif

  /* Sets up the capability flags */
  if (caps.type & VID_TYPE_CAPTURE)
    info ->caps.flags |= TVENG_CAPS_CAPTURE;
  if (caps.type & VID_TYPE_TUNER)
    info ->caps.flags |= TVENG_CAPS_TUNER;
  if (caps.type & VID_TYPE_TELETEXT)
    info ->caps.flags |= TVENG_CAPS_TELETEXT;
  if (1)
    {
  if (caps.type & VID_TYPE_OVERLAY)
    info ->caps.flags |= TVENG_CAPS_OVERLAY;
  if (caps.type & VID_TYPE_CHROMAKEY)
    info ->caps.flags |= TVENG_CAPS_CHROMAKEY;
  if (caps.type & VID_TYPE_CLIPPING)
    info ->caps.flags |= TVENG_CAPS_CLIPPING;
    }
  if (caps.type & VID_TYPE_FRAMERAM)
    info ->caps.flags |= TVENG_CAPS_FRAMERAM;
  if (caps.type & VID_TYPE_SCALES)
    info ->caps.flags |= TVENG_CAPS_SCALES;
  if (caps.type & VID_TYPE_MONOCHROME)
    info ->caps.flags |= TVENG_CAPS_MONOCHROME;
  if (caps.type & VID_TYPE_SUBCAPTURE)
    info ->caps.flags |= TVENG_CAPS_SUBCAPTURE;

  /* This tries to fill the fb_info field */
  tveng1_detect_preview(info);

  /* Set some flags for this device */
  fcntl( info -> fd, F_SETFD, FD_CLOEXEC );

  /* Ignore the alarm signal */
  signal(SIGALRM, SIG_IGN);

  /* Set the controller */
  info -> current_controller = TVENG_CONTROLLER_V4L1;

  p_info->read_back_controls	= FALSE;
  p_info->audio_mode_reads_rx	= TRUE;	/* bttv TRUE, other devices? */

  /* XXX should be autodetected */
#ifdef TVENG1_BTTV_MUTE_BUG_WORKAROUND
  p_info->mute_flag_readable	= FALSE;
#else
  p_info->mute_flag_readable	= TRUE;
#endif

  /* Everything seems to be OK with this device */
  return (info -> fd);
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
int tveng1_attach_device(const char* device_file,
			enum tveng_attach_mode attach_mode,
			tveng_device_info * info)
{
  int error;
  struct private_tveng1_device_info * p_info =
    (struct private_tveng1_device_info *)info;
  struct stat st;
  int minor = -1;

  t_assert(device_file != NULL);
  t_assert(info != NULL);

  if (info -> fd) /* If the device is already attached, detach it */
    tveng_close_device(info);

  info -> file_name = strdup(device_file);
  if (!(info -> file_name))
    {
      perror("strdup");
      info->tveng_errno = errno;
      snprintf(info->error, 256, "Cannot duplicate device name");
      return -1;
    }

  if ((attach_mode == TVENG_ATTACH_XV) ||
      (attach_mode == TVENG_ATTACH_CONTROL))
    attach_mode = TVENG_ATTACH_READ;

  switch (attach_mode)
    {
      /* In V4L there is no control-only mode */
    case TVENG_ATTACH_READ:
      info -> fd = p_tveng1_open_device_file(O_RDWR, info);
      break;
    default:
      t_error_msg("switch()", "Unknown attach mode for the device",
		  info);
      free(info->file_name);
      info->file_name = NULL;
      return -1;
    };

  /*
    Errors (if any) are already aknowledged when we reach this point,
    so we don't show them again
  */
  if (info -> fd < 0)
    {
      free(info->file_name);
      info->file_name = NULL;
      return -1;
    }
  
  info -> attach_mode = attach_mode;
  /* Current capture mode is no capture at all */
  info -> current_mode = TVENG_NO_CAPTURE;

  /* We have a valid device, get some info about it */
  /* Fill in inputs */
  info->inputs = NULL;
  info->cur_input = 0;
  error = tveng1_get_inputs(info);
  if (error < 1)
    {
      if (error == 0) /* No inputs */
	{
	  info->tveng_errno = -1;
	  snprintf(info->error, 256, "No inputs for this device");
	  fprintf(stderr, "%s\n", info->error);
	}
      tveng1_close_device(info);
      return -1;
    }

  /* Fill in standards */
  info->standards = NULL;
  info->cur_standard = 0;
  error = tveng1_get_standards(info);
  if (error < 0)
    {
      tveng1_close_device(info);
      return -1;
    }

  /* Query present controls */
  info->controls = NULL;
  error = p_tveng1_build_controls(info);
  if (error == -1)
      return -1;

#ifdef TVENG1_BTTV_MUTE_BUG_WORKAROUND
  /* Mute the device, so we know for sure which is the mute value on
     startup */
//  tveng1_set_mute(0, info);
#endif

  /* Set up the palette according to the one present in the system */
  error = info->priv->current_bpp;

  if (error == -1)
    {
      tveng1_close_device(info);
      return -1;
    }

  switch(error)
    {
    case 15:
      info->format.pixformat = TVENG_PIX_RGB555;
      break;
    case 16:
      info->format.pixformat = TVENG_PIX_RGB565;
      break;
    case 24:
      info->format.pixformat = TVENG_PIX_BGR24;
      break;
    case 32:
      info->format.pixformat = TVENG_PIX_BGR32;
      break;
    default:
      info -> tveng_errno = -1;
      t_error_msg("switch()", 
		  "Cannot find appropiate palette for current display",
		  info);
      tveng1_close_device(info);
      return -1;
    }
XX();
  /* Set our desired size, make it halfway */
  info -> format.width = (info->caps.minwidth + info->caps.maxwidth)/2;
  info -> format.height = (info->caps.minheight +
			   info->caps.maxheight)/2;
XX();
  tveng1_set_capture_format(info);

  /* init the private struct */
  p_info->mmaped_data = NULL;
  p_info->queued = p_info->dequeued = 0;

  /* get the minor device number for accessing the appropiate /proc
     entry */
  if (!fstat(info -> fd, &st))
    minor = MINOR(st.st_rdev);

  p_info -> ogb_fd = -1;
  if (strstr(info -> caps.name, "OV51") &&
      strstr(info -> caps.name, "USB") &&
      minor > -1)
    {
      char filename[256];

      filename[sizeof(filename)-1] = 0;
      snprintf(filename, sizeof(filename)-1,
	       "/proc/video/ov511/%d/button", minor);
      p_info -> ogb_fd = open(filename, O_RDONLY);
      if (p_info -> ogb_fd > 0 &&
	  flock (p_info->ogb_fd, LOCK_EX | LOCK_NB) == -1)
	{
	  close(p_info -> ogb_fd);
	  p_info -> ogb_fd = -1;
	}
    }

  if (info->debug_level > 0)
    fprintf(stderr, "TVeng: V4L1 controller loaded\n");

  return info -> fd;
}

/*
  Stores in short_str and long_str (if they are non-null) the
  description of the current controller. The enum value can be found in
  info->current_controller.
  For example, V4L2 controller would say:
  short_str: 'V4L2'
  long_str: 'Video4Linux 2'
  info->current_controller: TVENG_CONTROLLER_V4L2
  This function always succeeds.
*/
static void
tveng1_describe_controller(char ** short_str, char ** long_str,
			   tveng_device_info * info)
{
  t_assert(info != NULL);
  if (short_str)
    *short_str = "V4L1";
  if (long_str)
    *long_str = "Video4Linux 1";
}

/* Closes a device opened with tveng_init_device */
static void tveng1_close_device(tveng_device_info * info)
{
  struct private_tveng1_device_info *p_info=
    (struct private_tveng1_device_info*) info;
  tv_control *tc;

  t_assert(info != NULL);

  tveng_stop_everything(info);

  device_close(LOG_FP, info -> fd);
  info -> fd = 0;
  info -> current_controller = TVENG_CONTROLLER_NONE;

  if (info -> file_name)
    free(info -> file_name);
  if (info -> inputs)
    free(info -> inputs);
  if (info -> standards)
    free(info -> standards);

	while ((tc = info->controls)) {
		info->controls = tc->_next;
		free_control (tc);
	}

  if (p_info -> ogb_fd > 0)
    device_close(LOG_FP, p_info->ogb_fd);
  p_info ->ogb_fd = -1;

  info -> num_standards = 0;
  info -> num_inputs = 0;
  info -> inputs = NULL;
  info -> standards = NULL;
  info -> file_name = NULL;

  if (info->debug_level > 0)
    fprintf(stderr, "\nTVeng: V4L1 controller unloaded\n");
}

/*
  Functions for controlling the video capture. All of them return -1
  in case of error, so any value != -1 should be considered valid
  (unless explicitly stated in the description of the function) 
*/
/*
  Returns the number of inputs in the given device and fills in info
  with the correct info, allocating memory as needed
*/
static int tveng1_get_inputs(tveng_device_info * info)
{
  /* In v4l, inputs are called channels */
  struct video_channel channel;
  int i;

  t_assert(info != NULL);

  if (info->inputs)
    free(info->inputs);

  info->inputs = NULL;
  info->num_inputs = 0;
  info->cur_input = 0;

  for (i=0;i<info->caps.channels;i++)
    {
      CLEAR (channel);
      channel.channel = i;
      if (v4l_ioctl(info->fd, VIDIOCGCHAN, &channel))
	continue;
      info->inputs = realloc(info->inputs, (info->num_inputs+1)*
			     sizeof(struct tveng_enum_input));
      info->inputs[info->num_inputs].id = i;
      info->inputs[info->num_inputs].index = info->num_inputs;
      snprintf(info->inputs[info->num_inputs].name, 32, channel.name);
      info->inputs[info->num_inputs].name[31] = 0;
      info->inputs[info->num_inputs].hash =
	tveng_build_hash(info->inputs[info->num_inputs].name);
      info->inputs[info->num_inputs].tuners = channel.tuners;
      info->inputs[info->num_inputs].flags = 0;
      if (channel.flags & VIDEO_VC_TUNER)
	info->inputs[info->num_inputs].flags |= TVENG_INPUT_TUNER;
      if (channel.flags & VIDEO_VC_AUDIO)
	info->inputs[info->num_inputs].flags |= TVENG_INPUT_AUDIO;
      /* get the correct input type */
      switch(channel.type)
	{
	case VIDEO_TYPE_TV:
	  info->inputs[info->num_inputs].type = TVENG_INPUT_TYPE_TV;
	  break;
	case VIDEO_TYPE_CAMERA:
	  info->inputs[info->num_inputs].type = TVENG_INPUT_TYPE_CAMERA;
	  break;
	default:
	  break;
	}
      info->num_inputs++;
    }

  input_collisions(info);

  if (i) /* If there is any channel, switch to the first one */
    {
      CLEAR (channel);
      channel.channel = 0;
      if (v4l_ioctl(info->fd, VIDIOCGCHAN, &channel))
	{
	  info -> tveng_errno = errno;
	  t_error("VIDIOCGCHAN", info);
	  return -1;
	}
      if (v4l_ioctl(info->fd, VIDIOCSCHAN, &channel))
	{
	  info -> tveng_errno = errno;
	  t_error("VIDIOCSCHAN", info);
	  return -1;
	}
    }

  return (info->num_inputs);
}

/*
  Sets the current input for the capture
*/
static
int tveng1_set_input(struct tveng_enum_input * input,
		     tveng_device_info * info)
{
  enum tveng_capture_mode current_mode;
  struct video_channel channel;
  int retcode;

  t_assert(info != NULL);
  t_assert(input != NULL);

  current_mode = tveng_stop_everything(info);

  /* Fill in the channel with the appropiate info */
  CLEAR (channel);
  channel.channel = input->id;
  if (v4l_ioctl(info->fd, VIDIOCGCHAN, &channel))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGCHAN", info);
      return -1;
    }    

  /* Now set the channel */
  if ((retcode = v4l_ioctl(info->fd, VIDIOCSCHAN, &channel)))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCSCHAN", info);
    }

  info->cur_input = input->index;

  /* Maybe there are some other standards, get'em */
  tveng1_get_standards(info);

  /* Start capturing again as if nothing had happened */
  tveng_restart_everything(current_mode, info);

  return retcode;
}

/* For declaring the possible standards */
struct dummy_standard_struct{
  int id;
  char * name;
  tv_videostd_id stdid;
};

/*
  Queries the device about its standards. Fills in info as appropiate
  and returns the number of standards in the device. This is for the
  first tuner in the current input, should be enough since most (all)
  inputs have 1 or less tuners.
*/
static int tveng1_get_standards(tveng_device_info * info)
{
  int count = 0; /* Number of standards */
  struct video_channel channel;
  int i;
  struct dummy_standard_struct * std_t;

  /* The table with the possible standards as in the V4L1 spec */
  struct dummy_standard_struct spec_t[] =
  {
    {  0, "PAL",	TV_VIDEOSTD_PAL	},
    {  1, "NTSC",	TV_VIDEOSTD_NTSC_M },
    {  2, "SECAM",	TV_VIDEOSTD_SECAM },
    {  3, "AUTO",	TV_VIDEOSTD_UNKNOWN },
    { -1, NULL, 0 }
  };
  /* The set of standards in the V4L bttv controller */
  struct dummy_standard_struct bttv_t[] =
  {
    {  0, "PAL",	TV_VIDEOSTD_PAL },
    {  1, "NTSC",	TV_VIDEOSTD_NTSC_M },
    {  2, "SECAM",	TV_VIDEOSTD_SECAM },
    {  3, "PAL-NC",	TV_VIDEOSTD_PAL_NC },
    {  4, "PAL-M",	TV_VIDEOSTD_PAL_M },
    {  5, "PAL-N",	TV_VIDEOSTD_PAL_N },
    {  6, "NTSC-JP",	TV_VIDEOSTD_NTSC_M_JP },
    { -1, NULL, 0 }
  };

  /* Free any previously allocated mem */
  if (info->standards)
    free(info->standards);

  info->standards = NULL;
  info->cur_standard = 0;
  info->num_standards = 0;
  info->tveng_errno = 0; /* Set errno flag */

  /* If it has no tuners, we are done */
  if (!(info->caps.flags & TVENG_CAPS_TUNER))
    return 0;

  /* This comes from xawtv, in its author's words: "dirty hack time"
   */
  std_t = spec_t;
#ifdef TVENG1_BTTV_PRESENT
  /* rather poor, imho, but we shouldn't happily send a bttv private
     ioctl to the innocent driver */
  if (strstr(info->caps.name, "bt") || strstr(info->caps.name, "BT"))
    
#define BTTV_VERSION  	        _IOR('v' , BASE_VIDIOCPRIVATE+6, int)
#define IOCTL_ARG_TYPE_CHECK_BTTV_VERSION(x) ((void) 0)
  /* dirty hack time / v4l design flaw -- works with bttv only
   * this adds support for a few less common PAL versions */
  /* returns version number or -1 */

  if (-1 != v4l_ioctl(info->fd,BTTV_VERSION, (int *) 0)) {
    std_t = bttv_t;
  }
#endif  

  while (std_t[count].name)
    {
      /* Valid norm, add it to the list */
      info -> standards = realloc(info->standards,
				  sizeof(struct tveng_enumstd)*(count+1));
      info -> standards[count].stdid = std_t[count].stdid;
      info -> standards[count].id = std_t[count].id;
      info -> standards[count].index = count;
      snprintf(info -> standards[count].name, 32, std_t[count].name);
      info -> standards[count].name[31] = 0;
      info -> standards[count].hash =
	tveng_build_hash(info->standards[count].name);
      /* Weird that there's no strcasestr */
      if (strstr(std_t[count].name, "ntsc") ||
	  strstr(std_t[count].name, "NTSC") ||
	  strstr(std_t[count].name, "Ntsc") ||
	  strstr(std_t[count].name, "PAL-M"))
	{
	  /* (M) NTSC, NTSC-J (Japan) */
	  /* (M) PAL similar (M) NTSC except PAL color modulation */
	  /* NTSC 4.43 aka PAL-60 (525/60, PAL modulation, 4.43 MHz FSC) */
	  /*  what's the PAL VHS -> NTSC TV counterpart, "PAL 3.57"? */
	  info -> standards[count].width = 640;
	  info -> standards[count].height = 480;
	  info -> standards[count].frame_rate = 30000 / 1001.0;
	}
      else
	{
	  /* (B, B1, G, H, I, D, N) PAL */
	  /* (Nc) PAL similar (N) PAL except 3.58 MHz FSC */
	  /* (B, G, D, K, K1, L) SECAM */
	  info -> standards[count].width = 768;
	  info -> standards[count].height = 576;
	  info -> standards[count].frame_rate = 25.0;
	}
      count++;
    }

  standard_collisions(info);

  CLEAR (channel);
  /* Get the current standard if this input has a tuner */
  if (info->inputs[info->cur_input].tuners)
    channel.channel = info->inputs[info->cur_input].id;
  else /* look for the input we're using to fake the tuner */
    {
      int j;
      for (j=0; j<info->num_inputs; j++)
	if (info->inputs[j].tuners)
	  break;
      /* Otherwise we've a broken driver */
      t_assert(j != info->num_inputs);
      channel.channel = info->inputs[j].id;
    }

  if (v4l_ioctl(info->fd, VIDIOCGCHAN, &channel))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGCHAN", info);
      return -1;
    }

  for (i = 0; i<count; i++)
    if (info->standards[i].id == channel.norm)
      break; /* Found */

  if (i != count)
    info->cur_standard = i;
    
  return (info->num_standards = count);
}

/*
  Sets the current standard for the capture. standard is the name for
  the desired standard. updates cur_standard
*/
static
int tveng1_set_standard(struct tveng_enumstd * std, tveng_device_info * info)
{
  enum tveng_capture_mode current_mode;
  /* in v4l the standard is known as the norm of the channel */
  struct video_channel channel;
  int retcode;

  t_assert(info != NULL);
  t_assert(std != NULL);

  current_mode = tveng_stop_everything(info);

  if ((!info->inputs[info->cur_input].tuners) &&
      (info->caps.flags & TVENG_CAPS_TUNER))
    { /* switch to an input with tuner and then switch back */
      int i = 0;
      struct video_channel schan;

      for (i=0; i<info->num_inputs; i++)
	if (info->inputs[i].tuners)
	  {
	    CLEAR (schan);
	    schan.channel = info->inputs[i].id;
	    if (v4l_ioctl(info->fd, VIDIOCGCHAN, &schan))
	      continue; /* not valid */
	    schan.norm = std->id;
	    if (!v4l_ioctl(info->fd, VIDIOCSCHAN, &schan))
	      break; /* assume the std was changed correctly */
	  }

      if (i==info->num_inputs)
	{
	  t_error_msg("set_standard",
		      "No available input found for setting standard",
		      info);
	  retcode = -1;
	}
      else
	{
	  info->cur_standard = std->index;
	  retcode = 0;
	}

      /* restore current input */
      /* no error checking, try to get as far as we can */
      fprintf(stderr, "Restoring %s\n", info->inputs[info->cur_input].name);
#warning
      schan.channel = info->inputs[info->cur_input].id;
      printf("%d [%s]\n", v4l_ioctl(info->fd, VIDIOCGCHAN, &schan), schan.name);
      printf("%d\n", v4l_ioctl(info->fd, VIDIOCSCHAN, &schan));
    }
  /* The current input can do itself */
  else if (info->inputs[info->cur_input].tuners)
    {
      CLEAR (channel);
      /* Fill in the channel with the appropiate info */
      channel.channel = info->inputs[info->cur_input].id;
      if (v4l_ioctl(info->fd, VIDIOCGCHAN, &channel))
	{
	  info->tveng_errno = errno;
	  t_error("VIDIOCGCHAN", info);
	  return -1;
	}

      /* Now set the channel and the norm */
      channel.norm = std->id;
      if ((retcode = v4l_ioctl(info->fd, VIDIOCSCHAN, &channel)))
	{
	  info->tveng_errno = errno;
	  t_error("VIDIOCSCHAN", info);
	}
      info->cur_standard = std->index;
    }
  else
    {
      t_error_msg("set_standard", "No tuners in the given device", info);
      retcode = -1;
    }

  /* Start capturing again as if nothing had happened */
  tveng_restart_everything(current_mode, info);

  return retcode;
}

/* Updates the current capture format info. -1 if failed */
static int
tveng1_update_capture_format(tveng_device_info * info)
{
  struct video_picture pict;
  struct video_window window;

  t_assert(info != NULL);

  CLEAR (window);

  if (v4l_ioctl(info->fd, VIDIOCGPICT, &pict))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGPICT", info);
      return -1;
    }

  /* Transform the palette value into a tveng value */
  switch(pict.palette)
    {
    case VIDEO_PALETTE_RGB555:
      info->format.depth = 15;
      info->format.pixformat = TVENG_PIX_RGB555;
      break;
    case VIDEO_PALETTE_RGB565:
      info->format.depth = 16;
      info->format.pixformat = TVENG_PIX_RGB565;
      break;
    case VIDEO_PALETTE_RGB24:
      info->format.depth = 24;
      info->format.pixformat = TVENG_PIX_BGR24;
      break;
    case VIDEO_PALETTE_RGB32:
      info->format.depth = 32;
      info->format.pixformat = TVENG_PIX_BGR32;
      break;
    case VIDEO_PALETTE_YUV420P:
      info->format.depth = 12;
      info->format.pixformat = TVENG_PIX_YUV420;
      break;
    case VIDEO_PALETTE_YUV422:
      info->format.depth = 16;
      info->format.pixformat = TVENG_PIX_YUYV;
      break;
    case VIDEO_PALETTE_YUYV:
      info->format.depth = 16;
      info->format.pixformat = TVENG_PIX_YUYV;
      break;
    default:
      info->tveng_errno = -1; /* unknown */
      t_error_msg("switch()",
		  "Cannot understand the actual palette", info);
      return -1;
    }

  /* Ok, now get the video window dimensions */
  if (v4l_ioctl(info->fd, VIDIOCGWIN, &window))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGWIN", info);
      return -1;
    }

  /* Fill in the format structure (except for the data field) */
  info->format.bpp = ((double)info->format.depth)/8;
  info->format.width = window.width;
  info->format.height = window.height;
  info->format.bytesperline = window.width * info->format.bpp;
  info->format.sizeimage = info->format.height* info->format.bytesperline;
  info->window.x = window.x;
  info->window.y = window.y;
  info->window.width = window.width;
  info->window.height = window.height;
  /* These two are write-only */
  info->window.clipcount = 0;
  info->window.clips = NULL;

XX();

  return 0;
}

/* -1 if failed. Sets the pixformat and fills in info -> pix_format
   with the correct values  */
static int
tveng1_set_capture_format(tveng_device_info * info)
{
  struct video_picture pict;
  struct video_window window;
  enum tveng_capture_mode mode;

  CLEAR (pict);
  CLEAR (window);

XX();

  t_assert(info != NULL);

  mode = tveng_stop_everything(info);

  if (v4l_ioctl(info->fd, VIDIOCGPICT, &pict))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGPICT", info);
      tveng_restart_everything(mode, info);
      return -1;
    }

  /* Transform the given palette value into a V4L value */
  switch(info->format.pixformat)
    {
    case TVENG_PIX_RGB555:
      pict.palette = VIDEO_PALETTE_RGB555;
      pict.depth = 15;
      break;
    case TVENG_PIX_RGB565:
      pict.palette = VIDEO_PALETTE_RGB565;
      pict.depth = 16;
      break;
    case TVENG_PIX_RGB24:
    case TVENG_PIX_BGR24: /* No way to distinguish these two in V4L */
      pict.palette = VIDEO_PALETTE_RGB24;
      pict.depth = 24;
      break;
    case TVENG_PIX_RGB32:
    case TVENG_PIX_BGR32:
      pict.palette = VIDEO_PALETTE_RGB32;
      pict.depth = 32;
      break;
    case TVENG_PIX_YUYV:
    case TVENG_PIX_UYVY:
      pict.palette = VIDEO_PALETTE_YUV422;
      pict.depth = 16;
      break;
    case TVENG_PIX_YUV420:
    case TVENG_PIX_YVU420:
      pict.palette = VIDEO_PALETTE_YUV420P;
      pict.depth = 12;
      break;
    default:
      info->tveng_errno = -1; /* unknown */
      t_error_msg("switch()", "Cannot understand the given palette",
		  info);
      tveng_restart_everything(mode, info);
      return -1;
    }

  /* Set this values for the picture properties */
  if (v4l_ioctl(info->fd, VIDIOCSPICT, &pict))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCSPICT", info);
      tveng_restart_everything(mode, info);
      return -1;
    }

  info->format.width = (info->format.width + 3) & -4;
  info->format.height = (info->format.height + 3) & -4;

  info->format.width = SATURATE (info->format.width,
				 info->caps.minwidth,
				 info->caps.maxwidth);
  info->format.height = SATURATE (info->format.height,
				  info->caps.minheight,
				  info->caps.maxheight);

  window.width = info->format.width;
  window.height = info->format.height;
  window.clips = NULL;
  window.clipcount = 0;

XX();

  /* Ok, now set the video window dimensions */
  if (v4l_ioctl(info->fd, VIDIOCSWIN, &window))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCSWIN", info);
      tveng_restart_everything(mode, info);
      return -1;
    }

  tveng_restart_everything(mode, info);

  /* Check fill in info with the current values (may not be the ones
     asked for) */
XX();
  if (tveng1_update_capture_format(info) == -1)
    return -1; /* error */

  return 0; /* Success */
}


/* the defined audio decoding modes */
struct p_tveng1_audio_decoding_entry
{
  char * label;
  __u16 id;
};

static
struct p_tveng1_audio_decoding_entry audio_decoding_modes[] =
{
  { N_("Automatic"), 0 },
  { N_("Mono"), VIDEO_SOUND_MONO },
  { N_("Stereo"), VIDEO_SOUND_STEREO },
  { N_("Alternate 1"), VIDEO_SOUND_LANG1 },
  { N_("Alternate 2"), VIDEO_SOUND_LANG2 }
};

/* i don't like defines too much, but it's better this way */
#define num_audio_decoding_modes \
(sizeof(audio_decoding_modes)/sizeof(struct p_tveng1_audio_decoding_entry))

/* tests if audio decoding selecting actually works, NULL if not */
static char ** p_tveng1_test_audio_decode (tveng_device_info * info)
{
  struct video_audio audio;
  int i, j;
  char ** list = NULL; /* The returned list of menu entries labels */

  CLEAR (audio);

  if (v4l_ioctl(info->fd, VIDIOCGAUDIO, &audio))
    {
      info -> tveng_errno = errno; /* Nobody will check this, but
				      anyway */
      t_error("VIDIOCGAUDIO", info);
      return NULL;
    }

  /*
   *  FIXME
   *  According to /linux/Documentation/video4linux/API.html
   *  audio.mode is the "mode the audio input is in". The bttv driver
   *  returns the received audio on read, a set, not the selected
   *  mode and not capabilities. One write a single bit must be
   *  set, zero selects autodetection. NB VIDIOCSAUDIO is not w/r.
   */

  for (i = 0; i<num_audio_decoding_modes; i++)
    {
      audio.mode = audio_decoding_modes[i].id;
      if (v4l_ioctl(info->fd, VIDIOCSAUDIO, &audio))
	{
	  info->tveng_errno = errno;
	  t_error("VIDIOCSAUDIO", info);
	  goto failure;
	}
      if (v4l_ioctl(info->fd, VIDIOCGAUDIO, &audio))
	{
	  info->tveng_errno = errno;
	  t_error("VIDIOCGAUDIO", info);
	  goto failure;
	}
      /* Ok, add this id */
      list = realloc(list, sizeof(char*) * (i+1));
      t_assert(list != NULL);
      list[i] = strdup(audio_decoding_modes[i].label);
    }

  /* FIXME restore previous mode */
  /* audio.mode = cur_value; */
  audio.mode = 0; /* autodetect */
  if (v4l_ioctl(info->fd, VIDIOCSAUDIO, &audio))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCSAUDIO", info);
      goto failure;
    }

  /* Add the NULL at the end of the list */
  list = realloc(list, sizeof(char*) * (i+1));
  list[i] = NULL;

  return list; /* Success, the control apparently works */

failure:
  audio.mode = 0; /* autodetect */
  v4l_ioctl(info->fd, VIDIOCSAUDIO, &audio);

  if (list)
    {
      for (j=0; j<(i+1); j++)
        free(list[j]);
      free(list);
    }
  return NULL;
}





/*
 *  Controls
 */

static void
update_controls			(tveng_device_info *	info,
				 struct video_picture *	pict,
				 struct video_audio *	audio,
				 unsigned int		controls)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	tv_control *tc;

	for (tc = p_info->info.controls; tc; tc = tc->_next) {
		struct control *c = C(tc);
		int value;

		if (c->pub._device != info)
			continue;

		if (!(c->id & controls))
			continue;

		switch (c->id) {
		case CONTROL_BRIGHTNESS: value = pict->brightness; break;
		case CONTROL_HUE:	 value = pict->hue; break;
		case CONTROL_COLOUR:	 value = pict->colour; break;
		case CONTROL_CONTRAST:	 value = pict->contrast; break;

		case CONTROL_MUTE:
			if (!p_info->mute_flag_readable)
				continue;

			value = ((audio->flags & VIDEO_AUDIO_MUTE) != 0);
			break;

		case CONTROL_VOLUME:	value = audio->volume; break;
		case CONTROL_BASS:	value = audio->bass; break;
		case CONTROL_TREBLE:	value = audio->treble; break;
		case CONTROL_BALANCE:	value = audio->balance; break;

		case CONTROL_AUDIO_DECODING:
			if (p_info->audio_mode_reads_rx)
				continue;

			for (value = 0; value < num_audio_decoding_modes; value++)
				if (audio->mode == audio_decoding_modes[value].id)
					break;

			if (value >= num_audio_decoding_modes) {
				p_info->info.tveng_errno = -1;
				t_error_msg ("switch()", "Unknown decoding mode (%d)",
					     info, audio->mode);
				continue;
			}

			break;
			
		default:
			assert (0);
		}

		if (c->pub.value != value) {
			c->pub.value = value;
			tv_callback_notify (&c->pub, c->pub._callback);
		}
	}
}

static tv_bool
tveng1_update_control		(tveng_device_info *	info,
				 tv_control *		tc)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	struct video_picture pict;
	struct video_audio audio;
	unsigned int controls;

	if (tc)
		controls = C(tc)->id;
	else
		controls = p_info->all_controls;

	if (controls & VIDEO_CONTROLS) {
		CLEAR (pict);

		if (-1 == v4l_ioctl (p_info->info.fd, VIDIOCGPICT, &pict)) {
			p_info->info.tveng_errno = errno;
			t_error("VIDIOCGPICT", &p_info->info);
			return FALSE;
		}
	}

	if (controls & AUDIO_CONTROLS) {
		CLEAR (audio);

		if (-1 == v4l_ioctl (p_info->info.fd, VIDIOCGAUDIO, &audio)) {
			p_info->info.tveng_errno = errno;
			t_error("VIDIOCGAUDIO", &p_info->info);
			return FALSE;
		}
	}

	update_controls (&p_info->info, &pict, &audio, controls);

	return TRUE;
}

static tv_bool
tveng1_set_control		(tveng_device_info *	info,
				 tv_control *		tc,
				 int			value)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	struct control *c = C(tc);

	if (c->id & VIDEO_CONTROLS) {
		struct video_picture pict;

		CLEAR (pict);

		if (-1 == v4l_ioctl (p_info->info.fd, VIDIOCGPICT, &pict)) {
			p_info->info.tveng_errno = errno;
			t_error ("VIDIOCGPICT", &p_info->info);
			return FALSE;
		}

		switch (c->id) {
		case CONTROL_BRIGHTNESS: pict.brightness = value; break;
		case CONTROL_HUE:	 pict.hue = value; break;
		case CONTROL_COLOUR:	 pict.colour = value; break;
		case CONTROL_CONTRAST:   pict.contrast = value; break;
		default:
			break;
		}

		if (-1 == v4l_ioctl (p_info->info.fd, VIDIOCSPICT, &pict)) {
			p_info->info.tveng_errno = errno;
			t_error ("VIDIOCSPICT", &p_info->info);
			return FALSE;
		}

		if (p_info->read_back_controls) {
			/* Error ignored */
			v4l_ioctl (p_info->info.fd, VIDIOCGPICT, &pict);
		}

		update_controls (&p_info->info, &pict, NULL, VIDEO_CONTROLS);

	} else if (c->id & AUDIO_CONTROLS) {
		struct video_audio audio;
		unsigned int rx_mode;
		tv_bool no_read;

		CLEAR (audio);

		if (-1 == v4l_ioctl (p_info->info.fd, VIDIOCGAUDIO, &audio)) {
			p_info->info.tveng_errno = errno;
			t_error ("VIDIOCGAUDIO", &p_info->info);
			return FALSE;
		}

		switch (c->id) {
		case CONTROL_VOLUME:	audio.volume = value; break;
		case CONTROL_BASS:	audio.bass = value; break;
		case CONTROL_TREBLE:	audio.treble = value; break;
		case CONTROL_BALANCE:	audio.balance = value; break;
		default:
			break;
		}

		no_read = FALSE;

		if (c->id == CONTROL_MUTE) {
			audio.flags &= ~VIDEO_AUDIO_MUTE;

			if (value)
				audio.flags |= VIDEO_AUDIO_MUTE;

			no_read = !p_info->mute_flag_readable;

		} else if (!p_info->mute_flag_readable) {
			audio.flags &= ~VIDEO_AUDIO_MUTE;

			if (p_info->control_mute
			    && p_info->control_mute->value)
				audio.flags |= VIDEO_AUDIO_MUTE;
 		}

		rx_mode = audio.mode;

		if (c->id == CONTROL_AUDIO_DECODING) {
			audio.mode = audio_decoding_modes[value].id;

			no_read = p_info->audio_mode_reads_rx;

		} else if (p_info->audio_mode_reads_rx) {
			if (p_info->control_audio_dec)
				audio.mode = audio_decoding_modes
					[p_info->control_audio_dec->value].id;
			else
				audio.mode = 0; /* automatic */
		}

		if (-1 == v4l_ioctl (p_info->info.fd, VIDIOCSAUDIO, &audio)) {
			p_info->info.tveng_errno = errno;
			t_error ("VIDIOCSAUDIO", &p_info->info);
			return FALSE;
		}

		if (p_info->read_back_controls) {
			if (-1 == v4l_ioctl (p_info->info.fd, VIDIOCGAUDIO, &audio)
			    && p_info->audio_mode_reads_rx)
				audio.mode = rx_mode;
		}

		if (no_read && c->pub.value != value) {
			c->pub.value = value;
			tv_callback_notify (&c->pub, c->pub._callback);
		}

		update_controls (&p_info->info, NULL, &audio, AUDIO_CONTROLS);
	} else {
		assert (0);
	}

	return TRUE;
}

static tv_control *
add_control			(tveng_device_info *	info,
				 unsigned int		id,
				 const char *		label,
				 tv_control_id		tcid,
				 tv_control_type	type,
				 int			cur,
				 int			def,
				 int			minimum,
				 int			maximum,
				 int			step)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	struct control c;
	tv_control *tc;

	CLEAR (c);

	c.id = id;

	c.pub.id = tcid;

	if (!(c.pub.label = strdup (_(label))))
		return NULL;

	c.pub.type	= type;
	c.pub.minimum	= minimum;
	c.pub.maximum	= maximum;
	c.pub.step	= step;
	c.pub.reset	= def;
	c.pub.value	= cur;

	c.pub._device	= info;

	if ((tc = append_control (info, &c.pub, sizeof (c)))) {
		p_info->all_controls |= id;
	} else {
		free ((char *) c.pub.label);
		return NULL;
	}

	return tc;
}

static inline tv_control *
add_audio_dec_control		(tveng_device_info *	info)
{
	struct control *c;

	if (!(c = calloc (1, sizeof (*c))))
		return NULL;

	c->id = CONTROL_AUDIO_DECODING;

	if (!(c->pub.label = strdup (_("Audio")))) {
		free_control (&c->pub);
		return NULL;
	}

	c->pub.maximum = 4; /* XXX */
	c->pub.type = TV_CONTROL_TYPE_CHOICE;

	c->pub._device = info;

	if (!(c->pub.menu = (char **)
	      p_tveng1_test_audio_decode (info))) {
		free_control (&c->pub);
		return NULL;
	}

	append_control (info, &c->pub, 0);

	P_INFO (info)->all_controls |= CONTROL_AUDIO_DECODING;

	return &c->pub;
}

/* Private, builds the controls structure */
static int
p_tveng1_build_controls(tveng_device_info * info)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	struct video_picture pict;
	struct video_audio audio;

	p_info->all_controls = 0;

  	CLEAR (pict);

	/*
	 *  The range 0 ... 65535 is mandated by the v4l spec.
	 *  We add a reset value of 32768 and a step value of 256.
	 *  The actual reset value and hardware resolution is not
	 *  reported by v4l, but for the GUI these values should do.
	 */
#define ADD_STD_CONTROL(name, label, id, value)				\
	add_control (info, CONTROL_##name, _(label),			\
		     TV_CONTROL_ID_##id, TV_CONTROL_TYPE_INTEGER,	\
		     value, 32768, 0, 65535, 256)

	if (0 == v4l_ioctl (info->fd, VIDIOCGPICT, &pict)) {
		ADD_STD_CONTROL (BRIGHTNESS, "Brightness", BRIGHTNESS, pict.brightness);
		ADD_STD_CONTROL (CONTRAST,   "Contrast",   CONTRAST,   pict.contrast);
		ADD_STD_CONTROL (COLOUR,     "Saturation", SATURATION, pict.colour);
		ADD_STD_CONTROL (HUE,        "Hue",        HUE,        pict.hue);
	} else {
		info->tveng_errno = errno;
		t_error("VIDIOCGPICT", info);
		return -1;
	}

	CLEAR (audio);
   
	if (0 == v4l_ioctl (info->fd, VIDIOCGAUDIO, &audio)) {
		tv_bool rewrite = FALSE;

		if (audio.flags & VIDEO_AUDIO_MUTABLE) {
			if (!p_info->mute_flag_readable) {
				audio.flags |= VIDEO_AUDIO_MUTE;
				rewrite = TRUE;
			}

			p_info->control_mute =
				add_control (info, CONTROL_MUTE, _("Mute"),
					     TV_CONTROL_ID_MUTE,
					     TV_CONTROL_TYPE_BOOLEAN,
					     (audio.flags & VIDEO_AUDIO_MUTE) != 0,
					     0, 0, 1, 1);
		}

		if (audio.flags & VIDEO_AUDIO_VOLUME)
			ADD_STD_CONTROL (VOLUME, "Volume", VOLUME, audio.volume);

		if (audio.flags & VIDEO_AUDIO_BASS)
			ADD_STD_CONTROL (BASS, "Bass", BASS, audio.bass);

		if (audio.flags & VIDEO_AUDIO_TREBLE)
			ADD_STD_CONTROL (TREBLE, "Treble", TREBLE, audio.treble);

#ifdef VIDEO_AUDIO_BALANCE /* In the V4L API, but not present in bttv */
		if (audio.flags & VIDEO_AUDIO_BALANCE)
			ADD_STD_CONTROL (BALANCE, "Balance", UNKNOWN, audio.balance);
#endif
		p_info->control_audio_dec =
			add_audio_dec_control (info);

		if (p_info->audio_mode_reads_rx) {
			if (p_info->control_audio_dec) {
				audio.mode = audio_decoding_modes
					[p_info->control_audio_dec->value].id;
				rewrite = TRUE;
			} else {
				audio.mode = 0; /* automatic */
			}
		}

		if (rewrite) {
			/* Can't read values, we must write to
			   synchronize. Error ignored. */
			v4l_ioctl (info->fd, VIDIOCSAUDIO, &audio);
		}
	} else if (errno != EINVAL) {
		info->tveng_errno = errno;
		t_error("VIDIOCGAUDIO", info);
		return -1;
	}

	return 0;
}


/*
  Tunes the current input to the given freq. Returns -1 on error.
*/
static int
tveng1_tune_input(uint32_t freq, tveng_device_info * info)
{
  unsigned long new_freq;
  struct video_tuner tuner;

  memset(&tuner, 0, sizeof(struct video_tuner));

  t_assert(info != NULL);
  t_assert(info->cur_input < info->num_inputs);
  t_assert(info->cur_input >= 0);

  /* Check that there are tuners in the current input */
  if (info->inputs[info->cur_input].tuners == 0)
    return 0; /* Success (we shouldn't be tuning, anyway) */

  /* Get info about the current tuner (usually the 0 tuner) */
  tuner.tuner = 0;
  if (v4l_ioctl(info->fd, VIDIOCGTUNER, &tuner))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGTUNER", info);
      return -1;
    }
  if (tuner.flags & VIDEO_TUNER_LOW)
    new_freq = (freq * 16);
  else
    new_freq = (freq * 0.016);

  /* Clip to the valid freq range */
  if (new_freq < tuner.rangelow)
    new_freq = tuner.rangelow;
  if (new_freq > tuner.rangehigh)
    new_freq = tuner.rangehigh;

  /* To 'fix' the current behaviour of bttv, i don't like
     it too much (it mutes the input if the signal strength
     is too low) */
  if (P_INFO (info)->control_mute)
    tveng1_update_control (info, P_INFO (info)->control_mute);

  /* Ok, tune the current input */
  if (v4l_ioctl(info->fd, VIDIOCSFREQ, &new_freq))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCSFREQ", info);
      return -1;
    }

  /* Restore the mute status. This makes bttv behave like i want */
  if (P_INFO (info)->control_mute)
    tveng1_set_control (info, P_INFO (info)->control_mute,
			P_INFO (info)->control_mute->value);

  return 0; /* Success */
}

/*
  Gets the signal strength and the afc code. The afc code indicates
  how to get a better signal, if negative, tune higher, if negative,
  tune lower. 0 means no idea of feature not present in the current
  controller (i.e. V4L1). Strength and/or afc can be NULL pointers,
  that would mean ignore that parameter.
*/
static int
tveng1_get_signal_strength (int *strength, int * afc,
			   tveng_device_info * info)
{
  struct video_tuner tuner;

  memset(&tuner, 0, sizeof(struct video_tuner));

  t_assert(info != NULL);
  t_assert(info->cur_input < info->num_inputs);
  t_assert(info->cur_input >= 0);

  /* Check that there are tuners in the current input */
  if (info->inputs[info->cur_input].tuners == 0)
    return -1;

  /* Get info about the current tuner (usually the 0 tuner) */
  tuner.tuner = 0;
  if (v4l_ioctl(info->fd, VIDIOCGTUNER, &tuner))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGTUNER", info);
      return -1;
    }

  if (strength)
    *strength = tuner.signal;

  if (afc)
    *afc = 0; /* No such thing in the V4L1 spec */

  return 0; /* Success */
}

/*
  Stores in freq the currently tuned freq. Returns -1 on error.
*/
static int
tveng1_get_tune(uint32_t * freq, tveng_device_info * info)
{
  unsigned long real_freq;
  struct video_tuner tuner;

  t_assert(info != NULL);
  t_assert(freq != NULL);
  t_assert(info->cur_input < info->num_inputs);
  t_assert(info->cur_input >= 0);

  /* Check that there are tuners in the current input */
  if (info->inputs[info->cur_input].tuners == 0)
    {
      if (freq)
	*freq = 0;
      info->tveng_errno = -1;
      t_error_msg("tuners check",
		  _("There are no tuners for the active input"),
		  info);
      return -1;
    }

  /* get the current tune value */
  if (v4l_ioctl(info->fd, VIDIOCGFREQ, &real_freq))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGFREQ", info);
      return -1;
    }

  /* Now we must convert this to a valid kHz value */
  /* Get info about the current tuner (usually the 0 tuner) */
  tuner.tuner = 0;
  if (v4l_ioctl(info->fd, VIDIOCGTUNER, &tuner))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGTUNER", info);
      return -1;
    }

  if (tuner.flags & VIDEO_TUNER_LOW)
    *freq = (real_freq / 16);
  else
    *freq = (real_freq / 0.016);

  return 0;
}

/*
  Gets the minimum and maximum freq that the current input can
  tune. If there is no tuner in this input, -1 will be returned.
  If any of the pointers is NULL, its value will not be filled.
*/
static int
tveng1_get_tuner_bounds(uint32_t * min, uint32_t * max, tveng_device_info *
			info)
{
  struct video_tuner tuner;

  memset(&tuner, 0, sizeof(struct video_tuner));

  t_assert(info != NULL);
  t_assert(info->cur_input < info->num_inputs);
  t_assert(info->cur_input >= 0);

  /* Check that there are tuners in the current input */
  if (info->inputs[info->cur_input].tuners == 0)
    return -1;

  /* Get info about the current tuner (usually the 0 tuner) */
  tuner.tuner = 0;
  if (v4l_ioctl(info->fd, VIDIOCGTUNER, &tuner))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGTUNER", info);
      return -1;
    }

  if (min)
    *min = tuner.rangelow;
  if (max)
    *max = tuner.rangehigh;

  if (tuner.flags & VIDEO_TUNER_LOW)
    {
      if (min)
	*min /= 16;
      if (max)
	*max /= 16;
    }
  else
    {
      if (min)
	*min /= 0.016;
      if (max)
	*max /= 0.016;
    }

  return 0; /* Success */
}

/* Two internal functions, both return -1 on error */
static int p_tveng1_queue(tveng_device_info * info);
static int p_tveng1_dequeue(tveng_image_data * where,
			    tveng_device_info * info);

static void p_tveng1_timestamp_init(tveng_device_info *info);

/*
  Sets up the capture device so any read() call after this one
  succeeds. Returns -1 on error.
*/
static int
tveng1_start_capturing(tveng_device_info * info)
{
  struct private_tveng1_device_info * p_info =
    (struct private_tveng1_device_info*) info;

  tveng_stop_everything(info);
  t_assert(info -> current_mode == TVENG_NO_CAPTURE);

  p_tveng1_timestamp_init(info);

  /* Make the pointer a invalid pointer */
  p_info -> mmaped_data = (char*) -1;

  /* 
     When this function is called, the desired capture format should
     have been set.
  */
  if (v4l_ioctl(info->fd, VIDIOCGMBUF, &(p_info->mmbuf)))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGMBUF", info);
      return -1;
    }

  t_assert (p_info->mmbuf.frames > 0);

  /* bttv 0.8.x wants PROT_WRITE although AFAIK we don't. */
  p_info->mmaped_data = (char *) mmap (0, p_info->mmbuf.size,
				       PROT_READ | PROT_WRITE,
				       MAP_SHARED, info->fd, 0);

  if (p_info->mmaped_data == (char *) -1)
    p_info->mmaped_data = (char *) mmap (0, p_info->mmbuf.size,
					 PROT_READ, MAP_SHARED,
					 info->fd, 0);

  if (p_info->mmaped_data == (char *) -1)
    {
      info->tveng_errno = errno;
      t_error("mmap()", info);
      return -1;
    }

  p_info -> queued = p_info -> dequeued = 0;

  info->current_mode = TVENG_CAPTURE_READ;  

  /* Queue first buffer */
  if (p_tveng1_queue(info) == -1)
    return -1;

  return 0;
}

/* Tries to stop capturing. -1 on error. */
static int
tveng1_stop_capturing(tveng_device_info * info)
{
  struct private_tveng1_device_info * p_info =
    (struct private_tveng1_device_info*) info;

  if (info -> current_mode == TVENG_NO_CAPTURE)
    {
      fprintf(stderr, 
	      "Warning: trying to stop capture with no capture active\n");
      return 0; /* Nothing to be done */
    }
  t_assert(info->current_mode == TVENG_CAPTURE_READ);

  /* Dequeue last buffer */
  p_tveng1_dequeue(NULL, info);

  if (p_info -> mmaped_data != ((char*)-1))
    if (munmap(p_info->mmaped_data, p_info->mmbuf.size) == -1)
      {
	info -> tveng_errno = errno;
	t_error("munmap()", info);
      }

  info->current_mode = TVENG_NO_CAPTURE;

  return 0;
}

static int p_tveng1_queue(tveng_device_info * info)
{
  struct video_mmap bm;
  struct private_tveng1_device_info * p_info =
    (struct private_tveng1_device_info*) info;

  t_assert(info != NULL);
  t_assert(info -> current_mode == TVENG_CAPTURE_READ);

  /* Fill in the mmaped_buffer struct */
  memset(&bm, 0, sizeof(struct video_mmap));

  switch(info->format.pixformat)
    {
    case TVENG_PIX_RGB555:
      bm.format = VIDEO_PALETTE_RGB555;
      break;
    case TVENG_PIX_RGB565:
      bm.format = VIDEO_PALETTE_RGB565;
      break;
    case TVENG_PIX_RGB24:
    case TVENG_PIX_BGR24:
      bm.format = VIDEO_PALETTE_RGB24;
      break;
    case TVENG_PIX_BGR32:
    case TVENG_PIX_RGB32:
      bm.format = VIDEO_PALETTE_RGB32;
      break;
    case TVENG_PIX_YUYV:
    case TVENG_PIX_UYVY:
      bm.format = VIDEO_PALETTE_YUV422;
      break;
    case TVENG_PIX_YUV420:
    case TVENG_PIX_YVU420:
      bm.format = VIDEO_PALETTE_YUV420P;
      break;
    default:
      info -> tveng_errno = -1;
      t_error_msg("switch()", "Cannot understand actual palette",
		  info);
      return -1;
    }
  bm.frame = (p_info -> queued) % p_info->mmbuf.frames;
  bm.width = info -> format.width;
  bm.height = info -> format.height;

  if (v4l_ioctl(info -> fd, VIDIOCMCAPTURE, &bm) == -1)
    {
      /* This comes from xawtv, it isn't in the V4L API */
      if (errno == EAGAIN)
	t_error_msg("VIDIOCMCAPTURE", 
		    "Grabber chip can't sync (no station tuned in?)",
		    info);
      else
	{
	  info -> tveng_errno = errno;
	  t_error("VIDIOCMCAPTURE", info);
	}
      return -1;
    }

  /* increase the queued index */
  p_info -> queued ++;

  return 0; /* Success */
}

/*
 *  From rte/mp1e since it now needs much more stable
 *  time stamps than v4l/gettimeofday can provide. 
 */
static inline double
p_tveng1_timestamp(struct private_tveng1_device_info *p_info)
{
  double now = current_time();
  double stamp;

  if (p_info->capture_time > 0) {
    double dt = now - p_info->capture_time;
    double ddt = p_info->frame_period_far - dt;

    if (fabs(p_info->frame_period_near)
	< p_info->frame_period_far * 1.5) {
      p_info->frame_period_near =
	(p_info->frame_period_near - dt) * 0.8 + dt;
      p_info->frame_period_far = ddt * 0.9999 + dt;
      stamp = p_info->capture_time += p_info->frame_period_far;
    } else {
      /* Frame dropping detected & confirmed */
      p_info->frame_period_near = p_info->frame_period_far;
      stamp = p_info->capture_time = now;
    }
  } else {
    /* First iteration */
    stamp = p_info->capture_time = now;
  }

  return stamp;
}

static void
p_tveng1_timestamp_init(tveng_device_info *info)
{
  struct private_tveng1_device_info *p_info =
    (struct private_tveng1_device_info *) info;
  double rate = info->num_standards ?
    info->standards[info->cur_standard].frame_rate : 25;

  p_info->capture_time = 0.0;
  p_info->frame_period_near = p_info->frame_period_far = 1.0 / rate;
}

static int p_tveng1_dequeue(tveng_image_data * where,
			    tveng_device_info * info)
{
  struct video_mmap bm;
  struct private_tveng1_device_info * p_info =
    (struct private_tveng1_device_info*) info;

  t_assert(info != NULL);
  t_assert(info -> current_mode == TVENG_CAPTURE_READ);

  if (p_info -> dequeued == p_info -> queued)
    return 0; /* All queued frames have been dequeued */

  memset(&bm, 0, sizeof(struct video_mmap));

  /* Fill in the mmaped_buffer struct */
  switch(info->format.pixformat)
    {
    case TVENG_PIX_RGB555:
      bm.format = VIDEO_PALETTE_RGB555;
      break;
    case TVENG_PIX_RGB565:
      bm.format = VIDEO_PALETTE_RGB565;
      break;
    case TVENG_PIX_RGB24:
    case TVENG_PIX_BGR24:
      bm.format = VIDEO_PALETTE_RGB24;
      break;
    case TVENG_PIX_BGR32:
    case TVENG_PIX_RGB32:
      bm.format = VIDEO_PALETTE_RGB32;
      break;
    case TVENG_PIX_YUYV:
    case TVENG_PIX_UYVY:
      bm.format = VIDEO_PALETTE_YUV422;
      break;
    case TVENG_PIX_YUV420:
    case TVENG_PIX_YVU420:
      bm.format = VIDEO_PALETTE_YUV420P;
      break;
    default:
      info -> tveng_errno = -1;
      t_error_msg("switch()", "Cannot understand actual palette",
		  info);
      return -1;
    }
  bm.frame = (p_info -> dequeued) % (p_info->mmbuf.frames);
  bm.width = info -> format.width;
  bm.height = info -> format.height;

  while (v4l_ioctl(info -> fd, VIDIOCSYNC, &(bm.frame)) == -1)
    {
      if (errno == EINTR)
	continue;

      info -> tveng_errno = errno;
      t_error("VIDIOCSYNC", info);
      return -1;
    }

  p_info->last_timestamp = p_tveng1_timestamp(p_info);

  /* Copy the mmaped data to the data struct, if it is not null */
  if (where)
    tveng_copy_frame (p_info-> mmaped_data +
		      p_info->mmbuf.offsets[bm.frame], where, info);

  /* increase the dequeued index */
  p_info -> dequeued ++;

  return 0;
}

/* 
   Reads a frame from the video device, storing the read data in
   the location pointed to by where.
   time: time to wait using select() in miliseconds
   info: pointer to the video device info structure
   This call was originally intended to wrap a single read() call, but
   since i cannot get it to work, now encapsulates the dqbuf/qbuf
   logic.
   Returns -1 on error, anything else on success
*/
static
int tveng1_read_frame(tveng_image_data *where, 
		      unsigned int time, tveng_device_info * info)
{
  struct itimerval iv;

  t_assert(info != NULL);

  if (info -> current_mode != TVENG_CAPTURE_READ)
    {
      info -> tveng_errno = -1;
      t_error_msg("check", "Current capture mode is not READ",
		  info);
      return -1;
    }

  /* This should be inmediate */
  if (p_tveng1_queue(info) == -1)
    return -1;

  /* Dequeue previously queued frame */
  /* Sets the timer to expire (SIGALARM) in the given time */
  iv.it_interval.tv_sec = iv.it_interval.tv_usec = iv.it_value.tv_sec
    = 0;
  iv.it_value.tv_usec = time;
  if (setitimer(ITIMER_REAL, &iv, NULL) == -1)
    {
      info->tveng_errno = errno;
      t_error("setitimer()", info);
      return -1;
    }

  if (p_tveng1_dequeue(where, info) == -1)
    return -1;

  /* Everything has been OK, return 0 (success) */
  return 0;
}

static double tveng1_get_timestamp(tveng_device_info * info)
{
  struct private_tveng1_device_info * p_info =
    (struct private_tveng1_device_info *) info;

  t_assert(info != NULL);
  if (info->current_mode != TVENG_CAPTURE_READ)
    return -1;

  return (p_info -> last_timestamp);
}

/* 
   Sets the capture buffer to an specific size. returns -1 on
   error. Remember to check the value of width and height since it can
   be different to the one requested. 
*/
static
int tveng1_set_capture_size(int width, int height, tveng_device_info * info)
{
  enum tveng_capture_mode current_mode;
  t_assert(info != NULL);
  t_assert(width > 0);
  t_assert(height > 0);

XX();

  current_mode = tveng_stop_everything(info);

  if (width < info->caps.minwidth)
    width = info->caps.minwidth;
  else if (width > info->caps.maxwidth)
    width = info->caps.maxwidth;
  if (height < info->caps.minheight)
    height = info->caps.minheight;
  else if (height > info->caps.maxheight)
    height = info->caps.maxheight;

  info -> format.width = width;
  info -> format.height = height;

XX();

  if (tveng1_set_capture_format(info) == -1)
    return -1;

  /* Restart capture again */
  return tveng_restart_everything(current_mode, info);
}

/* 
   Gets the actual size of the capture buffer in width and height.
   -1 on error
*/
static
int tveng1_get_capture_size(int *width, int *height, tveng_device_info * info)
{
  t_assert(info != NULL);
XX();

  if (tveng1_update_capture_format(info))
    return -1;

  if (width)
    *width = info->format.width;
  if (height)
    *height = info->format.height;

  return 0; /* Success */
}

/* XF86 Frame Buffer routines */
/*
  Returns 1 if the device attached to info suports previewing, 0 otherwise
*/
static int
tveng1_detect_preview (tveng_device_info * info)
{
  struct video_buffer buffer;
  
  t_assert(info != NULL);

  if ((info -> caps.flags & TVENG_CAPS_OVERLAY) == 0)
    {
      info -> tveng_errno = -1;
      t_error_msg("flags check",
       "The capability field says that there is no overlay", info);
      return 0;
    }

  /* Get the current framebuffer info */
  if (v4l_ioctl(info -> fd, VIDIOCGFBUF, &buffer))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGFBUF", info);
      return 0;
    }

  info->fb_info.base = buffer.base;
  info->fb_info.height = buffer.height;
  info->fb_info.width = buffer.width;
  info->fb_info.depth = buffer.depth;
  info->fb_info.bytesperline = buffer.bytesperline;

  if (!tveng_detect_XF86DGA(info))
    {
      info->tveng_errno = -1;
      t_error_msg("tveng_detect_XF86DGA",
		  "No DGA present, make sure you enable it in"
		  " /etc/X11/XF86Config.",
		  info);
      return 0;
    }

  return 1;
}

/*
  According to the V4L spec we should return a host order RGB32
  value. Using the pixel value directly would make much more sense,
  not to mention "host order RGB32" doesn't mean anything till you
  define what RGB32 means :-)
  Hope this works, i have no way of testing apart from feedback.
*/
static uint32_t calc_chroma (tveng_device_info * info)
{
  struct private_tveng1_device_info * p_info =
    (struct private_tveng1_device_info*) info;
  uint32_t r, g, b, pixel;

  r = p_info->r;
  g = p_info->g;
  b = p_info->b;

#if __BYTE_ORDER == __LITTLE_ENDIAN
  /* ARGB */
  pixel = (r<<16) + (g<<8) + b;
#elif __BYTE_ORDER == __BIG_ENDIAN
  /* ABGR or BGRA ??? Try with BGRA */
  pixel = (b<<24) + (g<<16) + (r<<8);
#else /* pdp endian */
  /* GBAR */
  pixel = (g<<24) + (b<<16) + r;
#endif

  return pixel;
}

/*
  Sets the preview window dimensions to the given window.
  Returns -1 on error, something else on success.
  Success doesn't mean that the requested dimensions are used, maybe
  they are different, check the returned fields to see if they are suitable
  info   : Device we are controlling
*/
static int
tveng1_set_preview_window(tveng_device_info * info)
{
  struct video_window v4l_window;
  struct video_clip * clips=NULL;
  int i;

  t_assert(info != NULL);
  t_assert(info-> window.clipcount >= 0);

  memset(&v4l_window, 0, sizeof(struct video_window));

  v4l_window.x = info->window.x;
  v4l_window.y = info->window.y;
  v4l_window.width = info->window.width;
  v4l_window.height = info->window.height;
  v4l_window.clipcount = info->window.clipcount;
  v4l_window.clips = NULL;
  v4l_window.chromakey = calc_chroma (info);
  if (v4l_window.clipcount)
    {
      clips = (struct video_clip*)malloc(v4l_window.clipcount* 
					 sizeof(struct video_clip));
      memset(clips, 0, v4l_window.clipcount*
	     sizeof(struct video_clip));
      if (!clips)
	{
	  info->tveng_errno = errno;
	  t_error("malloc", info);
	  return -1;
	}
      v4l_window.clips = clips;
      for (i=0;i<v4l_window.clipcount;i++)
	{
	  /* The clip seems to be always dword-aligned, reflect that */
	  v4l_window.clips[i].x = info->window.clips[i].x;
	  v4l_window.clips[i].y = info->window.clips[i].y;
	  v4l_window.clips[i].width = info->window.clips[i].width;
	  v4l_window.clips[i].height = info->window.clips[i].height;
	}
    }

  /* Up to the caller to call _on */
  tveng_set_preview_off(info);

  /* Set the new window */
  if (v4l_ioctl(info->fd, VIDIOCSWIN, &v4l_window))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCSWIN", info);
      if (clips)
	free(clips);
      return -1;
    }

  /* free allocated mem */
  if (clips)
    free(clips);

  /* Update the info struct */
  return (tveng1_get_preview_window(info));
}

/*
  Gets the current overlay window parameters.
  Returns -1 on error, and any other value on success.
  info   : The device to use
*/
static int
tveng1_get_preview_window(tveng_device_info * info)
{
  /* Updates the entire capture format, since in V4L there is no
     difference */
XX();
  return (tveng1_update_capture_format(info));
}

/* 
   Sets the previewing on/off.
   on : if 1, set preview on, if 0 off, other values are silently ignored
   info  : device to use for previewing
   Returns -1 on error, anything else on success
*/
static int
tveng1_set_preview (int on, tveng_device_info * info)
{
  int one = 1, zero = 0;
  t_assert(info != NULL);

  if ((on < 0) || (on > 1))
    return 0;

  if (v4l_ioctl(info->fd, VIDIOCCAPTURE, on ? &one : &zero))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCCAPTURE", info);
      return -1;
    }
  return 0;
}

/* 
   Sets up everything and starts previewing.
   Just call this function to start previewing, it takes care of
   (mostly) everything.
   Returns -1 on error.
*/
static int
tveng1_start_previewing (tveng_device_info * info,
			 x11_dga_parameters *dga)
{
  int width, height;

  if (!x11_dga_present (dga))
    return -1;

  tveng_stop_everything(info);

  t_assert(info -> current_mode == TVENG_NO_CAPTURE);

  if (!tveng1_detect_preview(info))
    /* We shouldn't be reaching this if the app is well programmed */
    t_assert_not_reached();

  width = info->caps.maxwidth;
  if (width > dga->width)
    width = dga->width;

  height = info->caps.maxheight;
  if (height > dga->height)
    height = dga->height;

  /* Center the window, dwidth is always >= width */
  info->window.x = (dga->width - width)/2;
  info->window.y = (dga->height - height)/2;
  info->window.width = width;
  info->window.height = height;
  info->window.clips = NULL;
  info->window.clipcount = 0;

  /* Set new capture dimensions */
  if (tveng1_set_preview_window(info) == -1)
    return -1;

  /* Center preview window (maybe the requested width and/or height)
     aren't valid */
  info->window.x = (dga->width - info->window.width)/2;
  info->window.y = (dga->height - info->window.height)/2;
  info->window.clipcount = 0;
  info->window.clips = NULL;
  if (tveng1_set_preview_window(info) == -1)
    return -1;

  /* Start preview */
  if (tveng1_set_preview(1, info) == -1)
    return -1;

  info -> current_mode = TVENG_CAPTURE_PREVIEW;
  return 0; /* Success */
}

/*
  Stops the fullscreen mode. Returns -1 on error
*/
static int
tveng1_stop_previewing(tveng_device_info * info)
{
  if (info -> current_mode == TVENG_NO_CAPTURE)
    {
      fprintf(stderr, 
	      "Warning: trying to stop preview with no capture active\n");
      return 0; /* Nothing to be done */
    }
  t_assert(info->current_mode == TVENG_CAPTURE_PREVIEW);

  /* No error checking */
  tveng1_set_preview(0, info);

  info -> current_mode = TVENG_NO_CAPTURE;
  return 0; /* Success */
}

static void
tveng1_set_chromakey		(uint32_t chroma, tveng_device_info *info)
{
  struct private_tveng1_device_info * p_info =
    (struct private_tveng1_device_info*) info;
  XColor color;
  Display *dpy = info->priv->display;

  color.pixel = chroma;
  XQueryColor (dpy, DefaultColormap(dpy, DefaultScreen(dpy)),
	       &color);

  p_info->chroma = chroma;
  p_info->r = color.red>>8;
  p_info->g = color.green>>8;
  p_info->b = color.blue>>8;

  /* Will be set in the next set_window call */
}

static int
tveng1_get_chromakey		(uint32_t *chroma, tveng_device_info *info)
{
  struct private_tveng1_device_info * p_info =
    (struct private_tveng1_device_info*) info;

  /* We aren't returning the chromakey currently used by the driver,
     but the one previously set. The reason for this is that it is
     unclear whether calc_chroma works correctly or not, and that
     color precision could be lost during the V4L->X conversion. In
     other words, this is prolly good enough. */
  *chroma = p_info->chroma;

  return 0;
}

static int
ov511_get_button_state		(tveng_device_info	*info)
{
  struct private_tveng1_device_info * p_info =
    (struct private_tveng1_device_info*) info;
  char button_state;

  if (p_info -> ogb_fd < 1)
    return -1; /* Unsupported feature */

  lseek(p_info -> ogb_fd, 0, SEEK_SET);
  if (read(p_info -> ogb_fd, &button_state, 1) < 1)
    return -1;

  return (button_state - '0');
}

static struct tveng_module_info tveng1_module_info = {
  .attach_device =		tveng1_attach_device,
  .describe_controller =	tveng1_describe_controller,
  .close_device =		tveng1_close_device,
  .get_inputs =			tveng1_get_inputs,
  .set_input =			tveng1_set_input,
  .get_standards =		tveng1_get_standards,
  .set_standard =		tveng1_set_standard,
  .update_capture_format =	tveng1_update_capture_format,
  .set_capture_format =		tveng1_set_capture_format,
  .update_control =		tveng1_update_control,
  .set_control =		tveng1_set_control,
  .tune_input =			tveng1_tune_input,
  .get_signal_strength =	tveng1_get_signal_strength,
  .get_tune =			tveng1_get_tune,
  .get_tuner_bounds =		tveng1_get_tuner_bounds,
  .start_capturing =		tveng1_start_capturing,
  .stop_capturing =		tveng1_stop_capturing,
  .read_frame =			tveng1_read_frame,
  .get_timestamp =		tveng1_get_timestamp,
  .set_capture_size =		tveng1_set_capture_size,
  .get_capture_size =		tveng1_get_capture_size,
  .detect_preview =		tveng1_detect_preview,
  .set_preview_window =		tveng1_set_preview_window,
  .get_preview_window =		tveng1_get_preview_window,
  .set_preview =		tveng1_set_preview,
  .start_previewing =		tveng1_start_previewing,
  .stop_previewing =		tveng1_stop_previewing,
  .set_chromakey =		tveng1_set_chromakey,
  .get_chromakey =		tveng1_get_chromakey,

  .ov511_get_button_state =	ov511_get_button_state,

  .private_size =		sizeof(struct private_tveng1_device_info)
};

/*
  Inits the V4L1 module, and fills in the given table.
*/
void tveng1_init_module(struct tveng_module_info *module_info)
{
  t_assert(module_info != NULL);

  memcpy(module_info, &tveng1_module_info,
	 sizeof(struct tveng_module_info)); 
}

#else /* !ENABLE_V4L */

#include "tveng1.h"

void tveng1_init_module(struct tveng_module_info *module_info)
{
  t_assert(module_info != NULL);

  memset(module_info, 0, sizeof(struct tveng_module_info));
}

#endif /* ENABLE_V4L */
