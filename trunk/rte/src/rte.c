/*
 *  Real Time Encoder
 *
 *  Copyright (C) 2000-2001 I�aki Garc�a Etxebarria
 *  Modified 2001 Michael H. Schimek
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
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "rtepriv.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*
  TODO in no particular order:
  * context->bsize should be visible by the app.
  * Push modes not functional (think about termination).
  * better error reporting.
  * Pause, resume.
  * Status api.
  * Sliced vbi.
  * Finish b_mp1e, write b_divx4linux and b_ffmpeg.
  * i18n support.
*/

#define xc context->class
#define dc codec->class

#ifdef MP1E
extern const rte_backend_info b_mp1e_info;
#endif

static const rte_backend_info *
backends[] = 
{
#ifdef MP1E
	&b_mp1e_info
#endif
	/* tbd */
};
static const int num_backends = sizeof(backends)/sizeof(backends[0]);

/* Private flag for fifos */
#define BLANK_BUFFER 1

/* one function to iterate them all */
static rte_bool codec_forall(rte_context *context,
			     rte_bool (*func)(rte_codec*, rte_pointer),
			     rte_pointer udata)
{
	int i;
	int j;

	/* FIXME: Should this use codec->next instead?? */

	for (i=0; i<RTE_STREAM_MAX; i++)
		for (j=0; j<xc->public.elementary[i]; j++) {
			rte_codec *codec = rte_codec_get(context, i, j);
			if (codec && !func(codec, udata))
				return FALSE;
		}

	return TRUE;
}

/* inits the backends if not already inited. */
static void
rte_init(void)
{
	static rte_bool inited = FALSE;

	if (!inited) {
		int i;
		for (i = 0; i<num_backends; i++)
			if (backends[i]->init)
				backends[i]->init();

		inited = TRUE;
	}
}

rte_context_info *
rte_context_info_enum(int index)
{
	rte_context_info *rxi;
	int i, j;

	rte_init();
	
	for (j = 0; j < num_backends; j++)
		for (i = 0; backends[j]->context_enum != NULL
			     && (rxi = backends[j]->context_enum(i)); i++)
			if (index-- == 0)
				return rxi;
	return NULL;
}

rte_context_info *
rte_context_info_keyword(const char *keyword)
{
	rte_context_info *rxi;
	int i, j;

	rte_init();

	for (j = 0; j < num_backends; j++)
		for (i = 0; backends[j]->context_enum != NULL
			     && (rxi = backends[j]->context_enum(i)); i++)
			if (strcmp(keyword, rxi->keyword) == 0)
				return rxi;
	return NULL;
}

rte_context_info *
rte_context_info_context(rte_context *context)
{
	nullcheck(context, return NULL);

	return &xc->public;
}

rte_context *
rte_context_new(const char *keyword)
{
	rte_context *context = NULL;
	int j;

	rte_init();

	for (j = 0; j < num_backends; j++)
		if (backends[j]->context_new
		    && (context = backends[j]->context_new(keyword))) {
			/* set output and we are ready */
			context->status = RTE_STATUS_PARAM;
			return context;
		}

	return NULL;
}

void
rte_context_delete(rte_context *context)
{
	nullcheck(context, return);

	if (context->status == RTE_STATUS_RUNNING)
		rte_stop(context);
	else if (context->status == RTE_STATUS_PAUSED)
		/* FIXME */;

	if (context->error) {
		free(context->error);
		context->error = NULL;
	}

	xc->delete(context);
}

void
rte_context_set_user_data(rte_context *context, rte_pointer user_data)
{
	context->user_data = user_data;
}

rte_pointer
rte_context_get_user_data	(rte_context *context)
{
	return context->user_data;
}

rte_codec_info *
rte_codec_info_enum(rte_context *context, int index)
{
	nullcheck(context, return NULL);

	if (!xc->codec_enum)
		return NULL;

	return xc->codec_enum(context, index);
}

rte_codec_info *
rte_codec_info_keyword(rte_context *context,
		       const char *keyword)
{
	rte_codec_info *rci;
	int i;

	nullcheck(context, return NULL);

	if (!xc->codec_enum)
		return NULL;

	for (i = 0;; i++)
	        if (!(rci = xc->codec_enum(context, i))
		    || strcmp(keyword, rci->keyword) == 0)
			break;
	return rci;
}

