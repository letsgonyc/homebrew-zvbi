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

#define ZCONF_DOMAIN "/zapping/plugins/"
#include "zconf.h"
#include "plugins.h"
#include "properties.h"
#include "globals.h"

/* This is the main plugin list, it is only used in plugin_bridge */
extern GList * plugin_list;

/* This shouldn't be public */
static void plugin_foreach_free(struct plugin_info * info, void *
				user_data);

static gpointer
get_symbol_or_null		(struct plugin_info *	info,
				 const gchar *		name,
				 gint			hash)
{
	gpointer p;

	if (info->get_symbol (name, hash, &p))
		return p;
	else
		return NULL;
}

/* Loads a plugin, returns TRUE if the plugin seems usable and FALSE
   in case of error. Shows an error box describing the error in case
   of error and the given structure is filled in on success.
   file_name: name of the plugin to load "/lib/plugin.so", "libm.so", z.b.
   info: Structure that holds all the info about the plugin, and will
         be passed to the rest of functions in this library.
*/
static gboolean plugin_load(gchar * file_name, struct plugin_info * info)
{
  /* This variables are for querying the symbols */
  gint i = 0;
  gchar *symbol, *type, *description;
  gpointer ptr;
  gint hash;
  /* This is used to get the canonical name */
  gchar * canonical_name;
  gchar * version;
  gint (*plugin_get_protocol)(void) = NULL;

  g_assert(info != NULL);
  g_assert(file_name != NULL);

  CLEAR (*info);

  info -> handle = g_module_open (file_name, 0);

  if (!info -> handle)
    {
      g_warning("Failed to load plugin: %s", g_module_error());
      return FALSE;
    }

  /* Check that the protocols we speak are the same */
  if (!g_module_symbol(info -> handle, "plugin_get_protocol",
		       (gpointer*)&(plugin_get_protocol)))
    {
      g_module_close(info->handle);
      return FALSE;
    }

  if (((*plugin_get_protocol)()) != PLUGIN_PROTOCOL)
    {
      g_warning("While loading %s\n"
		"The plugin uses the protocol %x, and the current"
		" one is %x, it cannot be loaded.",
		file_name,
		(*plugin_get_protocol)(), PLUGIN_PROTOCOL);
      g_module_close(info->handle);
      return FALSE;
    }

  /* Get the other symbol */
  if (!g_module_symbol (info->handle, "plugin_get_symbol",
			(gpointer *) &info->get_symbol))
    {
      g_warning("%s", g_module_error());
      g_module_close(info->handle);
      return FALSE;
    }

  /* plugin_get_info is the only compulsory symbol that must be
     present in the plugin's table of symbols */
  if (!info->get_symbol("plugin_get_info", 0x1234,
			    (gpointer*)&(info->plugin_get_info)))
    {
      g_warning("plugin_get_info was not found in %s",
		file_name);
      g_module_close(info->handle);
      return FALSE;
    }

  /* Get the remaining symbols */
  info->plugin_init = get_symbol_or_null (info, "plugin_init", 0x1234);
  info->plugin_close = get_symbol_or_null (info, "plugin_close", 0x1234);
  info->plugin_start = get_symbol_or_null (info, "plugin_start", 0x1234);
  info->plugin_stop = get_symbol_or_null (info, "plugin_stop", 0x1234);
  info->plugin_load_config =
	  get_symbol_or_null (info, "plugin_load_config", 0x1234);
  info->plugin_save_config =
	  get_symbol_or_null (info, "plugin_save_config", 0x1234);
  info->plugin_running = get_symbol_or_null (info, "plugin_running", 0x1234);
  info->plugin_read_frame =
	  get_symbol_or_null (info, "plugin_read_frame", 0x1234);
  info->plugin_capture_start =
	  get_symbol_or_null (info, "plugin_capture_start", 0x1234);
  info->plugin_capture_stop =
	  get_symbol_or_null (info, "plugin_capture_stop", 0x1234);
  info->plugin_get_public_info =
	  get_symbol_or_null (info, "plugin_get_public_info", 0x1234);
  info->plugin_add_gui =
	  get_symbol_or_null (info, "plugin_add_gui", 0x1234);
  info->plugin_remove_gui =
	  get_symbol_or_null (info, "plugin_remove_gui", 0x1234);

  /* Check that the two functions are present */
  if ((!info->plugin_add_gui) ||
      (!info->plugin_remove_gui))
    info -> plugin_add_gui = info -> plugin_remove_gui = NULL;

  info->plugin_process_popup_menu =
	  get_symbol_or_null (info, "plugin_process_popup_menu", 0x1234);

  info->plugin_get_misc_info =
	  get_symbol_or_null (info, "plugin_get_misc_info", 0x1234);

  CLEAR (info->misc_info);

  if (info -> plugin_get_misc_info)
    memcpy(&(info->misc_info), (*info->plugin_get_misc_info)(),
	   ((*info->plugin_get_misc_info)())->size);

  plugin_get_info(&canonical_name, NULL, NULL, NULL, NULL, NULL,
		   info);
  if ((!canonical_name) || (strlen(canonical_name) == 0))
    {
      g_module_close(info->handle);
      g_warning("\"%s\" seems to be a valid plugin, but it doesn't "
		"provide a canonical name.", file_name);
      return FALSE;
    }

  /* Get the version of the plugin */
  version = plugin_get_version(info);
  if (!version)
    {
      g_module_close(info->handle);
      g_warning(_("\"%s\" doesn't provide a version"),
		file_name);
      return FALSE;
    }

  info -> major = info->minor = info->micro = 0;
  if (sscanf(version, "%d.%d.%d", &(info->major), &(info->minor),
      &(info->micro)) == 0)
    {
      g_warning(
	"Sorry, the version of the plugin cannot be parsed.\n"
	"The version must be something like %%d[.%%d[%%d[other things]]]\n"
	"The given version is %s.\nError loading \"%s\" (%s)",
      version, file_name, canonical_name);
      g_module_close(info -> handle);
      return FALSE;
    }

  info -> canonical_name = g_strdup(canonical_name);
  info -> file_name = g_strdup(file_name);

  /* Get the exported symbols for the plugin */
  info -> exported_symbols = NULL;
  info -> num_exported_symbols = 0;
  if (info->plugin_get_public_info)
    while ((*info->plugin_get_public_info)(i, &ptr, &symbol, 
					   &description, &type, &hash))
      {
	if ((!symbol) || (!type) || (!ptr) || (hash == -1))
	  {
	    i++;
	    continue;
	  }
	 info->exported_symbols = (struct plugin_exported_symbol*)
	   realloc(info->exported_symbols,
		   sizeof(struct plugin_exported_symbol)*(i+1));
	 if (!info->exported_symbols)
	  g_error(_("Insufficient memory"));
	
	info->exported_symbols[i].symbol = g_strdup(symbol);
 	info->exported_symbols[i].type = g_strdup(type);
	if (description)
	  info->exported_symbols[i].description =
	    g_strdup(description);
	else
	  info->exported_symbols[i].description =
	    g_strdup(_("No description available"));
	info->exported_symbols[i].ptr = ptr;
	info->exported_symbols[i].hash = hash;
	info->num_exported_symbols++;
	i++;
      }

  return TRUE;
}

