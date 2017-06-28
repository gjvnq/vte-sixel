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

namespace vte::image {

//static image_object::cairo_status_t read_callback (void *closure, char *data, unsigned int length);
//static image_object::cairo_status_t write_callback (void *closure, const char *data, unsigned int length);

/* image_object implementation */
image_object::image_object (cairo_surface_t *surface, gint pixelwidth, gint pixelheight, gint col, gint row, gint w, gint h, _VteStream *stream)
{
	this->pixelwidth = pixelwidth;
	this->pixelheight = pixelheight;
	this->left = col;
	this->top = row;
	this->width = w;
	this->height = h;
	this->surface = surface;
	this->stream = stream;
}

image_object::~image_object ()
{
	if (this->surface)
		cairo_surface_destroy (this->surface);
}

gulong
image_object::get_stream_position () const
{
	return this->position;
}

/* Indicate whether the image is serialized to the stream */
bool
image_object::is_freezed () const
{
	return (this->surface == NULL);
}

/* Deserialize the cairo image from the temporary file */
bool
image_object::ensure_thawn ()
{
	if (this->surface)
		return true;
	if (this->position < _vte_stream_tail (this->stream))
		return false;

	this->nread = 0;
	this->surface = cairo_image_surface_create_from_png_stream ((cairo_read_func_t)read_callback, this);
	if (! this->surface)
		return false;

	return true;
}

/* Serialize the image for saving RAM */
void
image_object::freeze ()
{
	cairo_status_t status;

	if (! this->surface)
		return;

	this->position = _vte_stream_head (this->stream);
	this->nwrite = 0;

	status = cairo_surface_write_to_png_stream (this->surface, (cairo_write_func_t)write_callback, this);
	if (status == CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy (this->surface);
		this->surface = NULL;
	}
}

/* Test whether image image includes that image */
bool
image_object::includes (const image_object *rhs) const
{
	g_assert_true (rhs != NULL);

	return rhs->left >= this->left &&
	       rhs->top >= this->top &&
	       rhs->left + rhs->width <= this->left + this->width &&
	       rhs->top + rhs->height <= this->top + this->height;
}

/* Merge right-hand-side image into left-hand-side image */
void
image_object::combine (image_object *rhs, gulong char_width, gulong char_height)
{
	cairo_t *cr;

	g_assert_true (rhs != NULL);

	gulong offsetx = (rhs->left - this->left) * char_width;
	gulong offsety = (rhs->top - this->top) * char_height;

	if ((! this->ensure_thawn ()) || (! rhs->ensure_thawn ()))
		return;

	cr = cairo_create (this->surface);
	cairo_rectangle (cr, offsetx, offsety, this->pixelwidth, this->pixelheight);
	cairo_clip (cr);
	cairo_set_source_surface (cr, rhs->surface, offsetx, offsety);
	cairo_paint (cr);
	cairo_destroy (cr);
}

/* Paint the image into given cairo rendering context */
void
image_object::paint (cairo_t *cr, gint offsetx, gint offsety)
{
	if (! this->ensure_thawn ())
		return;

	cairo_save (cr);
	cairo_rectangle (cr, offsetx, offsety, this->pixelwidth, this->pixelheight);
	cairo_clip (cr);
	cairo_set_source_surface (cr, this->surface, offsetx, offsety);
	cairo_paint (cr);
	cairo_restore (cr);
}

/* callback routines for stream I/O */

cairo_status_t
image_object::read_callback (void *closure, char *data, unsigned int length)
{
	image_object *image = (image_object *)closure;

	g_assert_true (image != NULL);

	_vte_stream_read (image->stream, image->position + image->nread, data, length);
	image->nread += length;

	return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
image_object::write_callback (void *closure, const char *data, unsigned int length)
{
	image_object *image = (image_object *)closure;

	g_assert_true (image != NULL);

	_vte_stream_append (image->stream, data, length);
	image->nwrite += length;

	return CAIRO_STATUS_SUCCESS;
}

} // namespace vte::image