rte_codec_info *
rte_codec_info_codec(rte_codec *codec)
{
	nullcheck(codec, return NULL);

	return &dc->public;
}

rte_codec *
rte_codec_set(rte_context *context,
	      rte_stream_type stream_type,
	      int stream_index,
	      const char *codec_keyword)
{
	nullcheck(context, return NULL);
	nullcheck(xc->codec_set, return NULL);

	return xc->codec_set(context, stream_type, stream_index,
			     codec_keyword);
}

void
rte_codec_remove(rte_context *context,
		 rte_stream_type stream_type,
		 int stream_index)
{
	nullcheck(context, return);
	nullcheck(xc->codec_set, return);

	xc->codec_set(context, stream_type, stream_index, NULL);
}

rte_codec *
rte_codec_get(rte_context *context, rte_stream_type stream_type,
	      int stream_index)
{
	nullcheck(context, return NULL);
	nullcheck(xc->codec_get, return NULL);

	return xc->codec_get(context, stream_type, stream_index);
}

void
rte_codec_set_user_data(rte_codec *codec, rte_pointer data)
{
	nullcheck(codec, return);

	codec->user_data = data;
}

rte_pointer
rte_codec_get_user_data(rte_codec *codec)
{
	nullcheck(codec, return NULL);

	return codec->user_data;
}

rte_bool
rte_codec_set_parameters(rte_codec *codec, rte_stream_parameters *rsp)
{
	nullcheck(codec, return FALSE);
	nullcheck(rsp, return FALSE);
	nullcheck(dc->set_parameters, return FALSE);

	return dc->set_parameters(codec, rsp);
}

void
rte_codec_get_parameters(rte_codec *codec, rte_stream_parameters *rsp)
{
	nullcheck(codec, return);
	nullcheck(rsp, return);
	nullcheck(dc->get_parameters, return);

	dc->get_parameters(codec, rsp);

}

void
rte_set_input_callback_buffered(rte_codec *codec,
				rteBufferCallback get_cb,
				rteBufferCallback unref_cb)
{
	nullcheck(codec, return);

	if (codec->status != RTE_STATUS_PARAM &&
	    codec->status != RTE_STATUS_READY) {
		rte_error(NULL,
			  "You must set_parameters before setting the"
			  " input mode.");
		return;
	}

	codec->input_mode = RTE_INPUT_CB;
	codec->input.cb.get = get_cb;
	codec->input.cb.unref = unref_cb;

	codec->status = RTE_STATUS_READY;
}

void
rte_set_input_callback_data(rte_codec *codec,
			    rteDataCallback data_cb)
{
	nullcheck(codec, return);

	if (codec->status != RTE_STATUS_PARAM &&
	    codec->status != RTE_STATUS_READY) {
		rte_error(NULL,
			  "You must set_parameters before setting the"
			  " input mode.");
		return;
	}

	codec->input_mode = RTE_INPUT_CD;
	codec->input.cd.get = data_cb;

	codec->status = RTE_STATUS_READY;
}

void
rte_set_input_push_buffered(rte_codec *codec,
			    rteBufferCallback unref_cb)
{
	nullcheck(codec, return);

	if (codec->status != RTE_STATUS_PARAM &&
	    codec->status != RTE_STATUS_READY) {
		rte_error(NULL,
			  "You must set_parameters before setting the"
			  " input mode.");
		return;
	}

	codec->input_mode = RTE_INPUT_PB;
	codec->input.pb.unref = unref_cb;

	codec->status = RTE_STATUS_READY;
}

void
rte_set_input_push_data(rte_codec *codec)
{
	nullcheck(codec, return);

	if (codec->status != RTE_STATUS_PARAM &&
	    codec->status != RTE_STATUS_READY) {
		rte_error(NULL,
			  "You must set_parameters before setting the"
			  " input mode.");
		return;
	}

	codec->input_mode = RTE_INPUT_PD;

	codec->status = RTE_STATUS_READY;
}