void plugin_unload(struct plugin_info * info)
{
  g_assert(info != NULL);

  /* Tell the plugin to close itself */
  plugin_close( info );

  /* Free the memory of the exported symbols */
  if (info -> num_exported_symbols > 0)
    {
      g_free(info -> exported_symbols);
      info -> exported_symbols = NULL;
    }
  g_free(info -> file_name);
  g_free(info -> canonical_name);
  g_module_close (info -> handle);
}

struct plugin_info *
plugin_by_name			(const gchar *		name)
{
  GList *glist;

  g_return_val_if_fail (NULL != name, NULL);

  for (glist = g_list_first (plugin_list); glist; glist = glist->next)
    {
      struct plugin_info *info;

      info = (struct plugin_info *) glist->data;

      if (0 == strcmp (info->canonical_name, name))
	return info;
    }

  return NULL;
}

gpointer
plugin_symbol			(const struct plugin_info *info,
				 const gchar *		name)
{
  gpointer ptr;

  g_return_val_if_fail (NULL != info, NULL);
  g_return_val_if_fail (NULL != name, NULL);

  if (!info->get_symbol (name, 0x1234, &ptr))
    return NULL;

  return ptr;
}

/* FIXME: This is ancient, remote.h is much nicer */
/*
  This is the bridge given to the plugins.
*/
static gboolean plugin_bridge (gpointer * ptr, gchar * plugin, gchar *
			       symbol, gchar * type, gint hash)
{
  gint i;
  GList * list = g_list_first(plugin_list); /* From main.c */
  struct plugin_info * info;
  struct plugin_exported_symbol * es = NULL; /* A pointer to the table of
						exported symbols */
  gint num_exported_symbols=0; /* Number of exported symbols in the
				plugin */

  if (!plugin)
    {
      if (ptr)
	*ptr = GINT_TO_POINTER(0x2);
      return FALSE; /* Zapping exports no symbols */
    }
  else /* We have to query the list of plugins */
    while (list)
      {
	info = (struct plugin_info*) list->data;
	if (!strcasecmp(info->canonical_name, plugin))
	  {
	    es = info->exported_symbols;
	    num_exported_symbols = info->num_exported_symbols;
	    break;
	  }
	list = list->next;
      }

  if (!es)
    {
      if (ptr)
	*ptr = GINT_TO_POINTER(0x1); /* Plugin not found */
      return FALSE;
    }

  /* Now try to find the given symbol in the table of exported symbols
   of the plugin */
  for (i=0; i<num_exported_symbols; i++)
    if (!strcmp(es[i].symbol, symbol))
      {
	if (es[i].hash != hash)
	  {
	    if (ptr)
	      *ptr = GINT_TO_POINTER(0x3);
	    /* Warn */
	    g_warning(_("Check error: \"%s\" in plugin %s"
			" was supposed to be \"%s\" but it is:"
			"\"%s\". Hashes are 0x%x vs. 0x%x"), symbol,
		      plugin ? plugin : "Zapping", type,
		      es[i].type, hash, es[i].hash);
	    return FALSE;
	  }
	if (ptr)
	  *ptr = es[i].ptr;
	return TRUE; /* Success */
      }

  if (ptr)
    *ptr = GINT_TO_POINTER(0x2); /* Symbol not found in the plugin */
  return FALSE;
}

