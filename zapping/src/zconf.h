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

#ifndef __ZCONF_H__
#define __ZCONF_H__

#include <gnome.h> /* This file depends on Gnome and glib */

#include <stdio.h>

G_BEGIN_DECLS

/* Possible key types */
enum zconf_type
{
  ZCONF_TYPE_NONE, /* Indicates failure, a key should never have this
		      type */
  ZCONF_TYPE_DIR, /* This key holds no actual value, just subdirs */
  ZCONF_TYPE_INTEGER,
  ZCONF_TYPE_STRING,
  ZCONF_TYPE_FLOAT,
  ZCONF_TYPE_BOOLEAN
};

/* ZConf initing and closing */
/* 
   Starts zconf, and returns FALSE on error.
   domain: Specifies the domain where the config will be saved under
   (usually it's equal to the program name).
*/
gboolean zconf_init(const gchar * domain);

/*
  Closes ZConf, returns FALSE if zconf could not be closed properly
  (usually because it couldn't write the config to disk)
*/
gboolean zconf_close(void);

/*
  Returns the last error zconf generated, 0 means the last operation
  was ok. It is useful for checking ambiguous return values:
  if ((!zconf_get_int(...)) && zconf_error())
     Show_Error_Message();
  else
     Show_Success_Message();
*/
gint zconf_error(void);

/* 
   In zconf there are three operations you can perform for each value:
   get: Gets the key value and fails if it doesn't exist.
   set: Sets a key value, and creates it (undocumented) if doesn't exist. 
   Never fails.
   create: If a key doesn't exist, it creates it with this value,
   if it already exists, it does nothing. Never fails.
   The paths are standard Unix filenames, relative to the domain or
   root dir:
   If the domain is "zapping", paths should be like
    plugins/png_save/interlaced
*/
/*
  Gets an integer value, returns 0 on error (ambiguous). If where
  is not NULL, the value is also stored in the location pointed to by
  where.
*/
gint zconf_get_int(gint * where, const gchar * path);
/*
  Sets an integer value
*/
void zconf_set_int(gint new_value, const gchar * path);
/*
  Sets a default integer value. desc is the description for the
  value. Set it to NULL for leaving it undocumented (a empty string is
  considered as a documented value)
*/
void zconf_create_int(gint new_value, const gchar * desc,
			  const gchar * path);

guint zconf_get_uint(guint * where, const gchar * path);
void zconf_set_uint(guint new_value, const gchar * path);
void zconf_create_uint(guint new_value, const gchar * desc,
		       const gchar * path);

/*
  Gets a string value. The returned string is statically allocated,
  will only be valid until the next call to zconf, so use g_strdup()
  to duplicate it if needed.
  Returns NULL on error (not ambiguous, NULL is always an error).
  if where is not NULL, zconf will g_strdup the string itself, and
  place a pointer to it in where, that should be freed later with g_free.
*/
const gchar * zconf_get_string(gchar ** where, const gchar * path);

/*
  Sets an string value to the given string.
*/
void zconf_set_string(const gchar * new_value, const gchar * path);

/*
  Creates an string value. Sets desc to NULL to leave it
  undocumented.
*/
void zconf_create_string(const gchar * value, const gchar * desc,
			 const gchar * path);

/*
  Gets a boolean value. If where is not NULL, the value is also stored
  there. Returns FALSE on error (ambiguous, use zconf_error to check).
*/
gboolean zconf_get_boolean(gboolean * where, const gchar * path);

/*
  Sets a boolean value.
*/
void zconf_set_boolean(gboolean new_value, const gchar * path);

/*
  Creates a boolean key. Cannot fail.
*/
void zconf_create_boolean(gboolean new_value, const gchar * desc,
			  const gchar * path);

/*
  Gets a float value. If where is not NULL, the value is also stored
  there. Returns 0.0 on error (ambiguous, use zconf_error to check).
*/
gfloat zconf_get_float(gfloat * where, const gchar * path);

/*
  Sets a floating point number. Cannot fail.
*/
void zconf_set_float(gfloat new_value, const gchar * path);

/*
  Creates a float key. Cannot fail.
*/
void zconf_create_float(gfloat new_value, const gchar * desc,
			const gchar * path);

gboolean
zconf_copy			(const gchar *		dst,
				 const gchar *		src);

/*
  Documentation functions.
*/
/*
  Returns the string that describes the given key, NULL if the key is
  undocumented (ambiguous, in case of the key not being found, NULL is
  returned, and zconf_error will return non-zero). If where is not
  NULL, the string will also be stored there (after g_strdup()'ing it)
*/
const gchar * zconf_get_description(gchar ** where, const gchar * path);

/*
  Sets the string that describes a key.
  Returns FALSE if the key could not be found.
*/
gboolean zconf_set_description(const gchar * desc, const gchar * path);

