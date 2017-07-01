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

#pragma once

#include <pango/pangocairo.h>
#include "vtestream.h"

namespace vte {

namespace image {

struct image_object {
private:
	gint m_left;                /* left position in cell unit at the vte virtual screen */
	gint m_width;               /* width in cell unit */
	gint m_top;                 /* top position in cell unit at the vte virtual screen */
	gint m_height;              /* height in cell unit */
	VteStream *m_stream;        /* NULL if it's serialized */
	gint m_pixelwidth;          /* image width in pixels */
	gint m_pixelheight;         /* image hieght in pixels */
	gulong m_position;          /* indicates the position at the stream if it's serialized */
	size_t m_nread;             /* private use: for read callback */
	size_t m_nwrite;            /* private use: for write callback */
	cairo_surface_t *m_surface; /* internal cairo image */
public:
	explicit image_object (cairo_surface_t *surface, gint pixelwidth, gint pixelheight, gint col, gint row, gint w, gint h, _VteStream *stream);
	~image_object ();
	glong get_left () const;
	glong get_top () const;
	glong get_bottom () const;
	gulong get_stream_position () const;
	bool is_freezed () const;
	bool includes (const image_object *rhs) const;
	size_t resource_size () const;
	void freeze ();
	bool thaw ();
	bool combine (image_object *rhs, gulong char_width, gulong char_height);
	bool unite (image_object *rhs, gulong char_width, gulong char_height);
	bool paint (cairo_t *cr, gint offsetx, gint offsety);
public:
	static cairo_status_t read_callback (void *closure, char *data, unsigned int length);
	static cairo_status_t write_callback (void *closure, const char *data, unsigned int length);
};

} // namespace image

} // namespace vte