void
rte_push_buffer(rte_codec *codec, rte_buffer *rbuf,
		rte_bool blocking)
{
	buffer *b;

	nullcheck(codec, return);

	if (codec->input_mode != RTE_INPUT_PB) {
		rte_error(NULL, "[%s] Pushing buffer but current input mode"
			  " isn't push_buffered", dc->public.keyword);
		return;
	}

	if (blocking)
		b = wait_empty_buffer(&codec->prod);
	else {
		b = recv_empty_buffer(&codec->prod);
		if (!b) {
			rte_error(codec->context,
				  "[%s] push_buffer failed, "
				  "would block", dc->public.keyword);
			return;
		}
	}

	b->time = rbuf->timestamp;
	b->data = rbuf->data;
	b->used = codec->bsize;
	b->user_data = rbuf->user_data;

	send_full_buffer(&codec->prod, b);
}

rte_pointer
rte_push_data(rte_codec *codec, rte_pointer data, double timestamp,
	      rte_bool blocking)
{
	buffer *b, *b2 = NULL;

	nullcheck(codec, return NULL);

	if (codec->input_mode != RTE_INPUT_PD) {
		rte_error(NULL, "[%s] Pushing data but current input mode"
			  " isn't push_data", dc->public.keyword);
		return NULL;
	}

	b = codec->input.pd.last_buffer;
	
	if (!blocking) {
		b2 = recv_empty_buffer(&codec->prod);
		if (!b2) {
			rte_error(codec->context, "[%s] push_data failed, "
				  "would block", dc->public.keyword);
			return NULL;
		}
	}

	if ((data && !b) || (!data && b)) {
		rte_error(NULL, "[%s] Stick to the usage, please",
			  dc->public.keyword);
		return NULL;
	}

	if (data) {
		if (data != b->data) {
			rte_error(NULL, "[%s] You haven't written to"
				  " the provided buffer",
				  dc->public.keyword);
			return NULL;
		}
		/* We always send full buffers except eof */
		/* FIXME: We should expose b->allocated somehow */
		b->used = codec->bsize;
		b->time = timestamp;
		send_full_buffer(&codec->prod, b);
	}

	if (blocking)
		b2 = wait_empty_buffer(&codec->prod);

	codec->input.pd.last_buffer = b2;

	return b->data;
}

void
rte_set_output_callback(rte_context *context,
			rteWriteCallback write_cb,
			rteSeekCallback seek_cb)
{
	nullcheck(context, return);

	context->write = write_cb;
	context->seek = seek_cb;

	context->status = RTE_STATUS_READY;
}

rte_option_info *
rte_codec_option_info_enum(rte_codec *codec, int index)
{
	nullcheck(codec, return 0);

	if (!dc->option_enum)
		return NULL;

	return dc->option_enum(codec, index);
}

rte_option_info *
rte_codec_option_info_keyword(rte_codec *codec, const char *keyword)
{
	rte_option_info *ro;
	int i;

	nullcheck(codec, return NULL);

	if (!dc->option_enum)
		return NULL;

	for (i = 0;; i++)
	        if (!(ro = dc->option_enum(codec, i))
		    || strcmp(keyword, ro->keyword) == 0)
			break;
	return ro;
}

rte_bool
rte_codec_option_get(rte_codec *codec, const char *keyword,
		     rte_option_value *v)
{
	nullcheck(codec, return 0);

	if (!dc->option_get)
		return FALSE;

	return dc->option_get(codec, keyword, v);
}

rte_bool
rte_codec_option_set(rte_codec *codec, const char *keyword, ...)
{
	va_list args;
	rte_bool r;

	nullcheck(codec, return FALSE);

	if (!dc->option_set)
		return FALSE;

	va_start(args, keyword);

	r = dc->option_set(codec, keyword, args);

	va_end(args);

	return r;
}

char *
rte_codec_option_print(rte_codec *codec, const char *keyword, ...)
{
	va_list args;
	char *r;

	nullcheck(codec, return NULL);

	if (!dc->option_print)
		return NULL;

	va_start(args, keyword);

	r = dc->option_print(codec, keyword, args);

	va_end(args);

	return r;
}