/* This are wrappers to avoid the use of the pointers in the
   plugin_info struct just in case somewhen the plugin system changes */
gint plugin_protocol(struct plugin_info * info)
{
  g_assert(info != NULL);

  return PLUGIN_PROTOCOL;
}

gboolean plugin_init ( tveng_device_info * device_info,
			      struct plugin_info * info )
{
  g_assert(info != NULL);

  printv("Initialize plugin %s %d.%d.%d\n",
	 info->file_name,
	 info->major,
	 info->minor,
	 info->micro);

  if (!info->plugin_init)
    return TRUE;

  return (((*info->plugin_init)((PluginBridge)plugin_bridge,
			      device_info)));
}

void plugin_close(struct plugin_info * info)
{
  g_assert(info != NULL);

  if (!info->plugin_close)
    return;

  ((*info->plugin_close))();
}

gboolean plugin_start (struct plugin_info * info)
{
  g_assert(info != NULL);

  if (!info->plugin_start)
    return TRUE;

  return ((*info->plugin_start))();
}

void plugin_stop (struct plugin_info * info)
{
  g_assert(info != NULL);

  if (!info->plugin_stop)
    return;

  ((*info->plugin_stop))();
}

void plugin_load_config(struct plugin_info * info)
{
  gchar * key = NULL;
  gchar * buffer = NULL;
  g_assert(info != NULL);

  if (!info->plugin_load_config)
    return;

  plugin_get_info (&buffer, NULL, NULL, NULL, NULL, NULL, info);
  key = g_strconcat("/zapping/plugins/", buffer, "/", NULL);
  ((*info->plugin_load_config))(key);
  g_free ( key );
}

void plugin_save_config(struct plugin_info * info)
{
  gchar * key = NULL;
  gchar * buffer = NULL;
  g_assert(info != NULL);

  if (!info->plugin_save_config)
    return;

  plugin_get_info (&buffer, NULL, NULL, NULL, NULL, NULL, info);
  key = g_strconcat("/zapping/plugins/", buffer, "/", NULL);
  ((*info->plugin_save_config))(key);
  g_free ( key );
}

