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

namespace vte {

namespace image {

/* image_object implementation */
image_object::image_object (cairo_surface_t *surface, gint pixelwidth, gint pixelheight, gint col, gint row, gint w, gint h, _VteStream *stream)
{
	m_pixelwidth = pixelwidth;
	m_pixelheight = pixelheight;
	m_left = col;
	m_top = row;
	m_width = w;
	m_height = h;
	m_surface = surface;
	m_stream = stream;

	g_object_ref (m_stream);
}

image_object::~image_object ()
{
	if (m_surface)
		cairo_surface_destroy (m_surface);
	g_object_unref (m_stream);
}

glong
image_object::get_left () const
{
	return (glong)m_left;
}

glong
image_object::get_top () const
{
	return (glong)m_top;
}

glong
image_object::get_bottom () const
{
	return (glong)(m_top + m_height - 1);
}

gulong
image_object::get_stream_position () const
{
	return m_position;
}

/* Indicate whether the image is serialized to the stream */
bool
image_object::is_freezed () const
{
	return (m_surface == NULL);
}

/* Test whether this image includes given image */
bool
image_object::includes (const image_object *other) const
{
	g_assert_true (other != NULL);

	return other->m_left >= m_left &&
	       other->m_top >= m_top &&
	       other->m_left + other->m_width <= m_left + m_width &&
	       other->m_top + other->m_height <= m_top + m_height;
}

size_t
image_object::resource_size () const
{
	size_t result_size;

	if (is_freezed ()) {
		/* If freezed, return the size sent to VteBoa.
                 * In reality, it may be more compressed on the real storage.
                 */
		result_size = m_nwrite;
	} else {
		/* If not freezed, return the pixel buffer size
                 * width x height x 4
                 */
		result_size = m_pixelwidth * m_pixelheight * 4;
	}

	return result_size;
}

/* Deserialize the cairo image from the temporary file */
bool
image_object::thaw ()
{
	if (m_surface)
		return true;
	if (m_position < _vte_stream_tail (m_stream))
		return false;

	m_nread = 0;
	m_surface = cairo_image_surface_create_from_png_stream ((cairo_read_func_t)read_callback, this);
	if (! m_surface)
		return false;

	return true;
}

/* Serialize the image for saving RAM */
void
image_object::freeze ()
{
	cairo_status_t status;
	double x_scale, y_scale;

	if (! m_surface)
		return;

	m_position = _vte_stream_head (m_stream);
	m_nwrite = 0;

	cairo_surface_get_device_scale (m_surface, &x_scale, &y_scale);
	if (x_scale != 1.0 || y_scale != 1.0) {
		/* If device scale exceeds 1.0, large size of PNG image created with cairo_surface_write_to_png_stream()
                 * So we need to convert m_surface into an image surface with resizing it.
                 */
		cairo_surface_t *image_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, m_pixelwidth, m_pixelheight);
		cairo_t *cr = cairo_create (image_surface);
		paint (cr, 0, 0);
		cairo_destroy (cr);
		cairo_surface_destroy (m_surface);
		m_surface = image_surface;
	}
	status = cairo_surface_write_to_png_stream (m_surface, (cairo_write_func_t)write_callback, this);
	if (status == CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy (m_surface);
		m_surface = NULL;
	}
}

/* Merge another image into this image */
bool
image_object::combine (image_object *other, gulong char_width, gulong char_height)
{
	cairo_t *cr;

	g_assert_true (other != NULL);

	gulong offsetx = (other->m_left - m_left) * char_width;
	gulong offsety = (other->m_top - m_top) * char_height;

	if (is_freezed ())
		if (! thaw ())
			return false;

	if (other->is_freezed ())
		if (! other->thaw ())
			return false;

	cr = cairo_create (m_surface);
	cairo_rectangle (cr, offsetx, offsety, m_pixelwidth, m_pixelheight);
	cairo_clip (cr);
	cairo_set_source_surface (cr, other->m_surface, offsetx, offsety);
	cairo_paint (cr);
	cairo_destroy (cr);

	return true;
}

bool
image_object::unite (image_object *other, gulong char_width, gulong char_height)
{
	if (is_freezed ())
		if (! thaw ())
			return false;

	gint new_left = std::min (m_left, other->m_left);
	gint new_top = std::min (m_top, other->m_top);
	gint new_width = std::max (m_left + m_width, other->m_left + other->m_width) - new_left;
	gint new_height = std::max (m_top + m_height, other->m_top + other->m_width) - new_top;
	gint pixelwidth = new_width * char_width;
	gint pixelheight = new_height * char_height;
	gint offsetx = (m_left - new_left) * char_width;
	gint offsety = (m_top - new_top) * char_height;

	cairo_surface_t * new_surface = cairo_surface_create_similar (other->m_surface, CAIRO_CONTENT_COLOR_ALPHA, m_pixelwidth, m_pixelheight);
	cairo_t *cr = cairo_create (new_surface);
	cairo_rectangle (cr, offsetx, offsety, m_pixelwidth, m_pixelheight);
	cairo_clip (cr);
	cairo_set_source_surface (cr, m_surface, offsetx, offsety);
	cairo_paint (cr);
	cairo_destroy (cr);

	cairo_surface_destroy (m_surface);

	m_left = new_left;
	m_top = new_top;
	m_width = new_width;
	m_height = new_height;
	m_pixelwidth = pixelwidth;
	m_pixelheight = pixelheight;
	m_surface = new_surface;

	return combine (other, char_width, char_height);
}

/* Paint the image into given cairo rendering context */
bool
image_object::paint (cairo_t *cr, gint offsetx, gint offsety)
{
	if (is_freezed ())
		if (! thaw ())
			return false;

	cairo_save (cr);
	cairo_rectangle (cr, offsetx, offsety, m_pixelwidth, m_pixelheight);
	cairo_clip (cr);
	cairo_set_source_surface (cr, m_surface, offsetx, offsety);
	cairo_paint (cr);
	cairo_restore (cr);

	return true;
}

/* callback routines for stream I/O */

cairo_status_t
image_object::read_callback (void *closure, char *data, unsigned int length)
{
	image_object *image = (image_object *)closure;

	g_assert_true (image != NULL);

	_vte_stream_read (image->m_stream, image->m_position + image->m_nread, data, length);
	image->m_nread += length;

	return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
image_object::write_callback (void *closure, const char *data, unsigned int length)
{
	image_object *image = (image_object *)closure;

	g_assert_true (image != NULL);

	_vte_stream_append (image->m_stream, data, length);
	image->m_nwrite += length;

	return CAIRO_STATUS_SUCCESS;
}

} // namespace image

} // namespace vte