/**
 * rte_codec_option_menu_get:
 * @codec: Pointer to a #rte_codec returned by rte_codec_get() or
 *	   rte_codec_set().
 * @keyword: Keyword identifying the option, as in #rte_option_info.
 * @entry: A place to store the current menu entry.
 * 
 * Similar to rte_codec_option_get() this function queries the current
 * value of the named option, but returns this value as number of the
 * corresponding menu entry. Naturally this must be an option with
 * menu, else this function will fail.
 * 
 * Return value: 
 * %TRUE on success, otherwise @value remained unchanged.
 **/
rte_bool
rte_codec_option_menu_get(rte_codec *codec, const char *keyword, int *entry)
{
	rte_option_info *oi;
	rte_option_value val;
	rte_bool r;
	int i;

	if (!(oi = rte_codec_option_info_keyword(codec, keyword)))
		return FALSE;

	if (!rte_codec_option_get(codec, keyword, &val))
		return FALSE;

	r = FALSE;

	for (i = 0; i <= oi->max.num; i++) {
		switch (oi->type) {
		case RTE_OPTION_BOOL:
		case RTE_OPTION_INT:
			if (!oi->menu.num)
				return FALSE;
			r = (oi->menu.num[i] == val.num);
			break;

		case RTE_OPTION_REAL:
			if (!oi->menu.dbl)
				return FALSE;
			r = (oi->menu.dbl[i] == val.dbl);
			break;

		case RTE_OPTION_MENU:
			r = (i == val.num);
			break;

		default:
			fprintf(stderr, __PRETTY_FUNCTION__
				": unknown export option type %d\n", oi->type);
			exit(EXIT_FAILURE);
		}

		if (r) {
			*entry = i;
			break;
		}
	}

	return r;
}

/**
 * rte_codec_option_menu_set:
 * @codec: Pointer to a #rte_codec returned by rte_codec_get() or
 *	   rte_codec_set().
 * @keyword: Keyword identifying the option, as in #rte_option_info.
 * @entry: Menu entry to be selected.
 * 
 * Similar to rte_codec_option_set() this function sets the value of
 * the named option, however it does so by number of the corresponding
 * menu entry. Naturally this must be an option with menu, else
 * this function will fail.
 * 
 * Return value: 
 * %TRUE on success, otherwise the option is not changed.
 **/
rte_bool
rte_codec_option_menu_set(rte_codec *codec, const char *keyword, int entry)
{
	rte_option_info *oi;

	if (!(oi = rte_codec_option_info_keyword(codec, keyword)))
		return FALSE;

	if (entry < 0 || entry > oi->max.num)
		return FALSE;

	switch (oi->type) {
	case RTE_OPTION_BOOL:
	case RTE_OPTION_INT:
		if (!oi->menu.num)
			return FALSE;
		return rte_codec_option_set(codec,
			keyword, oi->menu.num[entry]);

	case RTE_OPTION_REAL:
		if (!oi->menu.dbl)
			return FALSE;
		return rte_codec_option_set(codec,
			keyword, oi->menu.dbl[entry]);

	case RTE_OPTION_MENU:
		return rte_codec_option_set(codec, keyword, entry);

	default:
		fprintf(stderr, __PRETTY_FUNCTION__
			": unknown export option type %d\n", oi->type);
		exit(EXIT_FAILURE);
	}
}

/**
 * rte_codec_options_reset:
 * @codec: Pointer to a #rte_codec returned by rte_codec_get() or
 *	   rte_codec_set().
 * 
 * RTE internal function to reset all options of a codec to their
 * default.
 * 
 * Return value: 
 * %TRUE on success.
 **/