void plugin_get_info(gchar ** canonical_name, gchar **
		      descriptive_name, gchar ** description, gchar
		      ** short_description, gchar ** author, gchar **
		      version, struct plugin_info * info)
{
  g_assert(info != NULL);

  ((*info->plugin_get_info))(canonical_name, descriptive_name,
			     description, short_description, author,
			     version);
}

/* These functions are more convenient when accessing some individual
   fields of the plugin's info */
gchar * plugin_get_canonical_name (struct plugin_info * info)
{
  g_assert(info != NULL);

  return (info -> canonical_name);
}

gchar * plugin_get_name (struct plugin_info * info)
{
  gchar * buffer;
  g_assert(info != NULL);

  plugin_get_info(NULL, &buffer, NULL, NULL, NULL, NULL, info);
  return buffer;
}

gchar * plugin_get_description (struct plugin_info * info)
{
  gchar * buffer;
  g_assert(info != NULL);
  plugin_get_info(NULL, NULL, &buffer, NULL, NULL, NULL, info);
  return buffer;
}

gchar * plugin_get_short_description (struct plugin_info * info)
{
  gchar * buffer;
  g_assert(info != NULL);
  plugin_get_info(NULL, NULL, NULL, &buffer, NULL, NULL, info);
  return buffer;
}

gchar * plugin_get_author (struct plugin_info * info)
{
  gchar * buffer;
  g_assert(info != NULL);
  plugin_get_info(NULL, NULL, NULL, NULL, &buffer, NULL, info);
  return buffer;
}

gchar * plugin_get_version (struct plugin_info * info)
{
  gchar * buffer;
  g_assert(info != NULL);
  plugin_get_info(NULL, NULL, NULL, NULL, NULL, &buffer, info);
  return buffer;
}

gboolean plugin_running ( struct plugin_info * info)
{
  g_assert(info != NULL);

  if (!info->plugin_running)
    return FALSE; /* If the plugin doesn't care, we shouldn't either */

  return (*(info->plugin_running))();
}

void plugin_read_frame (capture_frame * frame,
			struct plugin_info * info)
{
  g_assert(info != NULL);
  g_return_if_fail(frame != NULL);

  if (info -> plugin_read_frame)
    (*info->plugin_read_frame)(frame);
}

void plugin_capture_start (struct plugin_info * info)
{
  g_assert(info != NULL);

  if (info -> plugin_capture_start)
    (*info->plugin_capture_start)();
}

void plugin_capture_stop (struct plugin_info * info)
{
  g_assert(info != NULL);

  if (info -> plugin_capture_stop)
    (*info->plugin_capture_stop)();
}

void plugin_add_gui (GnomeApp * app, struct plugin_info * info)
{
  g_assert(info != NULL);
  g_assert(app != NULL);

  if (info -> plugin_add_gui)
    ((*info->plugin_add_gui)(app));
}

void plugin_remove_gui (GnomeApp * app, struct plugin_info * info)
{
  g_assert(info != NULL);
  g_assert(app != NULL);

  if (info -> plugin_remove_gui)
    return ((*info->plugin_remove_gui)(app));
}

gint plugin_get_priority (struct plugin_info * info)
{
  g_assert(info != NULL);

  return info -> misc_info.plugin_priority;
}

void plugin_process_popup_menu (GtkWidget	*widget,
				GdkEventButton	*event,
				GtkMenu	*popup,
				struct plugin_info *info)
{
  g_assert(info != NULL);

  if (info->plugin_process_popup_menu)
    (*info->plugin_process_popup_menu)(widget, event, popup);
}

/* Loads all the valid plugins in the given directory, and appends them to
   the given GList. It returns the new GList. The plugins should
   contain exp in their filename (usually called with exp = .zapping.so) */