/*
  Value and type querying and erasing functions.
*/
/*
  Gets the name of the nth entry in the subdirectory. Call it starting
  from 0 and incrementing one by one until it gives an error (returns
  NULL) to query the contents of a key (a key is a directory AND a
  file). The returned pointer is a pointer to a statically allocated
  string, and it will only be valid until the next zconf call. if
  where is not NULL, a new copy will be g_strdup()'ed there.
*/
const gchar * zconf_get_nth(guint nth, gchar ** where, const gchar * path);

/*
  Removes a key from the database. All the keys descendant from this
  one will be erased too. Fails if it cannot find the given key.
*/
gboolean zconf_delete(const gchar * path);

/*
  Returns the type of the given key. It fails if the key doesn't
  exist. Failure is indicated by a return value of ZCONF_TYPE_NONE.
*/
enum zconf_type zconf_get_type(const gchar * key);

/*
  Translates the given key type to a string. This string is statically
  allocated, it may be overwritten the next time you call any zconf
  function. Always succeeds (returns [Unknown] if the type is unknown :-)
*/
const gchar *
zconf_type_string(enum zconf_type type);

/*
  zconf_set_type doesn't exist, if you wish to change the type of a
  key, you must zconf_set_[type] it, assigning thus a valid value to
  it. The old key, whatever its type and content was, is erased. Its
  description is kept untouched.
*/

/*
  Hooks handling. Any client can put a hook into a given key, it will
  be called when the value of the key is changed.
  new_value is a *pointer* to the contents.
  That is, if you are monitoring an int, you get the actual value by
  doing *((gint*)new_value_ptr), for a string (gchar*)new_value_ptr,
  etc.
  The pointer will only be valid during the callback.
*/

typedef void (*ZConfHook) ( const gchar * key,
			    gpointer new_value_ptr,
			    gpointer data );

/*
  key: The key we hook to.
  hook: The routine to be called.
  data: data to be passed to the hook.
*/
void
zconf_add_hook(const gchar * key, ZConfHook hook, gpointer data);

/*
  Add a hook that will be automatically removed when object is destroyed.
  object: Object to connect to.
  key: The key we hook to.
  hook: The routine to be called.
  data: data to be passed to the hook.
  wakeup: call routine once in advance.
*/
void
zconf_add_hook_while_alive(GObject *object,
			   const gchar * key, ZConfHook hook,
			   gpointer data, gboolean wakeup);

/*
  Removes the given hook from the key.
  key: the key to remove the hook from.
  hook: The hook function to remove
  data: the data passed to that function
*/
void
zconf_remove_hook(const gchar * key, ZConfHook hook, gpointer data);

/*
  Touches the key, triggering any callbacks without modifying the key
*/
void
zconf_touch(const gchar * key);

#ifdef ZCONF_DOMAIN
#define zcs_int(var, key) \
zconf_set_int(var,  ZCONF_DOMAIN key)
#define zcg_int(where, key) \
zconf_get_int(where, ZCONF_DOMAIN key)
#define zcc_int(value, desc, key) \
zconf_create_int(value, desc, ZCONF_DOMAIN key)

#define zcs_uint(var, key) \
zconf_set_uint(var,  ZCONF_DOMAIN key)
#define zcg_uint(where, key) \
zconf_get_uint(where, ZCONF_DOMAIN key)
#define zcc_uint(value, desc, key) \
zconf_create_uint(value, desc, ZCONF_DOMAIN key)

#define zcs_float(var, key) \
zconf_set_float(var,  ZCONF_DOMAIN key)
#define zcg_float(where, key) \
zconf_get_float(where, ZCONF_DOMAIN key)
#define zcc_float(value, desc, key) \
zconf_create_float(value, desc, ZCONF_DOMAIN key)

#define zcs_char(var, key) \
zconf_set_string(var,  ZCONF_DOMAIN key)
#define zcg_char(where, key) \
zconf_get_string(where, ZCONF_DOMAIN key)
#define zcc_char(value, desc, key) \
zconf_create_string(value, desc, ZCONF_DOMAIN key)

#define zcs_bool(var, key) \
zconf_set_boolean(var,  ZCONF_DOMAIN key)
#define zcg_bool(where, key) \
zconf_get_boolean(where, ZCONF_DOMAIN key)
#define zcc_bool(value, desc, key) \
zconf_create_boolean(value, desc, ZCONF_DOMAIN key)

#define zcs_z_key(var, key) \
zconf_set_z_key(var, ZCONF_DOMAIN key)
#define zcg_z_key(where, key) \
zconf_get_z_key(where, ZCONF_DOMAIN key)
#define zcc_z_key(value, desc, key) \
zconf_create_z_key(value, desc, ZCONF_DOMAIN key)
#endif /* ZCONF_DOMAIN */

/* Generic hooks */

extern void
zconf_hook_widget_show		(const gchar *		key,
				 gpointer		new_value_ptr,
				 gpointer		user_data);
extern void
zconf_hook_toggle_button	(const gchar *		key,
				 gpointer		new_value_ptr,
				 gpointer		user_data);
extern void
zconf_hook_check_menu		(const gchar *		key,
				 gpointer		new_value_ptr,
				 gpointer		user_data);

G_END_DECLS

#endif /* ZCONF.H */

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