rte_bool
rte_codec_options_reset(rte_codec *codec)
{
	rte_option_info *oi;
	rte_bool r;
	int i;

	for (i = 0; (oi = rte_codec_option_info_enum(codec, i)); i++) {
		switch (oi->type) {
		case RTE_OPTION_BOOL:
		case RTE_OPTION_INT:
			if (oi->menu.num)
				r = rte_codec_option_set(codec, oi->keyword,
							 oi->menu.num[oi->def.num]);
			else
				r = rte_codec_option_set(codec, oi->keyword,
							 oi->def.num);
			break;

		case RTE_OPTION_REAL:
			if (oi->menu.dbl)
				r = rte_codec_option_set(codec, oi->keyword,
							 oi->menu.dbl[oi->def.num]);
			else
				r = rte_codec_option_set(codec, oi->keyword, 
							 oi->def.dbl);
			break;

		case RTE_OPTION_STRING:
			if (oi->menu.str)
				r = rte_codec_option_set(codec, oi->keyword,
							 oi->menu.str[oi->def.num]);
			else
				r = rte_codec_option_set(codec, oi->keyword, 
							 oi->def.str);
			break;

		case RTE_OPTION_MENU:
			r = rte_codec_option_set(codec, oi->keyword, oi->def.num);
			break;

		default:
			fprintf(stderr, __PRETTY_FUNCTION__
				": unknown codec option type %d\n", oi->type);
			exit(EXIT_FAILURE);
		}

		if (!r)
			return FALSE;
	}

	return TRUE;
}

rte_option_info *
rte_context_option_info_enum(rte_context *context, int index)
{
	nullcheck(context, return 0);

	if (!xc->option_enum)
		return NULL;

	return xc->option_enum(context, index);
}

rte_option_info *
rte_context_option_info_keyword(rte_context *context, const char *keyword)
{
	rte_option_info *ro;
	int i;

	nullcheck(context, return 0);

	if (!xc->option_enum)
		return NULL;

	for (i = 0;; i++)
	        if (!(ro = xc->option_enum(context, i))
		    || strcmp(keyword, ro->keyword) == 0)
			break;
	return ro;
}

rte_bool
rte_context_option_get(rte_context *context, const char *keyword,
		     rte_option_value *v)
{
	nullcheck(context, return 0);

	if (!xc->option_get)
		return FALSE;

	return xc->option_get(context, keyword, v);
}

rte_bool
rte_context_option_set(rte_context *context, const char *keyword, ...)
{
	va_list args;
	rte_bool r;

	nullcheck(context, return FALSE);

	if (!xc->option_set)
		return FALSE;

	va_start(args, keyword);

	r = xc->option_set(context, keyword, args);

	va_end(args);

	return r;
}

char *
rte_context_option_print(rte_context *context, const char *keyword, ...)
{
	va_list args;
	char *r;

	nullcheck(context, return NULL);

	if (!xc->option_print)
		return NULL;

	va_start(args, keyword);

	r = xc->option_print(context, keyword, args);

	va_end(args);

	return r;
}

/**
 * rte_context_option_menu_get:
 * @context: Initialized #rte_context as returned by rte_context_new().
 * @keyword: Keyword identifying the option, as in #rte_option_info.
 * @entry: A place to store the current menu entry.
 * 
 * Similar to rte_context_option_get() this function queries the current
 * value of the named option, but returns this value as number of the
 * corresponding menu entry. Naturally this must be an option with
 * menu, else this function will fail.
 * 
 * Return value: 
 * %TRUE on success, otherwise @value remained unchanged.
 **/
rte_bool
rte_context_option_menu_get(rte_context *context, const char *keyword, int *entry)
{
	rte_option_info *oi;
	rte_option_value val;
	rte_bool r;
	int i;

	if (!(oi = rte_context_option_info_keyword(context, keyword)))
		return FALSE;

	if (!rte_context_option_get(context, keyword, &val))
		return FALSE;

	r = FALSE;

	for (i = 0; i <= oi->max.num; i++) {
		switch (oi->type) {
		case RTE_OPTION_BOOL:
		case RTE_OPTION_INT:
			if (!oi->menu.num)
				return FALSE;
			r = (oi->menu.num[i] == val.num);
			break;

		case RTE_OPTION_REAL:
			if (!oi->menu.dbl)
				return FALSE;
			r = (oi->menu.dbl[i] == val.dbl);
			break;

		case RTE_OPTION_MENU:
			r = (i == val.num);
			break;

		default:
			fprintf(stderr, __PRETTY_FUNCTION__
				": unknown export option type %d\n", oi->type);
			exit(EXIT_FAILURE);
		}

		if (r) {
			*entry = i;
			break;
		}
	}

	return r;
}

