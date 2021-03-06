/*
 * Copyright (C) 2002 Alexander Larsson <alexl@redhat.com>.
 * All rights reserved.
 *
 * This file is part of the Gnome Library.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gnome-icon-lookup.h"
#include "gnome-icon-theme.h"
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs.h>

#include <string.h>

#define ICON_NAME_BLOCK_DEVICE          "gnome-fs-blockdev"
#define ICON_NAME_BROKEN_SYMBOLIC_LINK  "gnome-fs-symlink"
#define ICON_NAME_CHARACTER_DEVICE      "gnome-fs-chardev"
#define ICON_NAME_DIRECTORY             "gnome-fs-directory"
#define ICON_NAME_EXECUTABLE            "gnome-fs-executable"
#define ICON_NAME_FIFO                  "gnome-fs-fifo"
#define ICON_NAME_REGULAR               "gnome-fs-regular"
#define ICON_NAME_SEARCH_RESULTS        "gnome-fs-search"
#define ICON_NAME_SOCKET                "gnome-fs-socket"

#define ICON_NAME_MIME_PREFIX           "gnome-mime-"
#define ICON_NAME_MIME_SUFFIX           "-x-generic"

#define SELF_THUMBNAIL_SIZE_THRESHOLD   32768


/* Returns NULL for regular */
static char *
get_icon_name (const char          *file_uri,
	       GnomeVFSFileInfo    *file_info,
	       const char          *mime_type,
	       GnomeIconLookupFlags flags)
{

  if (file_info &&
      (file_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_TYPE))
    {
      switch (file_info->type)
	{
        case GNOME_VFS_FILE_TYPE_DIRECTORY:
	  if (mime_type && g_ascii_strcasecmp (mime_type, "x-directory/search") == 0)
	    return g_strdup (ICON_NAME_SEARCH_RESULTS);
	  else
	    return g_strdup (ICON_NAME_DIRECTORY);
        case GNOME_VFS_FILE_TYPE_FIFO:
	  return g_strdup (ICON_NAME_FIFO);
        case GNOME_VFS_FILE_TYPE_SOCKET:
	  return g_strdup (ICON_NAME_SOCKET);
        case GNOME_VFS_FILE_TYPE_CHARACTER_DEVICE:
	  return g_strdup (ICON_NAME_CHARACTER_DEVICE);
        case GNOME_VFS_FILE_TYPE_BLOCK_DEVICE:
	  return g_strdup (ICON_NAME_BLOCK_DEVICE);
        case GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK:
	  /* Non-broken symbolic links return the target's type. */
	  return g_strdup (ICON_NAME_BROKEN_SYMBOLIC_LINK);
        case GNOME_VFS_FILE_TYPE_REGULAR:
        case GNOME_VFS_FILE_TYPE_UNKNOWN:
        default:
	  break;
	}
    }
  
  if (mime_type && g_ascii_strncasecmp (mime_type, "x-directory", strlen ("x-directory")) == 0)
    return g_strdup (ICON_NAME_DIRECTORY);
  
  /* Regular or unknown: */

  /* don't use the executable icon for text files, since it's more useful to display
   * embedded text
   */
  if ((flags & GNOME_ICON_LOOKUP_FLAGS_EMBEDDING_TEXT) &&
      file_info &&
      (file_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_PERMISSIONS) &&
      (file_info->permissions	& (GNOME_VFS_PERM_USER_EXEC
				   | GNOME_VFS_PERM_GROUP_EXEC
				   | GNOME_VFS_PERM_OTHER_EXEC)) &&
      (mime_type == NULL || g_ascii_strcasecmp (mime_type, "text/plain") != 0))
    return g_strdup (ICON_NAME_EXECUTABLE);

  return NULL;
}

