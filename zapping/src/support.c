/*
 * DO NOT EDIT THIS FILE - it is generated by Glade.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gnome.h>

#include "support.h"

/* This is an internally used function to create pixmaps. */
static GtkWidget* create_dummy_pixmap  (GtkWidget       *widget,
                                        gboolean         gnome_pixmap);

GtkWidget*
lookup_widget                          (GtkWidget       *widget,
                                        const gchar     *widget_name)
{
  GtkWidget *parent, *found_widget;

  for (;;)
    {
      if (GTK_IS_MENU (widget))
        parent = gtk_menu_get_attach_widget (GTK_MENU (widget));
      else
        parent = widget->parent;
      if (parent == NULL)
        break;
      widget = parent;
    }

  found_widget = (GtkWidget*) gtk_object_get_data (GTK_OBJECT (widget),
                                                   widget_name);
  if (!found_widget)
    g_warning ("Widget not found: %s", widget_name);
  return found_widget;
}

/* This is a dummy pixmap we use when a pixmap can't be found. */
static char *dummy_pixmap_xpm[] = {
/* columns rows colors chars-per-pixel */
"1 1 1 1",
"  c None",
/* pixels */
" ",
" "
};

/* This is an internally used function to create pixmaps. */
static GtkWidget*
create_dummy_pixmap                    (GtkWidget       *widget,
                                        gboolean         gnome_pixmap)
{
  GdkColormap *colormap;
  GdkPixmap *gdkpixmap;
  GdkBitmap *mask;
  GtkWidget *pixmap;

  if (gnome_pixmap)
    {
      return gnome_pixmap_new_from_xpm_d (dummy_pixmap_xpm);
    }

  colormap = gtk_widget_get_colormap (widget);
  gdkpixmap = gdk_pixmap_colormap_create_from_xpm_d (NULL, colormap, &mask,
                                                     NULL, dummy_pixmap_xpm);
  if (gdkpixmap == NULL)
    g_error ("Couldn't create replacement pixmap.");
  pixmap = gtk_pixmap_new (gdkpixmap, mask);
  gdk_pixmap_unref (gdkpixmap);
  gdk_bitmap_unref (mask);
  return pixmap;
}

/* This is an internally used function to create pixmaps. */
GtkWidget*
create_pixmap                          (GtkWidget       *widget,
                                        const gchar     *filename,
                                        gboolean         gnome_pixmap)
{
  GtkWidget *pixmap;
  GdkColormap *colormap;
  GdkPixmap *gdkpixmap;
  GdkBitmap *mask;
  gchar *pathname;

  pathname = gnome_pixmap_file (filename);
  if (!pathname)
    {
      g_warning (_("Couldn't find pixmap file: %s"), filename);
      return create_dummy_pixmap (widget, gnome_pixmap);
    }

  if (gnome_pixmap)
    {
      pixmap = gnome_pixmap_new_from_file (pathname);
      g_free (pathname);
      return pixmap;
    }

  colormap = gtk_widget_get_colormap (widget);
  gdkpixmap = gdk_pixmap_colormap_create_from_xpm (NULL, colormap, &mask,
                                                   NULL, pathname);
  if (gdkpixmap == NULL)
    {
      g_warning (_("Couldn't create pixmap from file: %s"), pathname);
      g_free (pathname);
      return create_dummy_pixmap (widget, gnome_pixmap);
    }
  g_free (pathname);

  pixmap = gtk_pixmap_new (gdkpixmap, mask);
  gdk_pixmap_unref (gdkpixmap);
  gdk_bitmap_unref (mask);
  return pixmap;
}

/* This is an internally used function to create imlib images. */
GdkImlibImage*
create_image                           (const gchar     *filename)
{
  GdkImlibImage *image;
  gchar *pathname;

  pathname = gnome_pixmap_file (filename);
  if (!pathname)
    {
      g_warning (_("Couldn't find pixmap file: %s"), filename);
      return NULL;
    }

  image = gdk_imlib_load_image (pathname);
  g_free (pathname);
  return image;
}