/**
 * rte_context_option_menu_set:
 * @context: Initialized #rte_context as returned by rte_context_new().
 * @keyword: Keyword identifying the option, as in #rte_option_info.
 * @entry: Menu entry to be selected.
 * 
 * Similar to rte_context_option_set() this function sets the value of
 * the named option, however it does so by number of the corresponding
 * menu entry. Naturally this must be an option with menu, else
 * this function will fail.
 * 
 * Return value: 
 * %TRUE on success, otherwise the option is not changed.
 **/
rte_bool
rte_context_option_menu_set(rte_context *context, const char *keyword, int entry)
{
	rte_option_info *oi;

	if (!(oi = rte_context_option_info_keyword(context, keyword)))
		return FALSE;

	if (entry < 0 || entry > oi->max.num)
		return FALSE;

	switch (oi->type) {
	case RTE_OPTION_BOOL:
	case RTE_OPTION_INT:
		if (!oi->menu.num)
			return FALSE;
		return rte_context_option_set(context,
			keyword, oi->menu.num[entry]);

	case RTE_OPTION_REAL:
		if (!oi->menu.dbl)
			return FALSE;
		return rte_context_option_set(context,
			keyword, oi->menu.dbl[entry]);

	case RTE_OPTION_MENU:
		return rte_context_option_set(context, keyword, entry);

	default:
		fprintf(stderr, __PRETTY_FUNCTION__
			": unknown export option type %d\n", oi->type);
		exit(EXIT_FAILURE);
	}
}

/**
 * rte_context_options_reset:
 * @context: Initialized #rte_context as returned by rte_context_new().
 * 
 * RTE internal function to reset all options of a context to their
 * default.
 * 
 * Return value: 
 * %TRUE on success.
 **/
rte_bool
rte_context_options_reset(rte_context *context)
{
	rte_option_info *oi;
	rte_bool r;
	int i;

	for (i = 0; (oi = rte_context_option_info_enum(context, i)); i++) {
		switch (oi->type) {
		case RTE_OPTION_BOOL:
		case RTE_OPTION_INT:
			if (oi->menu.num)
				r = rte_context_option_set(context, oi->keyword,
							   oi->menu.num[oi->def.num]);
			else
				r = rte_context_option_set(context, oi->keyword,
							   oi->def.num);
			break;

		case RTE_OPTION_REAL:
			if (oi->menu.dbl)
				r = rte_context_option_set(context, oi->keyword,
							   oi->menu.dbl[oi->def.num]);
			else
				r = rte_context_option_set(context, oi->keyword,
							   oi->def.dbl);
			break;

		case RTE_OPTION_STRING:
			if (oi->menu.str)
				r = rte_context_option_set(context, oi->keyword,
							   oi->menu.str[oi->def.num]);
			else
				r = rte_context_option_set(context, oi->keyword, 
							   oi->def.str);
			break;

		case RTE_OPTION_MENU:
			r = rte_context_option_set(context, oi->keyword,
						   oi->def.num);
			break;

		default:
			fprintf(stderr, __PRETTY_FUNCTION__
				": unknown context option type %d\n", oi->type);
			exit(EXIT_FAILURE);
		}

		if (!r)
			return FALSE;
	}

	return TRUE;
}

rte_status_info *
rte_context_status_enum(rte_context *context, int n)
{
	nullcheck(context, return NULL);
	nullcheck(xc->status_enum, return NULL);

	return xc->status_enum(context, n);
}

rte_status_info *
rte_context_status_keyword(rte_context *context, const char *keyword)
{
	rte_status_info *si;
	int i;

	nullcheck(context, return NULL);
	nullcheck(xc->status_enum, return NULL);

	for (i = 0;; i++)
	        if (!(si = xc->status_enum(context, i))
		    || strcmp(keyword, si->keyword) == 0)
			break;

	return si;
}

rte_status_info *
rte_codec_status_enum(rte_codec *codec, int n)
{
	nullcheck(codec, return NULL);
	nullcheck(dc->status_enum, return NULL);

	return dc->status_enum(codec, n);
}

rte_status_info *
rte_codec_status_keyword(rte_codec *codec, const char *keyword)
{
	rte_status_info *si;
	int i;

	nullcheck(codec, return NULL);
	nullcheck(dc->status_enum, return NULL);

	for (i = 0;; i++)
	        if (!(si = dc->status_enum(codec, i))
		    || strcmp(keyword, si->keyword) == 0)
			break;

	return si;
}