static char *
get_vfs_mime_name (const char *mime_type)
{
  const char *vfs_mime_name;
  char *p;

  vfs_mime_name = gnome_vfs_mime_get_icon (mime_type);

  if (vfs_mime_name)
    {
      /* Handle absolute files */
      if (g_path_is_absolute (vfs_mime_name))
	return g_strdup (vfs_mime_name);
      
      p = strrchr(vfs_mime_name, '.');

      if (p)
	return g_strndup (vfs_mime_name, p - vfs_mime_name);
      else
	return g_strdup (vfs_mime_name);
    }
  return NULL;
}

static char *
make_mime_name (const char *mime_type)
{
  char *mime_type_without_slashes, *icon_name;
  char *p;
  
  if (mime_type == NULL) {
    return NULL;
  }

  mime_type_without_slashes = g_strdup (mime_type);
  
  while ((p = strchr(mime_type_without_slashes, '/')) != NULL)
    *p = '-';
  
  icon_name = g_strdup (mime_type_without_slashes);
  g_free (mime_type_without_slashes);
  
  return icon_name;
}

static char *
make_generic_mime_name (const char *mime_type, gboolean embedd_text)
{
  char *generic_mime_type, *icon_name;
  char *p;

  
  if (mime_type == NULL) {
    return NULL;
  }

  generic_mime_type = g_strdup (mime_type);
  
  icon_name = NULL;
  if ((p = strchr(generic_mime_type, '/')) != NULL)
    {
      *p = 0;

      if (strcmp ("text", generic_mime_type) == 0 && embedd_text)
	icon_name = g_strdup ("gnome-fs-regular");
      else
	icon_name = g_strconcat (generic_mime_type, ICON_NAME_MIME_SUFFIX, NULL);
    }
  g_free (generic_mime_type);
  
  return icon_name;
}

static char *
make_compat_mime_name (const char *mime_type, gboolean embedd_text)
{
  char *compat_mime_type, *icon_name;
  
  if (mime_type == NULL) {
    return NULL;
  }

  compat_mime_type = make_mime_name (mime_type);

  icon_name = g_strconcat (ICON_NAME_MIME_PREFIX, compat_mime_type, NULL);
  g_free (compat_mime_type);
  
  return icon_name;
}


static char *
make_compat_generic_mime_name (const char *mime_type, gboolean embedd_text)
{
  char *generic_mime_type, *icon_name;
  char *p;

  
  if (mime_type == NULL) {
    return NULL;
  }

  generic_mime_type = g_strdup (mime_type);
  
  icon_name = NULL;
  if ((p = strchr(generic_mime_type, '/')) != NULL)
    {
      *p = 0;

      if (strcmp ("text", generic_mime_type) == 0 && embedd_text)
	icon_name = g_strdup ("gnome-fs-regular");
      else
	icon_name = g_strconcat (ICON_NAME_MIME_PREFIX, generic_mime_type, NULL);
    }
  g_free (generic_mime_type);
  
  return icon_name;
}

static gboolean
mimetype_supported_by_gdk_pixbuf (const char *mime_type)
{
	guint i;
	static GHashTable *formats_hash = NULL;

	if (!formats_hash) {
		GSList *formats, *list;
		
		formats_hash = g_hash_table_new (g_str_hash, g_str_equal);

		formats = gdk_pixbuf_get_formats ();
		list = formats;
		
		while (list) {
			GdkPixbufFormat *format = list->data;
			gchar **mime_types;
			
			mime_types = gdk_pixbuf_format_get_mime_types (format);

			for (i = 0; mime_types[i] != NULL; i++)
				g_hash_table_insert (formats_hash,
						     (gpointer) g_strdup (mime_types[i]),
						     GUINT_TO_POINTER (1));	
				
			g_strfreev (mime_types);
			list = list->next;
		}
		g_slist_free (formats);
	}

	if (g_hash_table_lookup (formats_hash, mime_type))
		return TRUE;

	return FALSE;
}

