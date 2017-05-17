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

typedef struct _VteImage {
	gint left;                /* left position in cell unit at the vte virtual screen */
	gint top;                 /* top position in cell unit at the vte virtual screen */
	gint width;               /* width in cell unit */
	gint height;              /* height in cell unit */
	VteStream *stream;        /* NULL if it's serialized */
	gulong position;          /* indicates the position at the stream if it's serialized */
	size_t nread;             /* private use: for read callback */
	size_t nwrite;            /* private use: for write callback */
	cairo_surface_t *surface; /* internal cairo image */
} VteImage;

void _vte_image_init (VteImage **image, cairo_surface_t *s, gint col, gint row, gint w, gint h, _VteStream *stream);
void _vte_image_fini (VteImage *image);
bool _vte_image_is_freezed (VteImage *image);
void _vte_image_freeze (VteImage *image);
bool _vte_image_ensure_thawn (VteImage *image);
bool _vte_image_includes (const VteImage *lhs, const VteImage *rhs);
void _vte_image_combine (VteImage *lhs, VteImage *rhs, gulong char_width, gulong char_height);
void _vte_image_paint (VteImage *image, cairo_t *cr, gint offsetx, gint offsety);