void
rte_status_free(rte_status_info *status)
{
	nullcheck(status, return);

	if (status->type == RTE_STRING)
	  free(status->val.str);

	free(status);
}

/* uninit all the codecs in the given context */
static void
codecs_uninit(rte_context *context)
{
	static rte_bool uninit_codec(rte_codec *codec,
				     rte_pointer udata) {
		switch (codec->input_mode) {
		case RTE_INPUT_PD:
		{
			/* flush */
			buffer *b = codec->input.pd.last_buffer;
			if (b) {
				b->used = 0; /* EOF */
				send_full_buffer(&codec->prod, b);
			}
		}
			break;
		default:
			break;
		}
		dc->uninit(codec);

		rem_producer(&codec->prod);
		destroy_fifo(&codec->f);

		memset(&codec->input, 0, sizeof(codec->input));

		return TRUE;
	}

	codec_forall(context, uninit_codec, NULL);
	xc->uninit(context);
}

/* Returns the required buffer size for the codec */
static int
buffer_size (rte_codec *codec)
{
	rte_stream_parameters par;
	rte_video_stream_parameters *vid =
		(rte_video_stream_parameters*)&par;

	rte_codec_get_parameters(codec, &par);

	if (dc->public.stream_type == RTE_STREAM_AUDIO)
		return par.audio.fragment_size;
	else if (dc->public.stream_type == RTE_STREAM_SLICED_VBI)
		return 0; /* FIXME */
	/* video */
	switch (vid->pixfmt) {
	case RTE_PIXFMT_YUV420:
		/* Assumes that there's no padding between Y, U, V
		   fields */
		return vid->stride * vid->height +
			vid->uv_stride * vid->height * 2;
	default:
		return vid->stride * vid->height;
	}
}

/* Wait a buffer in callback buffered mode */
static inline void
rte_wait_cb(rte_codec *codec)
{
	producer *p = &(codec->prod);
	rte_buffer rbuf;
	buffer *b;

	codec->input.cb.get(codec->context, codec, &rbuf);

	b = wait_empty_buffer(p);
	b->data = rbuf.data;
	b->used = codec->bsize;
	b->time = rbuf.timestamp;
	b->user_data = rbuf.user_data;
	b->rte_flags |= ~BLANK_BUFFER;

	send_full_buffer(p, b);
}

/* callback data mode */
static inline void
rte_wait_cd(rte_codec *codec)
{
	producer *p = &(codec->prod);
	buffer *b;

	b = wait_empty_buffer(p);
	codec->input.cd.get(codec->context, codec, b->data, &(b->time));
	b->rte_flags |= ~BLANK_BUFFER;
	b->used = codec->bsize;

	send_full_buffer(p, b);
}

/* push (buffer or data, it's the same at this point) */
static inline void
rte_wait_push(rte_codec *codec)
{
	/* FIXME: To do */
	/*
	  Callback modes can be joined (in stop()) without problems
	  since callbacks will get the needed data for the codecs to
	  complete.
	  In push modes, when stop() is called, no more data might reach
	  the codec, so we should fake a few frames for the codec to
	  finish. We mark these as BLANK_BUFFER, indicating that
	  they shouldn't be unref'ed.
	 */
}

static void
rte_wait_full (fifo *f)
{
	rte_codec *codec = (rte_codec*)f->user_data;

	switch (codec->input_mode) {
	case RTE_INPUT_CB:
		rte_wait_cb(codec);
		break;
	case RTE_INPUT_CD:
		rte_wait_cd(codec);
		break;
	case RTE_INPUT_PB:
	case RTE_INPUT_PD:
		rte_wait_push(codec);
		break;
	default:
		assert(!"reached");
		break;
	}
}

static void
rte_send_empty (consumer *c, buffer *b)
{
	rte_codec *codec = (rte_codec*)c->fifo->user_data;
	rte_buffer rbuf;

	if (!(b->rte_flags & BLANK_BUFFER)) {
		rbuf.data = b->data;
		rbuf.timestamp = b->time;
		rbuf.user_data = b->user_data;

		switch (codec->input_mode) {
		case RTE_INPUT_PB:
			codec->input.pb.unref(codec->context, codec,
					      &rbuf);
			break;
		case RTE_INPUT_CB:
			codec->input.cb.unref(codec->context, codec,
					      &rbuf);
			break;
		default:
			/* Nothing to be done for unbuffered modes */
			break;
		}
	}

	/* XXX temporary hack */
	send_empty_buffered(c, b);
}

