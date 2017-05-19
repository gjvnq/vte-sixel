/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <config.h>
#include <glib.h>
#include <stdio.h>
#include "vteimage.h"
#include "vteinternal.hh"

static cairo_status_t read_callback (void *closure, char *data, unsigned int length);
static cairo_status_t write_callback (void *closure, const char *data, unsigned int length);

/* VteImage implementation */
void
_vte_image_init (VteImage **image, cairo_surface_t *s, gint col, gint row, gint w, int h, VteStream *stream)
{
	*image = (VteImage *)g_malloc0 (sizeof (VteImage));
	(*image)->left = col;
	(*image)->top = row;
	(*image)->width = w;
	(*image)->height = h;
	(*image)->surface = s;
	(*image)->stream = stream;
}

void
_vte_image_fini (VteImage *image)
{
	if (! image->surface)
		if (! _vte_image_ensure_thawn (image))
			return;

	cairo_surface_destroy (image->surface);
	free (image);
}

/* Deserialize the cairo image from the temporary file */
bool
_vte_image_ensure_thawn (VteImage *image)
{
	g_assert_true (image != NULL);

	if (image->surface)
		return true;
	if (image->position < _vte_stream_tail (image->stream))
		return false;

	image->nread = 0;
	image->surface = cairo_image_surface_create_from_png_stream ((cairo_read_func_t)read_callback, image);
	if (! image->surface)
		return false;

	return true;
}

/* Serialize the image for saving RAM */
void
_vte_image_freeze (VteImage *image)
{
	cairo_status_t status;

	g_assert_true (image != NULL);

	if (! image->surface)
		return;

	image->position = _vte_stream_head (image->stream);
	image->nwrite = 0;

	status = cairo_surface_write_to_png_stream (image->surface, (cairo_write_func_t)write_callback, image);
	if (status == CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy (image->surface);
		image->surface = NULL;
	}
}

/* Test whether image image includes that image */
bool
_vte_image_includes (const VteImage *lhs, const VteImage *rhs)
{
	g_assert_true (lhs != NULL);
	g_assert_true (rhs != NULL);

	return rhs->left >= lhs->left &&
	       rhs->top >= lhs->top &&
	       rhs->left + rhs->width <= lhs->left + lhs->width &&
	       rhs->top + rhs->height <= lhs->top + lhs->height;
}

/* Merge right-hand-side image into left-hand-side image */
void
_vte_image_combine (VteImage *lhs, VteImage *rhs, gulong char_width, gulong char_height)
{
	cairo_t *cr;

	g_assert_true (lhs != NULL);
	g_assert_true (rhs != NULL);

	gulong offsetx = (rhs->left - lhs->left) * char_width;
	gulong offsety = (rhs->top - lhs->top) * char_height;
	gulong pixelwidth = cairo_image_surface_get_width (lhs->surface);
	gulong pixelheight = cairo_image_surface_get_height (lhs->surface);

	if ((! _vte_image_ensure_thawn (lhs)) || (! _vte_image_ensure_thawn (rhs)))
		return;

	cr = cairo_create (lhs->surface);
	cairo_rectangle (cr, offsetx, offsety, pixelwidth, pixelheight);
	cairo_clip (cr);
	cairo_set_source_surface (cr, rhs->surface, offsetx, offsety);
	cairo_paint (cr);
	cairo_destroy (cr);
}

/* Paint the image into given cairo rendering context */
void
_vte_image_paint (VteImage *image, cairo_t *cr, gint offsetx, gint offsety)
{
	gulong pixelwidth;
	gulong pixelheight;

	g_assert_true (image != NULL);

	if (! _vte_image_ensure_thawn (image))
		return;

	pixelwidth = cairo_image_surface_get_width (image->surface);
	pixelheight = cairo_image_surface_get_height (image->surface);
	cairo_save (cr);
	cairo_rectangle (cr, offsetx, offsety, pixelwidth, pixelheight);
	cairo_clip (cr);
	cairo_set_source_surface (cr, image->surface, offsetx, offsety);
	cairo_paint (cr);
	cairo_restore (cr);
}

/* Indicate whether the image is serialized to the stream */
bool
_vte_image_is_freezed (VteImage *image)
{
	g_assert_true (image != NULL);

	return (image->surface == NULL);
}

/* callback routines for stream I/O */

static cairo_status_t
read_callback (void *closure, char *data, unsigned int length)
{
	VteImage *image = (VteImage *)closure;

	g_assert_true (image != NULL);

	_vte_stream_read (image->stream, image->position + image->nread, (char *)data, length);
	image->nread += length;

	return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
write_callback (void *closure, const char *data, unsigned int length)
{
	VteImage *image = (VteImage *)closure;

	g_assert_true (image != NULL);

	_vte_stream_append (image->stream, (const char *)data, length);
	image->nwrite += length;

	return CAIRO_STATUS_SUCCESS;
}

