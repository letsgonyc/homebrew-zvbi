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

#include "../site_def.h"

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <X11/Xlib.h> /* We use some X calls */
#include <X11/Xutil.h>
#include <ctype.h>
#include <assert.h>
#include <dirent.h>

/* This undef's are to avoid a couple of header warnings */
#undef WNOHANG
#undef WUNTRACED
#include "tveng.h"
#include "tveng1.h" /* V4L specific headers */
#include "tveng2.h" /* V4L2 specific headers */
#include "tveng25.h" /* V4L2 2.5 specific headers */
#include "tvengxv.h" /* XVideo specific headers */
#include "tvengemu.h" /* Emulation device */
#include "tveng_private.h" /* private definitions */

#include "globals.h" /* XXX for vidmodes, dga_param */

//#include "../common/types.h"
#include "zmisc.h"

#define TVLOCK								\
({									\
  /* fprintf (stderr, "TVLOCK   in " __PRETTY_FUNCTION__ "\n"); */	\
  pthread_mutex_lock(&(info->priv->mutex));				\
})
#define UNTVLOCK							\
({									\
  /* fprintf (stderr, "UNTVLOCK in " __PRETTY_FUNCTION__ "\n"); */	\
  pthread_mutex_unlock(&(info->priv->mutex));				\
})

#define RETURN_UNTVLOCK(X)						\
do {									\
  __typeof__(X) _unlocked_result = X;					\
  UNTVLOCK;								\
  return _unlocked_result;						\
} while (0)

#define TVUNSUPPORTED do { \
  /* function not supported by the module */ \
 info->tveng_errno = -1; \
 t_error_msg("module", \
	     "function not supported by the module", info); \
} while (0)

/* for xv */
struct control {
  tv_control		pub;
  tveng_device_info *	info;
  Atom			atom;
  tv_mixer_line *	line;
  tv_callback_node *	line_cb;
};

#define C(l) PARENT (l, struct control, pub)

typedef void (*tveng_controller)(struct tveng_module_info *info);
static tveng_controller tveng_controllers[] = {
  tvengxv_init_module,
  tveng25_init_module,
  tveng2_init_module,
  tveng1_init_module,
  tvengemu_init_module
};

/*
static void
deref_callback			(void *			object,
				 void *			user_data)
{
	*((void **) user_data) = NULL;
}
*/

/* Initializes a tveng_device_info object */
tveng_device_info * tveng_device_info_new(Display * display, int bpp)
{
  size_t needed_mem = 0;
  tveng_device_info * new_object;
  struct tveng_module_info module_info;
  pthread_mutexattr_t attr;
  int i;

  /* Get the needed mem for the controllers */
  for (i=0; i<(sizeof(tveng_controllers)/sizeof(tveng_controller));
       i++)
    {
      tveng_controllers[i](&module_info);
      needed_mem = MAX(needed_mem, (size_t) module_info.private_size);
    }

  t_assert(needed_mem > 0);

  new_object = (tveng_device_info*) calloc(1, needed_mem);

  if (!new_object)
    return NULL;

  /* fill the struct with 0's */
  memset(new_object, 0, needed_mem);

  new_object -> priv = malloc(sizeof(struct tveng_private));

  if (!new_object->priv)
    {
      free(new_object);
      return NULL;
    }

  memset(new_object->priv, 0, sizeof(struct tveng_private));

  /* Allocate some space for the error string */
  new_object -> error = (char*) malloc(256);

  if (!new_object->error)
    {
      free(new_object->priv);
      free(new_object);
      perror("malloc");
      return NULL;
    }

  new_object->priv->display = display;
  new_object->priv->bpp = bpp;

  new_object->priv->zapping_setup_fb_verbosity = 0; /* No output by
							  default */
#ifdef USE_XV
  new_object->priv->port = None;
  new_object->priv->filter = None;
  new_object->priv->colorkey = None;
  new_object->priv->double_buffer = None;
#endif

  x11_vidmode_clear_state (&new_object->priv->old_mode);

  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&(new_object->priv->mutex), &attr);
  pthread_mutexattr_destroy(&attr);

  new_object->current_controller = TVENG_CONTROLLER_NONE;

  /* return the allocated memory */
  return (new_object);
}

/* destroys a tveng_device_info object (and closes it if neccesary) */
void tveng_device_info_destroy(tveng_device_info * info)
{
  t_assert(info != NULL);

  if (info -> fd > 0)
    tveng_close_device(info);

  if (info -> error)
    free(info -> error);

  pthread_mutex_destroy(&(info->priv->mutex));

  free(info->priv);
  free(info);
}


/*
  Associates the given tveng_device_info with the given video
  device. On error it returns -1 and sets info->tveng_errno, info->error to
  the correct values.
  device_file: The file used to access the video device (usually
  /dev/video)
  attach_mode: Specifies the mode to open the device file
  depth: The color depth the capture will be in, -1 means let tveng
  decide based on the current private->display depth.
  info: The structure to be associated with the device
*/
int tveng_attach_device(const char* device_file,
			enum tveng_attach_mode attach_mode,
			tveng_device_info * info)
{
  int i, j;
  char *long_str, *short_str, *sign = NULL;
  tv_control *tc;
  int num_controls;

  t_assert(device_file != NULL);
  t_assert(info != NULL);

  TVLOCK;

  if (info -> fd) /* If the device is already attached, detach it */
    tveng_close_device(info);

  info -> current_controller = TVENG_CONTROLLER_NONE;

  /*
    Check that the current private->display depth is one of the supported ones
  */
  info->priv->current_bpp = tveng_get_display_depth(info);
  switch (info->priv->current_bpp)
    {
    case 15:
    case 16:
    case 24:
    case 32:
      break;
    default:
      info -> tveng_errno = -1;
      t_error_msg("switch()",
		  "The current display depth isn't supported by TVeng",
		  info);
      UNTVLOCK;
      return -1;
    }

  for (i=0; i<(sizeof(tveng_controllers)/sizeof(tveng_controller));
       i++)
    {
      info -> fd = 0;
      tveng_controllers[i](&(info->priv->module));
      if (!info->priv->module.attach_device)
	continue;
      if (-1 != info->priv->module.attach_device(device_file, attach_mode,
						    info))
	goto success;
    }

  /* Error */
  info->tveng_errno = -1;
  t_error_msg("check()",
	      "The device cannot be attached to any controller",
	      info);
  memset(&(info->priv->module), 0, sizeof(info->priv->module));
  UNTVLOCK;
  return -1;

 success:
  {
    info->priv->control_mute = NULL;
    info->audio_mutable = 0;

    /* Add mixer controls */
    /* XXX the mixer_line should be property of a virtual
       device, but until we're there... */
    if (mixer && mixer_line)
      tveng_attach_mixer_line (info, mixer, mixer_line);

    num_controls = 0;

    for (tc = info->controls; tc; tc = tc->next) {
      if (tc->id == TV_CONTROL_ID_MUTE
	  && (!tc->ignore)
	  && info->priv->control_mute == NULL) {
	//	if (tv_control_callback_add (tc, NULL, (void *) deref_callback,
	//			     &info->priv->control_mute))
	  {
	    info->priv->control_mute = tc;
	    info->audio_mutable = 1; /* preliminary */
	  }
      }

      num_controls++;
    }
  }

  if (asprintf (&sign, "%s - %d %d - %d %d %d", info->caps.name,
		info->num_inputs, num_controls,
		info->caps.flags, info->caps.maxwidth,
		info->caps.maxheight) == -1)
    {
      t_error ("asprintf", info);
      tveng_close_device (info);
      memset(&(info->priv->module), 0, sizeof(info->priv->module));
      UNTVLOCK;
      return -1;
    }
  info->signature = tveng_build_hash (sign);
  free (sign);

  if (info->debug_level>0)
    {
      fprintf(stderr, "[TVeng] - Info about the video device\n");
      fprintf(stderr, "-------------------------------------\n");
      tveng_describe_controller(&short_str, &long_str, info);
      fprintf(stderr, "Device: %s [%s - %s]\n", info->file_name,
	      short_str, long_str);
      fprintf(stderr, "Device signature: %x\n", info->signature);
      fprintf(stderr, "Detected framebuffer depth: %d\n",
	      tveng_get_display_depth(info));
      fprintf(stderr, "Current capture format:\n");
      fprintf(stderr, "  Dimensions: %dx%d  BytesPerLine: %d  Depth: %d "
	      "Size: %d K\n", info->format.width,
	      info->format.height, info->format.bytesperline,
	      info->format.depth, info->format.sizeimage/1024);
      fprintf(stderr, "Current overlay window struct:\n");
      fprintf(stderr, "  Coords: %dx%d-%dx%d\n",
	      info->window.x, info->window.y, info->window.width,
	      info->window.height);
      fprintf(stderr, "Detected standards:\n");
      for (i=0;i<info->num_standards;i++)
	fprintf(stderr, "  %d) [%s] ID: %d 0x%llx Size: %dx%d Hash: %x\n", i,
		info->standards[i].name, info->standards[i].id,
		info->standards[i].stdid,
		info->standards[i].width, info->standards[i].height,
		info->standards[i].hash);
      fprintf(stderr, "Detected inputs:\n");
      for (i=0;i<info->num_inputs;i++)
	{
	  fprintf(stderr, "  %d) [%s] ID: %d Hash: %x\n", i,
		  info->inputs[i].name, info->inputs[i].id,
		  info->inputs[i].hash);
	  fprintf(stderr, "      Type: %s  Tuners: %d  Flags: 0x%x\n",
		  (info->inputs[i].type == TVENG_INPUT_TYPE_TV) ? _("TV")
		  : _("Camera"), info->inputs[i].tuners,
		  info->inputs[i].flags);
	}
      fprintf(stderr, "Available controls:\n");
      for (tc = info->controls; tc; tc = tc->next)
	{
	  fprintf(stderr, "  %d) [%s] ID: %d  Range: (%d, %d)  Value: %d ",
		  i, tc->label, tc->id,
		  tc->minimum, tc->maximum,
		  tc->value);
	  switch (tc->type)
	    {
	    case TV_CONTROL_TYPE_INTEGER:
	      fprintf(stderr, " <Slider>\n");
	      break;
	    case TV_CONTROL_TYPE_BOOLEAN:
	      fprintf(stderr, " <Checkbox>\n");
	      break;
	    case TV_CONTROL_TYPE_CHOICE:
	      fprintf(stderr, " <Menu>\n");
	      for (j=0; tc->menu[j]; j++)
		fprintf(stderr, "    %d.%d) [%s] <Menu entry>\n", i, j,
			tc->menu[j]);
	      break;
	    case TV_CONTROL_TYPE_ACTION:
	      fprintf(stderr, " <Button>\n");
	      break;
	    case TV_CONTROL_TYPE_COLOR:
	      fprintf(stderr, " <Color>\n");
	      break;
	    default:
	      fprintf(stderr, " <Unknown type>\n");
	      break;
	    }
	}
    }

  UNTVLOCK;
  return info->fd;
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
void
tveng_describe_controller(char ** short_str, char ** long_str,
			  tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->priv->module.describe_controller)
    info->priv->module.describe_controller(short_str, long_str,
					      info);
  else /* function not supported by the module */
    {
      if (short_str)
	*short_str = "UNKNOWN";
      if (long_str)
	*long_str = "No description provided";
    }

  UNTVLOCK;
}