/**
 * gnome_icon_lookup:
 * @icon_theme: a #GtkIconTheme, or (deprecated) a #GnomeIconTheme
 * @thumbnail_factory: an optional #GnomeThumbnailFactory used to look up thumbnails
 * @file_uri: the uri of the file
 * @custom_icon: optionally the name of a custom icon to try
 * @file_info: information about the file from gnome_vfs_get_file_info()
 * @mime_type: the mime type of the icon
 * @flags: flags that affect the result of the lookup
 * @result: optionally result flags of the lookups are stored here
 *
 * This function tries to locate an icon in @icon_theme that can be used
 * to represent the file passed. It can optionally also look for existing
 * thumbnails. It does no I/O, so the lookup should be relatively fast.
 * 
 * If you don't know any information about the file already you can use
 * gnome_icon_lookup_sync() which gets this information using gnome-vfs.
 *
 * For backwards compatibility, this function also accepts a GnomeIconTheme
 * instead of a GtkIconTheme.
 *
 * The following @flags are valid:
 * 
 * GNOME_ICON_LOOKUP_FLAGS_EMBEDDING_TEXT - Return the text icon for scripts.
 * This is useful if you do text embedding in the icons.
 * 
 * GNOME_ICON_LOOKUP_FLAGS_SHOW_SMALL_IMAGES_AS_THEMSELVES - Return the
 * filename of the filename itself for small images. Only availible if you
 * pass a thumbnail_factory.
 *
 * Possible @result flags:
 *
 * GNOME_ICON_LOOKUP_RESULT_FLAGS_THUMBNAIL - The returned file is a thumbnail.
 *
 * Returns: The name of an icon or an absolute filename of an image to
 *          use for the file.
 *
 * Since: 2.2
 **/
char *
gnome_icon_lookup (GtkIconTheme               *icon_theme,
		   GnomeThumbnailFactory      *thumbnail_factory,
		   const char                 *file_uri,
		   const char                 *custom_icon,
		   GnomeVFSFileInfo           *file_info,
		   const char                 *mime_type,
		   GnomeIconLookupFlags        flags,
		   GnomeIconLookupResultFlags *result)
{
  char *icon_name;
  char *mime_name;
  char *thumbnail;
  time_t mtime;
  gboolean is_svg;

  /* For backwards compat we support GnomeIconTheme too */
  if (GNOME_IS_ICON_THEME (icon_theme))
    {
      if (gnome_icon_theme_get_allow_svg (GNOME_ICON_THEME (icon_theme)))
	flags |= GNOME_ICON_LOOKUP_FLAGS_ALLOW_SVG_AS_THEMSELVES;
      icon_theme = _gnome_icon_theme_get_gtk_icon_theme (GNOME_ICON_THEME (icon_theme));
    }

  g_return_val_if_fail (GTK_IS_ICON_THEME (icon_theme), NULL);
  
  if (result)
    *result = GNOME_ICON_LOOKUP_RESULT_FLAGS_NONE;

  /* Look for availibility of custom icon */
  if (custom_icon)
    {
      /* WARNING: Does I/O for abs custom icons! */
      if ((g_path_is_absolute (custom_icon) &&
	   g_file_test (custom_icon, G_FILE_TEST_IS_REGULAR)) ||
	  gtk_icon_theme_has_icon (icon_theme, custom_icon))
	return g_strdup (custom_icon);
    }

  if (thumbnail_factory)
    {
      if(mime_type)
      {
	    is_svg = (strcmp (mime_type, "image/svg") == 0 ||
			strcmp (mime_type, "image/svg+xml") == 0);
      	    if (flags & GNOME_ICON_LOOKUP_FLAGS_SHOW_SMALL_IMAGES_AS_THEMSELVES 
		&& ((!is_svg && mimetype_supported_by_gdk_pixbuf (mime_type)) ||
	   	( is_svg && flags & GNOME_ICON_LOOKUP_FLAGS_ALLOW_SVG_AS_THEMSELVES))  &&
	  	strncmp (file_uri, "file:/", 6) == 0 &&
	  	file_info && file_info->size < SELF_THUMBNAIL_SIZE_THRESHOLD)
		return gnome_vfs_get_local_path_from_uri (file_uri);
      }
      
      mtime = 0;
      if (file_info)
	mtime = file_info->mtime;
      
      thumbnail = gnome_thumbnail_factory_lookup (thumbnail_factory, file_uri,
						  mtime);
      if (thumbnail)
	{
	  if (result)
	    *result = GNOME_ICON_LOOKUP_RESULT_FLAGS_THUMBNAIL;
	  
	  return thumbnail;
	}
    }

  if (mime_type)
    {
      mime_name = get_vfs_mime_name (mime_type);
      if (mime_name &&
	  ((g_path_is_absolute (mime_name) &&
	    g_file_test (mime_name, G_FILE_TEST_IS_REGULAR)) ||
	   gtk_icon_theme_has_icon (icon_theme, mime_name)))
	return mime_name;
      g_free (mime_name);
      
      mime_name = make_mime_name (mime_type);
      if (mime_name && gtk_icon_theme_has_icon (icon_theme, mime_name))
	return mime_name;
      g_free (mime_name);
      
      mime_name = make_generic_mime_name (mime_type, flags & GNOME_ICON_LOOKUP_FLAGS_EMBEDDING_TEXT);
      if (mime_name && gtk_icon_theme_has_icon (icon_theme, mime_name))
	return mime_name;
      g_free (mime_name);

      mime_name = make_compat_mime_name (mime_type, flags & GNOME_ICON_LOOKUP_FLAGS_EMBEDDING_TEXT);
      if (mime_name && gtk_icon_theme_has_icon (icon_theme, mime_name))
	return mime_name;
      g_free (mime_name);

      mime_name = make_compat_generic_mime_name (mime_type, flags & GNOME_ICON_LOOKUP_FLAGS_EMBEDDING_TEXT);
      if (mime_name && gtk_icon_theme_has_icon (icon_theme, mime_name))
	return mime_name;
      g_free (mime_name);
    }
      
  icon_name = get_icon_name (file_uri, file_info, mime_type, flags);
  if (icon_name && gtk_icon_theme_has_icon (icon_theme, icon_name))
    return icon_name;
  g_free (icon_name);

  return g_strdup (ICON_NAME_REGULAR);
}

