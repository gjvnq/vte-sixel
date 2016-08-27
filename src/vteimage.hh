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

typedef struct {
	void *fp;
	uint ref_counter;
	uint amount;
} tempfile_t;

typedef struct {
	tempfile_t *tempfile;
	size_t position;
} temp_storage_t;

class VteImage {
public:
	gint left;
	gint top;
	gint width;
	gint height;
	class VteImage *next;
	class VteImage *prev;
private:
	cairo_surface_t *surface;
	temp_storage_t *storage;

public:
	VteImage(cairo_surface_t *s, gint col, gint row, gint w, gint h);
	~VteImage(void);
	void hibernate(void);
	bool includes(const VteImage *image);
	void combine(VteImage *that, int char_width, int char_height);
	void paint(cairo_t *cr, int offsetx, int offsety);
	void destroy_recursively(void);
	bool storage_is_full(void);

private:
	bool lazyinit(void);
};