/* Closes a device opened with tveng_init_device */
void tveng_close_device(tveng_device_info * info)
{
  t_assert(info != NULL);

  if (info->current_controller == TVENG_CONTROLLER_NONE)
    return; /* nothing to be done */

  TVLOCK;

  tveng_stop_everything(info);

  /* remove mixer controls */
  tveng_attach_mixer_line (info, NULL, NULL);

  if (info->priv->module.close_device)
    info->priv->module.close_device(info);

  info->priv->control_mute = NULL;

  UNTVLOCK;
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
int tveng_get_inputs(tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->priv->module.get_inputs)
    RETURN_UNTVLOCK(info->priv->module.get_inputs(info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/* Returns a newly allocated copy of the string, normalized */
static char* normalize(const char *string)
{
  int i = 0;
  const char *strptr=string;
  char *result;

  t_assert(string != NULL);

  result = strdup(string);

  t_assert(result != NULL);

  while (*strptr != 0) {
    if (*strptr == '_' || *strptr == '-' || *strptr == ' ') {
      strptr++;
      continue;
    }
    result[i] = tolower(*strptr);

    strptr++;
    i++;
  }
  result[i] = 0;

  return result;
}

/* nomalize and compare */
static int tveng_normstrcmp (const char * in1, const char * in2)
{
  char *s1 = normalize(in1);
  char *s2 = normalize(in2);

  t_assert(in1 != NULL);
  t_assert(in2 != NULL);

  /* Compare the strings */
  if (!strcmp(s1, s2)) {
    free(s1);
    free(s2);
    return 1;
  } else {
    free(s1);
    free(s2);
    return 0;
  }
}

/* build hash for the given string, normalized */
int
tveng_build_hash(const char *string)
{
  char *norm = normalize(string);
  int i;
  int result=0;

  for (i = 0; i<strlen(norm); i++)
    result += ((result+171)*((int)norm[i]) & ~(norm[i]>>4));

  free(norm);

  return result;
}

/*
  Sets the current input for the capture
*/
int tveng_set_input(struct tveng_enum_input * input,
		    tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(input != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->priv->module.set_input)
    RETURN_UNTVLOCK(info->priv->module.set_input(input, info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/*
  Sets the input named name as the active input. -1 on error.
*/
int
tveng_set_input_by_name(const char * input_name,
			tveng_device_info * info)
{
  int i;

  t_assert(input_name != NULL);
  t_assert(info != NULL);

  TVLOCK;

  for (i = 0; i < info->num_inputs; i++)
    if (tveng_normstrcmp(info->inputs[i].name, input_name))
      RETURN_UNTVLOCK(tveng_set_input(&(info->inputs[i]), info));

  info->tveng_errno = -1;
  t_error_msg("finding",
	      "Input %s doesn't appear to exist", info, input_name);

  UNTVLOCK;
  return -1; /* String not found */
}

/*
  Sets the active input by its id (may not be the same as its array
  index, but it should be). -1 on error
*/
int
tveng_set_input_by_id(int id, tveng_device_info * info)
{
  int i;

  t_assert(info != NULL);

  TVLOCK;

  for (i = 0; i < info->num_inputs; i++)
    if (info->inputs[i].id == id)
      RETURN_UNTVLOCK(tveng_set_input(&(info->inputs[i]), info));

  info->tveng_errno = -1;
  t_error_msg("finding",
	      "Input number %d doesn't appear to exist", info, id);

  UNTVLOCK;
  return -1; /* String not found */
}

/*
  Sets the active input by its index in inputs. -1 on error
*/
int
tveng_set_input_by_index(int index, tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(index > -1);

  TVLOCK;

  if (info->num_inputs)
    {
      t_assert(index < info -> num_inputs);
      RETURN_UNTVLOCK((tveng_set_input(&(info -> inputs[index]), info)));
    }

  UNTVLOCK;
  return 0;
}

/**
 * Finds the input with the given hash, or NULL.
 * The hash is based on the input normalized name.
 */
struct tveng_enum_input *
tveng_find_input_by_hash(int hash, const tveng_device_info *info)
{
  int i;

  t_assert(info != NULL);

  for (i=0; i<info->num_inputs; i++)
    if (info->inputs[i].hash == hash)
      return &(info->inputs[i]);

  return  NULL;
}

/*
  Queries the device about its standards. Fills in info as appropiate
  and returns the number of standards in the device. This is for the
  first tuner in the current input, should be enough since most (all)
  inputs have 1 or less tuners.
*/
int tveng_get_standards(tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->priv->module.get_standards)
    RETURN_UNTVLOCK(info->priv->module.get_standards(info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/*
  Sets the current standard for the capture. standard is the name for
  the desired standard. updates cur_standard
*/
int tveng_set_standard(struct tveng_enumstd * std, tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(std != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->priv->module.set_standard)
    RETURN_UNTVLOCK(info->priv->module.set_standard(std, info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/*
  Sets the standard by name. -1 on error
*/
int
tveng_set_standard_by_name(const char * name, tveng_device_info * info)
{
  int i;

  t_assert(info != NULL);

  TVLOCK;

  for (i = 0; i < info->num_standards; i++)
    if (tveng_normstrcmp(name, info->standards[i].name))
      RETURN_UNTVLOCK(tveng_set_standard(&(info->standards[i]), info));

  info->tveng_errno = -1;
  t_error_msg("finding",
	      "Standard %s doesn't appear to exist", info, name);

  UNTVLOCK;
  return -1; /* String not found */  
}

/*
  Sets the standard by id.
*/
int
tveng_set_standard_by_id(int id, tveng_device_info * info)
{
  int i;

  t_assert(info != NULL);

  TVLOCK;

  for (i = 0; i < info->num_standards; i++)
    if (info->standards[i].id == id)
      RETURN_UNTVLOCK(tveng_set_standard(&(info->standards[i]), info));

  info->tveng_errno = -1;
  t_error_msg("finding",
	      "Standard number %d doesn't appear to exist", info, id);

  UNTVLOCK;
  return -1; /* id not found */
}

/*
  Sets the standard by index. -1 on error
*/
int
tveng_set_standard_by_index(int index, tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(index > -1);

  TVLOCK;

  if (info->num_standards)
    {
      t_assert(index < info->num_standards);
      RETURN_UNTVLOCK(tveng_set_standard(&(info->standards[index]), info));
    }

  UNTVLOCK;
  return 0;
}

/**
 * Finds the standard with the given hash, or NULL.
 * The hash is based on the standard normalized name.
 */
struct tveng_enumstd *
tveng_find_standard_by_hash(int hash, const tveng_device_info *info)
{
  int i;

  t_assert(info != NULL);

  for (i=0; i<info->num_standards; i++)
    if (info->standards[i].hash == hash)
      return &(info->standards[i]);

  return  NULL;
}

/* Updates the current capture format info. -1 if failed */
int
tveng_update_capture_format(tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->priv->module.update_capture_format)
    RETURN_UNTVLOCK(info->priv->module.update_capture_format(info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/* -1 if failed. Sets the pixformat and fills in info -> pix_format
   with the correct values  */
int
tveng_set_capture_format(tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;
XX();
  if (info->format.height < info->caps.minheight)
    info->format.height = info->caps.minheight;
  if (info->format.height > info->caps.maxheight)
    info->format.height = info->caps.maxheight;
  if (info->format.width < info->caps.minwidth)
    info->format.width = info->caps.minwidth;
  if (info->format.width > info->caps.maxwidth)
    info->format.width = info->caps.maxwidth;
XX();
  /* force dword aligning of width if requested */
  if (info->priv->dword_align)
    info->format.width = (info->format.width+3) & ~3;

  /* force dword-aligning of data */
  switch (info->format.pixformat)
    {
    case TVENG_PIX_RGB565:
    case TVENG_PIX_RGB555:
    case TVENG_PIX_YUYV:
    case TVENG_PIX_UYVY:
      info->format.width = (info->format.width+1) & ~1;
      break;
    case TVENG_PIX_YUV420:
    case TVENG_PIX_YVU420:
    case TVENG_PIX_RGB24:
    case TVENG_PIX_BGR24:
      info->format.width = (info->format.width+3) & ~3;
      break;
    default:
      break;
    }
XX();
  if (info->priv->module.set_capture_format)
    RETURN_UNTVLOCK(info->priv->module.set_capture_format(info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/*
 *  Controls
 */

tv_callback_node *
tv_control_callback_add		(tv_control *		control,
				 void			(* notify)(tv_control *, void *user_data),
				 void			(* destroy)(tv_control *, void *user_data),
				 void *			user_data)
{
	t_assert (control != NULL);

	return tv_callback_add (&control->_callback,
				(void *) notify,
				(void *) destroy,
				user_data);
}

static int
update_xv_control (tveng_device_info * info, struct control *c)
{
  int value;

  if (c->pub.id == TV_CONTROL_ID_VOLUME
      || c->pub.id == TV_CONTROL_ID_MUTE)
    {
      tv_mixer_line *line;

      line = c->line;
      t_assert (line != NULL);

      if (!tv_mixer_line_update (line))
	return -1;

      if (c->pub.id == TV_CONTROL_ID_VOLUME)
	value = (line->volume[0] + line->volume[1] + 1) >> 1;
      else
	value = !!(line->muted);
    }

#ifdef USE_XV
  else if ((c->atom == info->priv->filter) &&
	   (info->priv->port != None))
    {
      XvGetPortAttribute(info->priv->display,
			 info->priv->port,
			 info->priv->filter,
			 &value);
    }
  else if ((c->atom == info->priv->double_buffer) &&
	   (info->priv->port != None))
    {
      XvGetPortAttribute(info->priv->display,
			 info->priv->port,
			 info->priv->double_buffer,
			 &value);
    }
  else if ((c->atom == info->priv->colorkey) &&
	   (info->priv->port != None))
    {
      int r,g,b, val;
      int rm=0xff, gm=0xff, bm=0xff, rs=16, gs=8, bs=0; /* masks, shifts */

      XvGetPortAttribute(info->priv->display,
			 info->priv->port,
			 info->priv->colorkey,
			 &(val));

      /* Adjust colorkey to the current pixformat */
      switch (info->priv->current_bpp)
	{
	case 15:
	  rm = gm = bm = 0xf8;
	  rs = 7; gs = 2; bs = -3;
	  break;
	case 16:
	  rm = bm = 0xf8; gm = 0xfc;
	  rs = 8; gs = 3; bs = -3;
	  break;
	default:
	  break;
	}
      if (rs > 0)
	r = val >> rs;
      else
	r = val << -rs;
      r &= rm;
      if (gs > 0)
	g = val >> gs;
      else
	g = val << -gs;
      g &= gm;
      if (bs > 0)
	b = val >> bs;
      else
	b = val << -bs;
      b &= bm;
      value = (r<<16)+(g<<8)+b;
    }
#endif
  else
    {
      return 0;
    }

  if (c->pub.value != value)
    {
      c->pub.value = value;
      tv_callback_notify (&c->pub, c->pub._callback);
    }

  return 0;
}

int
tveng_update_control(tv_control *control,
		     tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);
  t_assert(control != NULL);

  TVLOCK;

  if (info->priv->quiet)
    if (control->id == TV_CONTROL_ID_MUTE
	|| control->id == TV_CONTROL_ID_VOLUME)
      RETURN_UNTVLOCK (0);
  
  if (control->_device == NULL /* TVENG_CONTROLLER_MOTHER */)
    RETURN_UNTVLOCK (update_xv_control (info, C(control)));

  if (info->priv->module.update_control)
    RETURN_UNTVLOCK(info->priv->module.update_control (info, control));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

int
tveng_update_controls(tveng_device_info * info)
{
  tv_control *tc;

  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  /* FIXME quiet */

  /* Update the controls we maintain */
  for (tc = info->controls; tc; tc = tc->next)
    if (tc->_device == NULL /* TVENG_CONTROLLER_MOTHER */)
      update_xv_control (info, C(tc));

  if (info->priv->module.update_control)
    RETURN_UNTVLOCK(info->priv->module.update_control (info, NULL) ? 0 : -1);

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/*
 *  When setting the mixer volume we must ensure the
 *  video device is unmuted and volume at maximum.
 */
static void
reset_video_volume		(tveng_device_info *	info,
				 guint			mute)
{
  tv_control *tc;

  for (tc = info->controls; tc; tc = tc->next)
    {
      if (tc->_device != NULL)
	switch (tc->id)
	  {
	  case TV_CONTROL_ID_VOLUME:
	    if (info->priv->module.set_control)
	      /* Error ignored */
	      info->priv->module.set_control (info, tc, tc->maximum);
	    break;

	  case TV_CONTROL_ID_MUTE:
	    if (info->priv->module.set_control)
	      /* Error ignored */
	      info->priv->module.set_control (info, tc, mute);
	    break;

	  default:
	    break;
	  }
    }
} 

static int
set_control			(struct control * c, int value,
				 tv_bool rvv,
				 tveng_device_info * info)
{
  int r = 0;

  if (value < c->pub.minimum)
    value = c->pub.minimum;
  else if (value > c->pub.maximum)
    value = c->pub.maximum;

  if (info->priv->quiet)
    if (c->pub.id == TV_CONTROL_ID_VOLUME
	|| c->pub.id == TV_CONTROL_ID_MUTE)
      goto set_and_notify;

  if (c->pub._device == NULL /* TVENG_CONTROLLER_MOTHER */)
    {
      if (c->pub.id == TV_CONTROL_ID_VOLUME)
	{
	  t_assert (c->line != NULL);
	  r = tv_mixer_line_set_volume (c->line, value, value) ? 0 : -1;
	  if (rvv)
	    reset_video_volume (info, 0);
	  value = (c->line->volume[0] + c->line->volume[1] + 1) >> 1;
	}
      else if (c->pub.id == TV_CONTROL_ID_MUTE)
	{
	  t_assert (c->line != NULL);
	  r = tv_mixer_line_set_mute (c->line, value) ? 0 : -1;
	  if (!value && rvv)
	    reset_video_volume (info, 0);
	  value = c->line->muted;
	}
#ifdef USE_XV
      else if ((c->atom == info->priv->filter) &&
	  (info->priv->port != None))
	{
	  XvSetPortAttribute(info->priv->display,
			     info->priv->port,
			     info->priv->filter,
			     value);
	}
      else if ((c->atom == info->priv->double_buffer) &&
	       (info->priv->port != None))
	{
	  XvSetPortAttribute(info->priv->display,
			     info->priv->port,
			     info->priv->double_buffer,
			     value);
	}
      else if ((c->atom == info->priv->colorkey) &&
	       (info->priv->port != None))
	{
	  int r, g, b;
	  int rm=0xff, gm=0xff, bm=0xff, rs=16, gs=8, bs=0; /* masks, shifts */
	  /* Adjust colorkey to the current pixformat */
	  switch (info->priv->current_bpp)
	    {
	    case 15:
	      rm = gm = bm = 0xf8;
	      rs = 7; gs = 2; bs = -3;
	      break;
	    case 16:
	      rm = bm = 0xf8; gm = 0xfc;
	      rs = 8; gs = 3; bs = -3;
	      break;
	    default:
	      break;
	    }
	  r = (value>>16)&rm;
	  if (rs > 0)
	    r <<= rs;
	  else
	    r >>= -rs;
	  g = (value>>8)&gm;
	  if (gs > 0)
	    g <<= gs;
	  else
	    g >>= -gs;
	  b = value&bm;
	  if (bs > 0)
	    b <<= bs;
	  else
	    b >>= -bs;
	  value = r+g+b;
	  XvSetPortAttribute(info->priv->display,
			     info->priv->port,
			     info->priv->colorkey,
			     value);
	}
      else
#endif
	{
	  t_assert (0);
	  return -1;
	}

    set_and_notify:
      if (c->pub.value != value)
	{
	  c->pub.value = value;
	  tv_callback_notify (&c->pub, c->pub._callback);
	}

      return r;
    }

  info = c->pub._device; /* XXX */

  if (info->priv->module.set_control)
    return info->priv->module.set_control (info, &c->pub, value) ? 0 : -1;

  TVUNSUPPORTED;

  return -1;
}

/*
  Sets the value for a specific control. The given value will be
  clipped between min and max values. Returns -1 on error
*/
int
tveng_set_control(tv_control * control, int value,
		  tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(control != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;
  RETURN_UNTVLOCK (set_control (C(control), value, TRUE, info));
}

/*
  Gets the value of a control, given its name. Returns -1 on
  error. The comparison is performed disregarding the case. The value
  read is stored in cur_value.
*/
int
tveng_get_control_by_name(const char * control_name,
			  int * cur_value,
			  tveng_device_info * info)
{
  int value;
  tv_control *tc;

  t_assert(info != NULL);
  t_assert(info->controls != NULL);
  t_assert(control_name != NULL);

  TVLOCK;

  /* Update the controls (their values) */
  if (tveng_update_controls(info) == -1)
    RETURN_UNTVLOCK(-1);

  /* iterate through the info struct to find the control */
  for (tc = info->controls; tc; tc = tc->next)
    if (!strcasecmp(tc->label,control_name))
      /* we found it */
      {
	value = tc->value;
	t_assert(value <= tc->maximum);
	t_assert(value >= tc->minimum);
	if (cur_value)
	  *cur_value = value;
	UNTVLOCK;
	return 0; /* Success */
      }

  /* if we reach this, we haven't found the control */
  info->tveng_errno = -1;
  t_error_msg("finding",
	      "Cannot find control \"%s\" in the list of controls",
	      info, control_name);
  UNTVLOCK;
  return -1;
}

/*
  Sets the value of a control, given its name. Returns -1 on
  error. The comparison is performed disregarding the case.
  new_value holds the new value given to the control, and it is
  clipped as neccessary.
*/
int
tveng_set_control_by_name(const char * control_name,
			  int new_value,
			  tveng_device_info * info)
{
  tv_control *tc;

  t_assert(info != NULL);
  t_assert(info->controls != NULL);

  TVLOCK;

  for (tc = info->controls; tc; tc = tc->next)
    if (!strcasecmp(tc->label,control_name))
      /* we found it */
      RETURN_UNTVLOCK((tveng_set_control(tc, new_value, info)));

  /* if we reach this, we haven't found the control */
  info->tveng_errno = -1;
  t_error_msg("finding",
	   "Cannot find control \"%s\" in the list of controls",
	   info, control_name);
  UNTVLOCK;
  return -1;
}

/*
  Gets the value of a control, given its control id. -1 on error (or
  cid not found). The result is stored in cur_value.
*/
#if 0
int
tveng_get_control_by_id(int cid, int * cur_value,
			tveng_device_info * info)
{
  t_assert (0);


  int i;
  int value;
  tv_dev_control *tc;

  t_assert(info != NULL);
  t_assert(info -> _controls != NULL);

  TVLOCK;

  /* Update the controls (their values) */
  if (tveng_update_controls(info) == -1)
    RETURN_UNTVLOCK(-1);

  for (tc = info->_controls; tc; tc = tc->next)
    if (tc->id == cid)
      /* we found it */
      {
	value = tc->cur_value;
	t_assert(value <= tc->max);
	t_assert(value >= tc->min);
	if (cur_value)
	  *cur_value = value;
	UNTVLOCK;
	return 0; /* Success */
      }

  /* if we reach this, we haven't found the control */
  info->tveng_errno = -1;
  t_error_msg("finding",
	      "Cannot find control %d in the list of controls",
	      info, cid);
  UNTVLOCK;

  return -1;
}
#endif
/*
  Sets a control by its id. Returns -1 on error
*/
#if 0
int tveng_set_control_by_id(int cid, int new_value,
			    tveng_device_info * info)
{
  t_assert (0);


  int i;
  tv_dev_control *tc;

  t_assert(info != NULL);
  t_assert(info -> _controls != NULL);

  TVLOCK;

  for (tc = info->_controls; tc; tc = tc->next)
    if (tc->id == cid)
      /* we found it */
      RETURN_UNTVLOCK(tveng_set_control(tc, new_value, info));

  /* if we reach this, we haven't found the control */
  info->tveng_errno = -1;
  t_error_msg("finding",
	      "Cannot find control %d in the list of controls",
	      info, cid);
  UNTVLOCK;

  return -1;
}
#endif

/*
 *  AUDIO ROUTINES
 */

#ifndef TVENG_MIXER_VOLUME_DEBUG
#define TVENG_MIXER_VOLUME_DEBUG 0
#endif

/*
 *  This is a mixer control. Notify when the underlying
 *  mixer line changes.
 */
static void
mixer_line_notify_cb		(tv_mixer_line *	line,
				 void *			user_data)
{
  struct control *c = user_data;
  tv_control *tc;
  int value;

  t_assert (line == c->line);

  tc = &c->pub;

  if (tc->id == TV_CONTROL_ID_VOLUME)
    {
      tc->minimum = line->minimum;
      tc->maximum = line->maximum;
      tc->step = line->step;
      tc->reset = line->reset;

      value = (line->volume[0] + line->volume[1] + 1) >> 1;
    }
  else /* TV_CONTROL_ID_MUTE */
    {
      tc->reset = 0;
      tc->minimum = 0;
      tc->maximum = 1;
      tc->step = 1;

      value = line->muted;
    }

  if (!c->info->priv->quiet)
    {
      tc->value = value;
      tv_callback_notify (tc, tc->_callback);
    }
}

/*
 *  This is a mixer control. When the underlying mixer line
 *  disappears, remove the control and make the corresponding
 *  video device control visible again.
 *
 *  XXX c->info->priv->quiet?
 */
static void
mixer_line_destroy_cb		(tv_mixer_line *	line,
				 void *			user_data)
{
  struct control *c = user_data;
  tv_control *tc, **tcp;
  tv_control_id id;

  t_assert (line == c->line);

  id = c->pub.id;

  if (id == TV_CONTROL_ID_MUTE)
    {
      c->info->priv->control_mute = NULL;
      c->info->audio_mutable = 0; /* preliminary */
    }

  for (tcp = &c->info->controls; (tc = *tcp);)
    {
      if (tc == &c->pub)
	{
	  tv_callback_destroy (tc, &tc->_callback);
	  *tcp = tc->next;
	  free (tc);
	  continue;
	}
      else if (tc->id == id)
	{
	  tc->ignore = FALSE;

	  if (id == TV_CONTROL_ID_MUTE)
	    {
	      c->info->priv->control_mute = tc;
	      c->info->audio_mutable = 1; /* preliminary */
	    }
	}

      tcp = &tc->next;
    }
}

/*
 *  When line != NULL this adds a mixer control, hiding the
 *  corresponding video device (volume or mute) control.
 *  When line == NULL the old state is restored.
 */
static void
mixer_replace			(tveng_device_info *	info,
				 tv_mixer_line *	line,
				 tv_control_id		id)
{
  tv_control *tc, **tcp, *orig;
  tv_bool done = FALSE;
  struct control *c;

  orig = NULL;

  for (tcp = &info->controls; (tc = *tcp);)
    {
      if (tc->id == id)
	{
	  if (tc->_device == NULL)
	    {
	      struct control *c = PARENT (tc, struct control, pub);

	      if (line)
		{
		  /* Replace mixer line */

		  tv_callback_remove (c->line_cb);

		  c->line = line;

		  c->line_cb = tv_mixer_line_callback_add
		    (line, mixer_line_notify_cb,
		     mixer_line_destroy_cb, c);
		  t_assert (c->line_cb != NULL); /* XXX */

		  mixer_line_notify_cb (line, c);

		  done = TRUE;
		}
	      else
		{
		  /* Remove mixer line & control */

		  tv_callback_destroy (tc, &tc->_callback);
		  tv_callback_remove (c->line_cb);

		  *tcp = tc->next;
		  free (tc);

		  done = TRUE;
		  continue;
		}
	    }
	  else /* video device control */
	    {
	      if (!TVENG_MIXER_VOLUME_DEBUG)
		tc->ignore = (line != NULL);

	      orig = tc;
	    }
	}

      tcp = &tc->next;
    }

  if (done || !line)
    return;

  /* Add mixer control */

  c = calloc (1, sizeof (*c));
  t_assert (c != NULL); /* XXX */

  c->pub.id = id;

  if (id == TV_CONTROL_ID_VOLUME)
    {
      c->pub.type = TV_CONTROL_TYPE_INTEGER;
      if (TVENG_MIXER_VOLUME_DEBUG)
	c->pub.label = "Mixer volume";
      else
	c->pub.label = _("Volume");
    }
  else
    {
      c->pub.type = TV_CONTROL_TYPE_BOOLEAN;
      if (TVENG_MIXER_VOLUME_DEBUG)
	c->pub.label = "Mixer mute";
      else
	c->pub.label = _("Mute");
    }

  c->info = info;
  c->line = line;

  c->line_cb = tv_mixer_line_callback_add
    (line, mixer_line_notify_cb,
     mixer_line_destroy_cb, c);
  t_assert (c->line_cb != NULL); /* XXX */

  mixer_line_notify_cb (line, c);

  if (orig)
    {
      c->pub.next = orig->next;
      orig->next = &c->pub;
    }
  else
    {
      *tcp = &c->pub;
    }
}

/*
 *  This provisional function logically attaches a
 *  soundcard audio input to a video device.
 *
 *  Two configurations use this: TV cards with an audio
 *  loopback to a soundcard, and TV cards without audio,
 *  where the audio source (e.g. VCR, microphone) 
 *  connects directly to a soundcard.
 *
 *  The function adds an audio mixer volume and mute control,
 *  and sets tv_control->ignore on any video device volume and
 *  mute controls. Tveng will also care to set the video device
 *  controls when mixer controls are changed.
 *
 *  To get rid of the mixer controls, call with line NULL or
 *  just delete the mixer struct.
 *
 *  NB control values are not copied when replacing controls.
 */
void
tveng_attach_mixer_line		(tveng_device_info *	info,
				 tv_mixer *		mixer,
				 tv_mixer_line *	line)
{
  tv_control *tc;

  info->priv->control_mute = NULL;
  info->audio_mutable = 0;

  if (mixer && mixer_line)
    printv ("Attaching mixer %s (%s %s), line %s\n",
	    mixer->node.device,
	    mixer->node.label,
	    mixer->node.driver,
	    mixer_line->label);
  else
    printv ("Removing mixer\n");

  mixer_replace (info, line, TV_CONTROL_ID_VOLUME);
  mixer_replace (info, line, TV_CONTROL_ID_MUTE);

  for (tc = info->controls; tc; tc = tc->next)
    if (tc->id == TV_CONTROL_ID_MUTE && !tc->ignore)
      {
	info->priv->control_mute = tc;
	info->audio_mutable = 1; /* preliminary */
	break;
      }
}

/*
 *  This function implements a second mute switch closer
 *  to the hardware. The 'quiet' state is not visible to the
 *  mute or volume controls or their callbacks, takes priority
 *  and can only be changed with this function.
 *
 *  Intention is that only the user changes audio controls, while
 *  this switch is flipped under program control, such as when
 *  switching channels.
 */
void
tv_quiet_set			(tveng_device_info *	info,
				 tv_bool		quiet)
{
  t_assert (info != NULL);

  TVLOCK;

  if (info->priv->quiet != quiet)
    {
      tv_control *tc;
      tv_bool reset;

      info->priv->quiet = quiet;

      reset = FALSE;

      for (tc = info->controls; tc; tc = tc->next)
	{
	  if (!tc->ignore)
	    {
	      if (tc->id == TV_CONTROL_ID_VOLUME)
		{
		  if (!quiet)
		    {
		      set_control (C(tc), tc->value, FALSE, info);
		      reset |= (tc->_device == NULL);
		    }
		}
	      else if (tc->id == TV_CONTROL_ID_MUTE)
		{
		  if (quiet)
		    {
		      if (tc->_device == NULL)
			{
			  struct control *c = PARENT (tc, struct control, pub);

			  t_assert (c->line != NULL);
			  tv_mixer_line_set_mute (c->line, TRUE);
			}
		      else if (info->priv->module.set_control)
			{
			  tv_callback_node *cb = tc->_callback;
			  int old_value = tc->value;

			  tc->_callback = NULL;
			  info->priv->module.set_control (info, tc, TRUE);
			  tc->_callback = cb;
			  tc->value = old_value;
			}
		    }
		  else /* !quiet */
		    {
		      set_control (C(tc), tc->value, FALSE, info);
		      reset |= (tc->_device == NULL);
		    }
		}
	    }
	}

      if (reset && !quiet)
	reset_video_volume (info, FALSE);
    }

  UNTVLOCK;
}

/*
 *  This is a shortcut to get the mute control state. When @update
 *  is TRUE the control is updated from the driver, otherwise
 *  it returns the last known state.
 *  1 = muted (no sound), 0 = unmuted, -1 = error
 */
int
tv_mute_get			(tveng_device_info *	info,
				 tv_bool		update)
{
  tv_control *tc;

  t_assert (info != NULL);
  t_assert (info->current_controller != TVENG_CONTROLLER_NONE);

  // XXX TVLOCK;

  if ((tc = info->priv->control_mute))
    {
      if (update && (-1 == tveng_update_control (tc, info)))
	return -1;
      else
	return tc->value;
    }

  TVLOCK;
  TVUNSUPPORTED;
  RETURN_UNTVLOCK (-1);
}

/*
 *  This is a shortcut to set the mute control state.
 *  0 = ok, -1 = error.
 */
int
tv_mute_set			(tveng_device_info *	info,
				 tv_bool		mute)
{
  tv_control *tc;
  int r = -1;

  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if ((tc = info->priv->control_mute))
    r = set_control (C(tc), mute, TRUE, info);
  else
    TVUNSUPPORTED;

  RETURN_UNTVLOCK(r);
}

/*
 *  This is a shortcut to add a callback to the mute control.
 */
tv_callback_node *
tv_mute_callback_add		(tveng_device_info *	info,
				 void			(* notify)(tveng_device_info *, void *user_data),
				 void			(* destroy)(tveng_device_info *, void *user_data),
				 void *			user_data)
{
  tv_control *tc;

  t_assert (info != NULL);

  if (!(tc = info->priv->control_mute))
    return NULL;

  return tv_callback_add (&tc->_callback,
			  (void *) notify,
			  (void *) destroy,
			  user_data);
}



/*
  Tunes the current input to the given freq. Returns -1 on error.
*/
int
tveng_tune_input(uint32_t freq, tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->priv->module.tune_input)
    RETURN_UNTVLOCK(info->priv->module.tune_input(freq, info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/*
  Gets the signal strength and the afc code. The afc code indicates
  how to get a better signal, if negative, tune higher, if negative,
  tune lower. 0 means no idea or feature not present in the current
  controller (i.e. V4L1). Strength and/or afc can be NULL pointers,
  that would mean ignore that parameter.
*/
int
tveng_get_signal_strength (int *strength, int * afc,
			   tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->priv->module.get_signal_strength)
    RETURN_UNTVLOCK(info->priv->module.get_signal_strength(strength,
							    afc,
							    info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/*
  Stores in freq the currently tuned freq. Returns -1 on error.
*/
int
tveng_get_tune(uint32_t * freq, tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(freq != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->priv->module.get_tune)
    RETURN_UNTVLOCK(info->priv->module.get_tune(freq, info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/*
  Gets the minimum and maximum freq that the current input can
  tune. If there is no tuner in this input, -1 will be returned.
  If any of the pointers is NULL, its value will not be filled.
*/
int
tveng_get_tuner_bounds(uint32_t * min, uint32_t * max, tveng_device_info *
		       info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->priv->module.get_tuner_bounds)
    RETURN_UNTVLOCK(info->priv->module.get_tuner_bounds(min, max,
							 info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/*
  Sets up the capture device so any read() call after this one
  succeeds. Returns -1 on error.
*/
int
tveng_start_capturing(tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->priv->module.start_capturing)
    RETURN_UNTVLOCK(info->priv->module.start_capturing(info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/* Tries to stop capturing. -1 on error. */
int
tveng_stop_capturing(tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->priv->module.stop_capturing)
    RETURN_UNTVLOCK(info->priv->module.stop_capturing(info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/* 
   Reads a frame from the video device, storing the read data in
   the location pointed to by dest.
   time: time to wait using select() in miliseconds
   info: pointer to the video device info structure
   Returns -1 on error, anything else on success
*/
int tveng_read_frame(tveng_image_data * dest, 
		     unsigned int time, tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->priv->module.read_frame)
    RETURN_UNTVLOCK(info->priv->module.read_frame(dest, time,
						   info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/*
  Gets the timestamp of the last read frame in seconds.
*/
double tveng_get_timestamp(tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->priv->module.get_timestamp)
    RETURN_UNTVLOCK(info->priv->module.get_timestamp(info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/* 
   Sets the capture buffer to an specific size. returns -1 on
   error. Remember to check the value of width and height since it can
   be different to the one requested. 
*/
int tveng_set_capture_size(int width, int height, tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->priv->dword_align)
    width = (width & ~3);
  if (width < info->caps.minwidth)
    width = info->caps.minwidth;
  if (width > info->caps.maxwidth)
    width = info->caps.maxwidth;
  if (height < info->caps.minheight)
    height = info->caps.minheight;
  if (height > info->caps.maxheight)
    height = info->caps.maxheight;

  if (info->priv->module.set_capture_size)
    RETURN_UNTVLOCK(info->priv->module.set_capture_size(width,
							 height,
							 info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/* 
   Gets the actual size of the capture buffer in width and height.
   -1 on error
*/
int tveng_get_capture_size(int *width, int *height, tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(width != NULL);
  t_assert(height != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->priv->module.get_capture_size)
    RETURN_UNTVLOCK(info->priv->module.get_capture_size(width,
							 height,
							 info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/* XF86 Frame Buffer routines */

/* 
   Detects the presence of a suitable Frame Buffer.
   1 if the program should continue (Frame Buffer present,
   available and suitable)
   0 if the framebuffer shouldn't be used.
   priv->display: The priv->display we are connected to (gdk_private->display)
   info: Its fb member is filled in
*/
int
tveng_detect_XF86DGA(tveng_device_info * info)
{
  return x11_dga_present (&dga_param);
}

/*
  Returns 1 if the device attached to info suports previewing, 0 otherwise
*/
int
tveng_detect_preview (tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->priv->module.detect_preview)
    RETURN_UNTVLOCK(info->priv->module.detect_preview(info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return 0;
}

/* 
   Runs zapping_setup_fb with the actual verbosity value.
   Returns -1 in case of error, 0 otherwise.
   This calls (or tries to) the external program zapping_setup_fb,
   that should be installed as suid root.
*/
int
tveng_run_zapping_setup_fb(tveng_device_info * info)
{
  char * argv[10]; /* The command line passed to zapping_setup_fb */
  pid_t pid; /* New child's pid as returned by fork() */
  int status; /* zapping_setup_fb returned status */
  int i=0;
  int verbosity = info->priv->zapping_setup_fb_verbosity;
  char buffer[256]; /* A temporary buffer */

  TVLOCK;

  /* Executes zapping_setup_fb with the given arguments */
  argv[i++] = "zapping_setup_fb";
  argv[i++] = "--device";
  argv[i++] = info -> file_name;

  /* Clip verbosity to valid values */
  if (verbosity < 0)
    verbosity = 0;
  else if (verbosity > 2)
    verbosity = 2;
  for (; verbosity > 0; verbosity --)
    argv[i++] = "--verbose";
  if (info->priv->bpp != -1)
    {
      snprintf(buffer, 255, "%d", info->priv->bpp);
      buffer[255] = 0;
      argv[i++] = "--bpp";
      argv[i++] = buffer;
    }
  argv[i] = NULL;
  
  pid = fork();

  if (pid == -1)
    {
      info->tveng_errno = errno;
      t_error("fork()", info);
      UNTVLOCK;
      return -1;
    }

  if (!pid) /* New child process */
    {
      execvp("zapping_setup_fb", argv);

      /* This shouldn't be reached if everything suceeds */
      perror("execvp(zapping_setup_fb)");

      _exit(2); /* zapping setup_fb on error returns 1 */
    }

  /* This statement is only reached by the parent */
  if (waitpid(pid, &status, 0) == -1)
    {
      info->tveng_errno = errno;
      t_error("waitpid", info);
      UNTVLOCK;
      return -1;
    }

  if (! WIFEXITED(status)) /* zapping_setup_fb exited abnormally */
    {
      info->tveng_errno = errno;
      t_error_msg("WIFEXITED(status)", 
		  "zapping_setup_fb exited abnormally, check stderr",
		  info);
      UNTVLOCK;
      return -1;
    }

  switch (WEXITSTATUS(status))
    {
    case 1:
      info -> tveng_errno = -1;
      t_error_msg("case 1",
		  "zapping_setup_fb failed to set up the video",
		  info);
      UNTVLOCK;
      return -1;
    case 2:
      info -> tveng_errno = -1;
      t_error_msg("case 2",
		  _("Couldn't locate zapping_setup_fb, check your install"),
		  info);
      UNTVLOCK;
      return -1;
    default:
      break; /* Exit code == 0, success setting up the framebuffer */
    }
  UNTVLOCK;
  return 0; /* Success */
}

/* 
   This is a convenience function, it returns the real screen depth in
   PRIV->BPP (bits per pixel). This one is quite important for 24 and 32 bit
   modes, since the default X visual may be 24 bit and the real screen
   depth 32, thus an expensive RGB -> RGBA conversion must be
   performed for each frame.
   priv->display: the priv->display we want to know its real depth (can be
   accessed through gdk_private->display)
*/
int
tveng_get_display_depth(tveng_device_info * info)
{
  /* This routines are taken form xawtv, i don't understand them very
     well, but they seem to work OK */
  XVisualInfo * visual_info, template;
  XPixmapFormatValues * pf;
  Display * dpy = info->priv->display;
  int found, v, i, n;
  int bpp = 0;

  TVLOCK;

  if (info->priv->bpp != -1)
    RETURN_UNTVLOCK(info->priv->bpp);

  /* Use the first screen, should give no problems assuming this */
  template.screen = 0;
  visual_info = XGetVisualInfo(dpy, VisualScreenMask, &template, &found);
  v = -1;
  for (i = 0; v == -1 && i < found; i++)
    if (visual_info[i].class == TrueColor && visual_info[i].depth >=
	15)
      v = i;

  if (v == -1) {
    info -> tveng_errno = -1;
    t_error_msg("XGetVisualInfo",
		"Cannot find an appropiate visual", info);
    XFree(visual_info);
    UNTVLOCK;
    return 0;
  }
  
  /* get depth + bpp (heuristic) */
  pf = XListPixmapFormats(dpy, &n);
  for (i = 0; i < n; i++) {
    if (pf[i].depth == visual_info[v].depth) {
      if (visual_info[v].depth == 15)
	bpp = 15; /* here bits_per_pixel is 16, but the depth is 15 */
      else
	bpp = pf[i].bits_per_pixel;
      break;
    }
  }

  if (bpp == 0) {
    info -> tveng_errno = -1;
    t_error_msg("XListPixmapFormats",
		"Cannot figure out X depth", info);
    XFree(visual_info);
    XFree(pf);
    UNTVLOCK;
    return 0;
  }

  XFree(visual_info);
  XFree(pf);

  UNTVLOCK;
  return bpp;
}

/*
  Sets the preview window dimensions to the given window.
  Returns -1 on error, something else on success.
  Success doesn't mean that the requested dimensions are used, maybe
  they are different, check the returned fields to see if they are suitable
  info   : Device we are controlling
  The current chromakey value is used, the caller doesn't need to fill
  it in.
*/
int
tveng_set_preview_window(tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->priv->dword_align)
    {
      info->window.x = (info->window.x+3) & ~3;
      info->window.width = (info->window.width+3) & ~3;
    }
  if (info->window.height < info->caps.minheight)
    info->window.height = info->caps.minheight;
  if (info->window.height > info->caps.maxheight)
    info->window.height = info->caps.maxheight;
  if (info->window.width < info->caps.minwidth)
    info->window.width = info->caps.minwidth;
  if (info->window.width > info->caps.maxwidth)
    info->window.width = info->caps.maxwidth;

  if (info->priv->module.set_preview_window)
    RETURN_UNTVLOCK(info->priv->module.set_preview_window(info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/*
  Gets the current overlay window parameters.
  Returns -1 on error, and any other value on success.
  info   : The device to use
*/
int
tveng_get_preview_window(tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->priv->module.get_preview_window)
    RETURN_UNTVLOCK(info->priv->module.get_preview_window(info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/* 
   Sets the previewing on/off.
   on : if 1, set preview on, if 0 off, other values are silently ignored
   info  : device to use for previewing
   Returns -1 on error, anything else on success
*/
int
tveng_set_preview (int on, tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->priv->module.set_preview)
    RETURN_UNTVLOCK(info->priv->module.set_preview(on, info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/* Adjusts the verbosity value passed to zapping_setup_fb, cannot fail
 */
void
tveng_set_zapping_setup_fb_verbosity(int level, tveng_device_info *
				     info)
{
  t_assert(info != NULL);

  if (level > 2)
    level = 2;
  else if (level < 0)
    level = 0;
  info->priv->zapping_setup_fb_verbosity = level;
}

/* Sets the chromakey */
void tveng_set_chromakey(uint32_t chroma, tveng_device_info *info)
{
  t_assert (info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->priv->module.set_chromakey)
    info->priv->module.set_chromakey (chroma, info);

  UNTVLOCK;
  return;
}

int tveng_get_chromakey (uint32_t *chroma, tveng_device_info *info)
{
  t_assert (info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->priv->module.get_chromakey)
    RETURN_UNTVLOCK (info->priv->module.get_chromakey (chroma, info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/* Sets the dword align flag */
void tveng_set_dword_align(int dword_align, tveng_device_info *info)
{
  info->priv->dword_align = dword_align;
}

/* Returns the current verbosity value passed to zapping_setup_fb */
int
tveng_get_zapping_setup_fb_verbosity(tveng_device_info * info)
{
  return (info->priv->zapping_setup_fb_verbosity);
}

#include "zmisc.h"
/* 
   Sets up everything and starts previewing.
   Just call this function to start previewing, it takes care of
   (mostly) everything.
   Returns -1 on error.
*/
int
tveng_start_previewing (tveng_device_info * info, const char *mode)
{
  x11_vidmode_info *v;
  unsigned int width, height;

  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  /* XXX we must distinguish between (limited) Xv overlay and Xv scaling.
   * 1) determine natural size (videostd -> 480, 576 -> 640, 768),
   *    try Xv target size, get actual
   * 2) find closest vidmode
   * 3) try Xv target size from vidmode, get actual,
   *    if closer go back to 2) until sizes converge 
   */
  if (info->current_controller == TVENG_CONTROLLER_XV
      && (mode == NULL || 0 == strcmp (mode, "auto")))
     mode = NULL; /* not needed  XXX wrong */
  else
  /* XXX ditto, limited V4L/V4L2 overlay */
    if (!tveng_detect_XF86DGA(info))
      {
	info->tveng_errno = -1;
	t_error_msg("tveng_detect_XF86DGA",
		    "No DGA present, make sure you enable it in"
		    " /etc/X11/XF86Config.",
		    info);
	UNTVLOCK;
	return -1;
      }

  /* special code, used only inside tveng, means remember from last
     time */
  if (mode == (const char *) -1)
    mode = info->priv->mode;
  else
    {
      g_free (info->priv->mode);
      info->priv->mode = g_strdup (mode);
    }

  width = info->caps.maxwidth; /* XXX wrong */
  height = info->caps.maxheight;

  v = NULL;

  if (mode == NULL || mode[0] == 0)
    {
      /* Don't change, v = NULL */
    }
  else if (vidmodes && !(v = x11_vidmode_by_name (vidmodes, mode)))
    {
      x11_vidmode_info *vmin;
      long long amin;

      /* Automatic */

      vmin = vidmodes;
      amin = 1LL << 62;

      for (v = vidmodes; v; v = v->next)
	{
	  long long a;
	  long long dw, dh;

	  /* XXX ok? */
	  dw = (long long) v->width - width;
	  dh = (long long) v->height - height;
	  a = dw * dw + dh * dh;

	  if (a < amin
	      || (a == amin
		  && v->vfreq > vmin->vfreq))
	    {
	      vmin = v;
	      amin = a;
	    }
	}

      v = vmin;

      if (info->debug_level > 0)
	fprintf (stderr, "Using mode %ux%u@%u for %ux%u\n",
		 v->width, v->height, (unsigned int)(v->vfreq + 0.5),
		 width, height);
    }

  if (!x11_vidmode_switch (vidmodes, v, &info->priv->old_mode))
    goto failure;

  if (info->priv->module.start_previewing)
    RETURN_UNTVLOCK(info->priv->module.start_previewing(info, &dga_param));

 failure:
  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/*
  Stops the fullscreen mode. Returns -1 on error
*/
int
tveng_stop_previewing(tveng_device_info * info)
{
  int return_code = 0;

  t_assert(info != NULL);

  TVLOCK;

  if (info->priv->module.stop_previewing)
    return_code = info->priv->module.stop_previewing(info);

  x11_vidmode_restore (vidmodes, &info->priv->old_mode);

  UNTVLOCK;

  return return_code;
}

/*
  Sets up everything and starts previewing in a window. It doesn't do
  many of the things tveng_start_previewing does, it's mostly just a
  wrapper around tveng_set_preview_on. Returns -1 on error
  The window must be specified from before calling this function (with
  tveng_set_preview_window), and overlaying must be available.
*/
int
tveng_start_window (tveng_device_info * info)
{
  t_assert(info != NULL);

  TVLOCK;

  tveng_stop_everything(info);

  t_assert(info -> current_mode == TVENG_NO_CAPTURE);

  if (!tveng_detect_preview(info))
    /* We shouldn't be reaching this if the app is well programmed */
    t_assert_not_reached();

  tveng_set_preview_window(info);

  if (tveng_set_preview_on(info) == -1)
    RETURN_UNTVLOCK(-1);

  info->current_mode = TVENG_CAPTURE_WINDOW;

  UNTVLOCK;
  return 0;
}

/*
  Stops the window mode. Returns -1 on error
*/
int
tveng_stop_window (tveng_device_info * info)
{
  t_assert(info != NULL);

  TVLOCK;

  if (info -> current_mode == TVENG_NO_CAPTURE)
    {
      fprintf(stderr, 
	      "Warning: trying to stop window with no capture active\n");
      UNTVLOCK;
      return 0; /* Nothing to be done */
    }

  t_assert(info->current_mode == TVENG_CAPTURE_WINDOW);

  /* No error checking */
  tveng_set_preview_off(info);

  info -> current_mode = TVENG_NO_CAPTURE;

  UNTVLOCK;
  return 0; /* Success */
}

/*
  Utility function, stops the capture or the previewing. Returns the
  mode the device was before stopping.
  For stopping and restarting the device do:
  enum tveng_capture_mode cur_mode;
  cur_mode = tveng_stop_everything(info);
  ... do some stuff ...
  if (tveng_restart_everything(cur_mode, info) == -1)
     ... show error dialog ...
*/
enum tveng_capture_mode tveng_stop_everything (tveng_device_info *
					       info)
{
  enum tveng_capture_mode returned_mode;

  t_assert(info != NULL);

  TVLOCK;

  returned_mode = info->current_mode;
  switch (info->current_mode)
    {
    case TVENG_CAPTURE_READ:
      tveng_stop_capturing(info);
      break;
    case TVENG_CAPTURE_PREVIEW:
      tveng_stop_previewing(info);
      break;
    case TVENG_CAPTURE_WINDOW:
      tveng_stop_window(info);
      break;
    default:
      break;
    };

  t_assert(info->current_mode == TVENG_NO_CAPTURE);

  UNTVLOCK;

  return returned_mode;
}

/*
  Restarts the given capture mode. See the comments on
  tveng_stop_everything. Returns -1 on error.
*/
int tveng_restart_everything (enum tveng_capture_mode mode,
			      tveng_device_info * info)
{
  t_assert(info != NULL);

  TVLOCK;

  switch (mode)
    {
    case TVENG_CAPTURE_READ:
      if (tveng_start_capturing(info) == -1)
	RETURN_UNTVLOCK(-1);
      break;
    case TVENG_CAPTURE_PREVIEW:
      if (tveng_start_previewing(info, (const char *) -1) == -1)
	RETURN_UNTVLOCK(-1);
      break;
    case TVENG_CAPTURE_WINDOW:
      if (tveng_start_window(info) == -1)
	RETURN_UNTVLOCK(-1);
      break;
    default:
      break;
    }

  UNTVLOCK;
  return 0; /* Success */
}

int tveng_get_debug_level(tveng_device_info * info)
{
  t_assert(info != NULL);

  return (info->debug_level);
}

void tveng_set_debug_level(int level, tveng_device_info * info)
{
  t_assert(info != NULL);

  info->debug_level = level;
}

void tveng_set_xv_support(int disabled, tveng_device_info * info)
{
  t_assert(info != NULL);

  info->priv->disable_xv = disabled;
}

void tveng_assume_yvu(int assume, tveng_device_info * info)
{
  t_assert(info != NULL);

  info->priv->assume_yvu = assume;
}

#ifdef USE_XV
void tveng_set_xv_port(XvPortID port, tveng_device_info * info)
{
  XvAttribute *at;
  int attributes, i;
  Display *dpy;

  TVLOCK;

  info->priv->port = port;
  dpy = info->priv->display;
  info->priv->filter = info->priv->colorkey =
    info->priv->double_buffer = None;

  /* Add the controls in this port to the struct of controls */
  at = XvQueryPortAttributes(dpy, port, &attributes);
  for (i=0; i<attributes; i++)
    {
      struct control c;

      if (info->debug_level)
	fprintf(stderr, "  TVeng.c Xv atom: %s%s%s (%i -> %i)\n",
		at[i].name,
		(at[i].flags & XvGettable) ? " gettable" : "",
		(at[i].flags & XvSettable) ? " settable" : "",
		at[i].min_value, at[i].max_value);

      /* Any attribute not settable and Gettable is of little value */
      if ((!(at[i].flags & XvGettable)) ||
	  (!(at[i].flags & XvSettable)))
	continue;

      memset (&c, 0, sizeof (c));

      if (!strcmp("XV_FILTER", at[i].name))
	  {
	    info->priv->filter = XInternAtom(dpy, "XV_FILTER",
						False);
	    c.atom = info->priv->filter;
	    if (!(c.pub.label = strdup (_("Filter"))))
	      goto failure;
	    c.pub.minimum = at[i].min_value;
	    c.pub.maximum = at[i].max_value;
	    c.pub.type = TV_CONTROL_TYPE_BOOLEAN;
	    c.pub.menu = NULL;
	    c.pub._device = NULL; /* TVENG_CONTROLLER_MOTHER; */
	    if (!append_control(info, &c.pub, sizeof (c)))
	      {
	      failure:
		UNTVLOCK;
		return;
	      }
	  }

      else if (!strcmp("XV_DOUBLE_BUFFER", at[i].name))
	  {
	    info->priv->double_buffer = XInternAtom(dpy, "XV_DOUBLE_BUFFER",
						       False);
	    c.atom = info->priv->double_buffer;
	    if (!(c.pub.label = strdup (_("Filter"))))
	      goto failure2;
	    c.pub.minimum = at[i].min_value;
	    c.pub.maximum = at[i].max_value;
	    c.pub.type = TV_CONTROL_TYPE_BOOLEAN;
	    c.pub.menu = NULL;
	    c.pub._device = NULL; /* TVENG_CONTROLLER_MOTHER; */
	    if (!append_control(info, &c.pub, sizeof (c)))
	      {
	      failure2:
		UNTVLOCK;
		return;
	      }
	  }

      else if (!strcmp("XV_COLORKEY", at[i].name))
	  {
	    info->priv->colorkey = XInternAtom(dpy, "XV_COLORKEY",
						False);
	    c.atom = info->priv->colorkey;
	    if (!(c.pub.label = strdup (_("Colorkey"))))
	      goto failure3;
	    c.pub.minimum = at[i].min_value;
	    c.pub.maximum = at[i].max_value;
	    c.pub.type = TV_CONTROL_TYPE_COLOR;
	    c.pub.menu = NULL;
	    c.pub._device = NULL; /* TVENG_CONTROLLER_MOTHER; */
	    if (!append_control(info, &c.pub, sizeof (c)))
	      {
	      failure3:
		UNTVLOCK;
		return;
	      }
	  }
    }

  tveng_update_controls(info);

  UNTVLOCK;
}

void tveng_unset_xv_port(tveng_device_info * info)
{
  info->priv->port = None;
  info->priv->filter = info->priv->colorkey = None;
}

int tveng_detect_xv_overlay(tveng_device_info * info)
{
  Display *dpy = info->priv->display;
  Window root_window = DefaultRootWindow(dpy);
  unsigned int version, revision, major_opcode, event_base,
    error_base;
  int nAdaptors;
  int i,j;
  XvAttribute *at;
  int attributes;
  XvAdaptorInfo *pAdaptors, *pAdaptor;
  XvPortID port; /* port id */
  XvEncodingInfo *ei; /* list of encodings, for reference */
  int encodings; /* number of encodings */

  if (info->priv->disable_xv)
    return 0;

  if (Success != XvQueryExtension(dpy, &version, &revision,
				  &major_opcode, &event_base,
				  &error_base))
    goto error1;

  if (info->debug_level > 0)
    fprintf(stderr, "tvengxv.c: XVideo major_opcode: %d\n",
	    major_opcode);

  if (version < 2 || (version == 2 && revision < 2))
    goto error1;

  if (Success != XvQueryAdaptors(dpy, root_window, &nAdaptors,
				 &pAdaptors))
    goto error1;

  if (nAdaptors <= 0)
    goto error1;

  for (i=0; i<nAdaptors; i++)
    {
      pAdaptor = pAdaptors + i;
      if ((pAdaptor->type & XvInputMask) &&
	  (pAdaptor->type & XvVideoMask))
	{ /* available port found */
	  for (j=0; j<pAdaptor->num_ports; j++)
	    {
	      port = pAdaptor->base_id + j;

	      if (Success == XvGrabPort(dpy, port, CurrentTime))
		goto adaptor_found;
	    }
	}
    }

  goto error2; /* no adaptors found */

  /* success */
 adaptor_found:
  /* Check that it supports querying controls and encodings */
  if (Success != XvQueryEncodings(dpy, port,
				  &encodings, &ei))
    goto error3;

  if (encodings <= 0)
    {
      info->tveng_errno = -1;
      t_error_msg("encodings",
		  "You have no encodings available",
		  info);
      goto error3;
    }

  if (ei)
    XvFreeEncodingInfo(ei);

  /* create the atom that handles the encoding */
  at = XvQueryPortAttributes(dpy, port, &attributes);
  if ((!at) && (attributes <= 0))
    goto error3;

  XvFreeAdaptorInfo(pAdaptors);
  XvUngrabPort(dpy, port, CurrentTime);
  return 1; /* the port seems to work ok, success */

 error3:
  XvUngrabPort(dpy, port, CurrentTime);
 error2:
  XvFreeAdaptorInfo(pAdaptors);
 error1:
  return 0; /* failure */
}
#else
int tveng_detect_xv_overlay(tveng_device_info * info)
{
  return 0; /* XV not built, no overlaying possible */
}
#endif

int tveng_is_planar (enum tveng_frame_pixformat fmt)
{
  return ((fmt == TVENG_PIX_YUV420) ||
	  (fmt == TVENG_PIX_YVU420));
}

int
tveng_ov511_get_button_state (tveng_device_info *info)
{
  t_assert(info != NULL);

  if (info->current_controller == TVENG_CONTROLLER_NONE)
    return -1;

  TVLOCK;

  if (info->priv->module.ov511_get_button_state)
    RETURN_UNTVLOCK(info->priv->module.ov511_get_button_state(info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

void tveng_mutex_lock(tveng_device_info * info)
{
  TVLOCK;
}

void tveng_mutex_unlock(tveng_device_info * info)
{
  UNTVLOCK;
}

char *
tveng_strdup_printf(const char *templ, ...)
{
  char *s, buf[512];
  va_list ap;
  int temp;

  temp = errno;

  va_start(ap, templ);
  vsnprintf(buf, sizeof(buf) - 1, templ, ap);
  va_end(ap);

  s = strdup(buf);

  errno = temp;

  return s;
}

/*
 *  Simple callback mechanism 
 */

struct _tv_callback_node {
	tv_callback_node *	next;
	tv_callback_node **	pred;
	void			(* notify)(void *, void *);
	void			(* destroy)(void *, void *);
	void *			user_data;
	unsigned int		blocked;
};

/*
 *  Add a function to a callback list. The notify function is called
 *  on some event, the destroy function (if not NULL) before the list
 *  is deleted.
 *
 *  Not intended to be called directly by tveng clients.
 */
tv_callback_node *
tv_callback_add			(tv_callback_node **	list,
				 void			(* notify)(void *, void *user_data),
				 void			(* destroy)(void *, void *user_data),
				 void *			user_data)
{
	tv_callback_node *cb;

	t_assert (list != NULL);

	for (; (cb = *list); list = &cb->next)
		;

	if (!(cb = calloc (1, sizeof (*cb))))
		return NULL;

	*list = cb;

	cb->pred = list; 

	cb->notify = notify;
	cb->destroy = destroy;
	cb->user_data = user_data;

	return cb;
}

/*
 *  Remove a callback function from its list. Argument is the result
 *  of tv_callback_add().
 *
 *  Intended for tveng clients. Note the destroy handler is not called.
 */
void
tv_callback_remove		(tv_callback_node *	cb)
{
	if (cb) {
		if (cb->next)
			cb->next->pred = cb->pred;
		*(cb->pred) = cb->next;
		free (cb);
	}
}

/*
 *  Delete an entire callback list, usually before the object owning
 *  the list is destroyed.
 */
void
tv_callback_destroy		(void *			object,
				 tv_callback_node **	list)
{
	tv_callback_node *cb;

	t_assert (object != NULL);
	t_assert (list != NULL);

	while ((cb = *list)) {
		*list = cb->next;

		if (cb->next)
			cb->next->pred = list;

		if (cb->destroy)
			cb->destroy (object, cb->user_data);

		free (cb);
	}
}

/*
 *  Temporarily block calls to the notify handler.
 */
void
tv_callback_block		(tv_callback_node *	cb)
{
  	if (cb)
		cb->blocked++;
}

/*
 *  Counterpart of tv_callback_block().
 */
void
tv_callback_unblock		(tv_callback_node *	cb)
{
	if (cb && cb->blocked > 0)
		cb->blocked--;
}

/*
 *  Traverse a callback list and call all notify functions,
 *  usually on some event. Only the list owner should call this.
 */
void
tv_callback_notify		(void *			object,
				 const tv_callback_node *list)
{
	const tv_callback_node *cb;

	t_assert (object != NULL);

	for (cb = list; cb; cb = cb->next)
		if (cb->notify && cb->blocked == 0)
			cb->notify (object, cb->user_data);
}

/*
 *  Device nodes
 */

tv_device_node *
tv_device_node_add		(tv_device_node **	list,
				 tv_device_node *	node)
{
	t_assert (list != NULL);

	if (!node)
		return NULL;

	while (*list)
		list = &(*list)->next;

	*list = node;
	node->next = NULL;

	return node;
}

tv_device_node *
tv_device_node_remove		(tv_device_node **	list,
				 tv_device_node *	node)
{
	if (node) {
		while (*list && *list != node)
			list = &(*list)->next;

		*list = node->next;
		node->next = NULL;
	}

	return node;
}

void
tv_device_node_delete		(tv_device_node **	list,
				 tv_device_node *	node,
				 tv_bool		restore)
{
	if (node) {
		if (list) {
			while (*list && *list != node)
				list = &(*list)->next;

			*list = node->next;
		}

		if (node->destroy)
			node->destroy (node, restore);
	}
}

static void
destroy_device_node		(tv_device_node *	node,
				 tv_bool		restore)
{
	if (!node)
		return;

	free (node->label);
	free (node->bus);
	free (node->driver);
	free (node->version);
	free (node->device);
	free (node);
}

tv_device_node *
tv_device_node_new		(const char *		label,
				 const char *		bus,
				 const char *		driver,
				 const char *		version,
				 const char *		device,
				 unsigned int		size)
{
	tv_device_node *node;

	if (!(node = calloc (1, MAX (size, sizeof (*node)))))
		return NULL;

	node->destroy = destroy_device_node;

#define STR_COPY(s) if (s && !(node->s = strdup (s))) goto error;

	STR_COPY (label);
	STR_COPY (bus);
	STR_COPY (driver);
	STR_COPY (version);
	STR_COPY (device);

	return node;

 error:
	destroy_device_node (node, FALSE);
	return NULL;
}

tv_device_node *
tv_device_node_find		(tv_device_node *	list,
				 const char *		name)
{
	struct stat st;

	if (-1 == stat (name, &st))
		return FALSE;

	for (; list; list = list->next) {
		struct stat lst;

		if (0 == strcmp (list->device, name))
			return list;

		if (-1 == stat (list->device, &lst))
			continue;

		if ((S_ISCHR (lst.st_mode) && S_ISCHR (st.st_mode))
		    || (S_ISBLK (lst.st_mode) && S_ISBLK (st.st_mode))) {
			/* Major and minor device number */
			if (lst.st_rdev == st.st_rdev)
				return list;
		}
	}

	return NULL;
}

#if 0
	tv_device_node *head = NULL;
	struct dirent dirent, *pdirent = &dirent;
	DIR *dir = NULL;

	t_assert (filter != NULL);

	if (!list)
		list = &head;

	for (; names && *names && names[0][0]; names++) {
		tv_device_node *node;

		if (find_node (*list, *names))
			continue;

		if ((node = filter (*names)))
			tv_device_node_append (list, node);
	}

	if (path && (dir = opendir (path))) {
		while (0 == readdir_r (dir, &dirent, &pdirent) && pdirent) {
			tv_device_node *node;
			char *s;

			if (!(s = malloc (strlen (path) + strlen (dirent.d_name) + 2)))
				continue;

			strcpy (s, path);
			if (s[0])
				strcat (s, "/");
			strcat (s, dirent.d_name);

			if (find_node (*list, s))
				continue;

			if ((node = filter (s)))
				tv_device_node_append (list, node);
		}

		closedir (dir);
	}

	return *list;
#endif