static GList * plugin_load_plugins_in_dir( const gchar * directory,
					   const gchar * exp,
					   GList * old )
{
  struct plugin_info plug;
  GError *error = NULL;
  GDir *dir;
  const gchar *name;

  g_assert(exp != NULL);
  g_assert(directory != NULL);
  g_assert(strlen(directory) > 0);

  printv("looking for plugins in %s\n", directory);

  dir = g_dir_open (directory, /* flags */ 0, &error);

  if (!dir || error)
    {
      g_assert (!dir);

      if (error)
	{
	  printv ("Cannot open directory '%s': %s\n",
		  directory, error->message);
	  g_error_free (error);
	  error = NULL;
	}

      return old;
    }

  while ((name = g_dir_read_name (dir)))
    {
      gchar *filename;
      GList *p;
      struct plugin_info *new_plugin;

      if (0 != strcmp (name + strlen (name) - strlen (exp), exp))
	continue;

      filename = g_build_filename (directory, name, NULL);

      printv("loading plugin %s\n", filename);

      if (!plugin_load(filename, &plug))
	{
	plugin_load_error:
	  g_free(filename);
	  continue;
	}

      /* Check whether there is no other other plugin with the same
	 canonical name */

      for (p = g_list_first (old); p; p = p->next)
	{
	  new_plugin = (struct plugin_info *) p->data;

	  if (0 == g_ascii_strcasecmp (new_plugin->canonical_name,
				       plug.canonical_name))
	    {
	      /* Collision, load the highest version number */
	      if (new_plugin->major > plug.major)
		{
		  goto plugin_load_error;
		}
	      else if (new_plugin->major == plug.major)
		{
		  if (new_plugin->minor > plug.minor)
		    {
		      goto plugin_load_error;
		    }
		  else if (new_plugin->minor == plug.minor)
		    {
		      if (new_plugin->micro >= plug.micro)
			goto plugin_load_error;
		    }
		}

	      /* Replace the old one with the new one, just delete the
		 old one */
	      old = g_list_remove (old, new_plugin);
	      plugin_unload (new_plugin);
	      free (new_plugin);
	      break; /* There is no need to continue querying */
	    }
	}

      g_free (filename);

      /* This plugin is valid, copy it and add it to the GList */
      new_plugin = malloc (sizeof (*new_plugin));
      if (!new_plugin)
	{
	  perror("malloc");
	  plugin_unload(&plug);
	  continue;
	}

      *new_plugin = plug;
      
      old = g_list_append(old, new_plugin);
    }

  g_dir_close (dir);

  return old;
}

/* This is the function used to sort the plugins by priority */
static gint plugin_sorter (struct plugin_info * a, struct plugin_info * b)
{
  g_assert(a != NULL);
  g_assert(b != NULL);

  return (b->misc_info.plugin_priority - a->misc_info.plugin_priority);
}

/* Loads all the plugins in the system */
GList * plugin_load_plugins ( void )
{
  gchar * plugin_path; /* Path to the plugins */
  const gchar *dir;
  gchar **plugin_dirs;
  GList * list = NULL;
  gint i;

  /* First load plugins in the home dir */
  dir = g_get_home_dir();
  if (dir)
    {
      g_assert(strlen(dir) > 0);
      if (dir[strlen(dir)-1] == '/')
	plugin_path = g_strconcat(dir, ".zapping/plugins", NULL);
      else
	plugin_path = g_strconcat(dir, "/.zapping/plugins", NULL);
      list = plugin_load_plugins_in_dir (plugin_path, PLUGIN_STRID,
					 list);
      g_free( plugin_path );
    }

  /* Load plugins in other directories */
  zcc_char ("",
	    "Colon separated list of dirs to be searched for plugins",
	    "plugin_dirs");
  plugin_dirs = g_strsplit(zcg_char (NULL, "plugin_dirs"), ":", 0);
  if (plugin_dirs)
    {
      for (i=0; plugin_dirs[i]; i++)
	list = plugin_load_plugins_in_dir (plugin_dirs[i],
					   PLUGIN_STRID, list);
      g_strfreev (plugin_dirs);
    }

  /* $(prefix)/lib/zapping/plugins */
  dir = PACKAGE_LIB_DIR "/plugins";
  list = plugin_load_plugins_in_dir (dir, PLUGIN_STRID, list);

  list = g_list_sort (list, (GCompareFunc) plugin_sorter);

  return list;
}

/* This function is called from g_list_foreach */
static void plugin_foreach_free(struct plugin_info * info,
				void * user_data _unused_)
{
  plugin_save_config(info);
  plugin_unload(info);
  free(info);
}

/* Unloads all the plugins loaded in the GList */
void plugin_unload_plugins(GList * list)
{
  g_list_foreach ( list, (GFunc) plugin_foreach_free, NULL );
  g_list_free ( list );
}

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