/* init one codec */
static rte_bool
init_codec (rte_codec *codec, rte_pointer udata)
{
	rte_context *context = (rte_context*)udata;
	int buffers;
	int alloc_bytes = 0;
	int bsize = codec->bsize = buffer_size(codec);
	rte_bool retval = dc->pre_init(codec, &buffers);
	
	if (!retval) {
		rte_error(context, "Cannot pre_init %s",
			  dc->public.keyword);
		return FALSE;
	}
	
	switch (codec->input_mode) {
	case RTE_INPUT_PD:
		codec->input.pd.last_buffer = NULL;
	case RTE_INPUT_CD:
		alloc_bytes = bsize;
		break;
	default:
		alloc_bytes = 0;
		break;
	}

	if (buffers != init_callback_fifo
	    (&codec->f, dc->public.keyword, NULL, NULL,
	     rte_wait_full, rte_send_empty, buffers,
	     alloc_bytes)) {
		rte_error(context, "not enough mem");
		return FALSE;
	}

	codec->f.user_data = codec;

	retval = dc->post_init(codec);

	assert(add_producer(&codec->f, &codec->prod));

	if (!retval) {
		rem_producer(&codec->prod);
		destroy_fifo(&codec->f);
		rte_error(context, "Cannot post_init %s",
			  dc->public.keyword);
		return FALSE;
	}

	return retval;
}

rte_bool
rte_start(rte_context *context)
{
	rte_bool result;
	int num_codecs = 0;
	char *failed_codec = NULL;

	/* check that all selected codecs are in ready state and count
	 selected codecs */
	static rte_bool check_ready (rte_codec *codec, rte_pointer udata) {
		if (codec->status != RTE_STATUS_READY) {
			failed_codec = dc->public.keyword;
			return FALSE; /* Stop forall */
		}
		num_codecs++;
		return TRUE;
	}

	nullcheck(context, return FALSE);

	if (context->status != RTE_STATUS_READY) {
		if (context->status == RTE_STATUS_RUNNING)
			rte_error(context, "Already encoding!");
		else if (context->status == RTE_STATUS_PAUSED)
			rte_error(context, "Paused, use rte_resume");
		else
			rte_error(context,
				  "You must context_set_output first");
		return FALSE;
	}

	if (!codec_forall(context, check_ready, NULL)) {
		rte_error(context, "Codec %s isn't ready to encode",
			  failed_codec);
		return FALSE;
	}

	if (!num_codecs) {
		rte_error(context, "No codecs set");
		return FALSE;
	}

	if (!xc->pre_init(context))
		return FALSE;

	if (!codec_forall(context, init_codec, NULL))
		return FALSE;

	result = xc->start(context);

	if (result)
		context->status = RTE_STATUS_RUNNING;
	else
		codecs_uninit(context);

	return result;
}

void
rte_stop(rte_context *context)
{
	nullcheck(context, return);

	if (context->status < RTE_STATUS_RUNNING) {
		rte_error(context, "Not running!!");
		return;
	}

	xc->stop(context);

	codecs_uninit(context);

	context->status = RTE_STATUS_READY;
}

void
rte_pause(rte_context *context)
{
	nullcheck(context, return);

	if (context->status != RTE_STATUS_RUNNING) {
		rte_error(context, "Not running!!");
		return;
	}

	/* FIXME: to do */

	context->status = RTE_STATUS_PAUSED;
}

rte_bool
rte_resume(rte_context *context)
{
	nullcheck(context, return FALSE);

	if (context->status != RTE_STATUS_PAUSED) {
		rte_error(context, "Not paused!!");
		return FALSE;
	}

	/* FIXME: to do */

	context->status = RTE_STATUS_RUNNING;

	return TRUE;
}

char *
rte_last_error(rte_context *context)
{
	nullcheck(context, return NULL);

	return context->error;
}