/**
 * gnome_icon_lookup_sync:
 * @icon_theme: a #GtkIconTheme, or (deprecated) a #GnomeIconTheme
 * @thumbnail_factory: an optional #GnomeThumbnailFactory used to look up thumbnails
 * @file_uri: the uri of the file
 * @custom_icon: optionally the name of a custom icon to try
 * @flags: flags that affect the result of the lookup
 * @result: optionally result flags of the lookups are stored here
 *
 * This function tries to locate an icon in @icon_theme that can be used
 * to represent the file passed. See gnome_icon_lookup() for more information.
 * 
 * Returns: The name of an icon or an absolute filename of an image to
 *          use for the file.
 *
 * Since: 2.2
 */
char *
gnome_icon_lookup_sync (GtkIconTheme            *icon_theme,
			GnomeThumbnailFactory   *thumbnail_factory,
			const char              *file_uri,
			const char              *custom_icon,
			GnomeIconLookupFlags     flags,
			GnomeIconLookupResultFlags *result)
{
  const char *mime_type;
  char *res;
  GnomeVFSFileInfo *file_info;

  file_info = gnome_vfs_file_info_new ();
  gnome_vfs_get_file_info (file_uri,
			   file_info,
			   GNOME_VFS_FILE_INFO_FOLLOW_LINKS|
			   GNOME_VFS_FILE_INFO_GET_MIME_TYPE);

  mime_type = NULL;
  if (file_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE)
    mime_type = file_info->mime_type;


  res = gnome_icon_lookup (icon_theme,
			   thumbnail_factory,
			   file_uri,
			   custom_icon,
			   file_info,
			   mime_type,
			   flags,
			   result);
  
  gnome_vfs_file_info_unref (file_info);

  return res;
}



