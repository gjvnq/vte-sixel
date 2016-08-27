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
#include "vteimage.hh"
#include "vteinternal.hh"

static cairo_status_t read_callback(void *closure, unsigned char *data, unsigned int length);
static cairo_status_t write_callback(void *closure, const unsigned char *data, unsigned int length);

// tempfile_t manipulation

static tempfile_t *tempfile_current = NULL;
static size_t const TEMPFILE_MAX_SIZE = 1024 * 1024 * 16;  /* 16MB */
static size_t tempfile_num = 0;
static size_t const TEMPFILE_MAX_NUM = 16;

static tempfile_t *
tempfile_new(void)
{
	tempfile_t *tempfile;
	FILE *fp;

	fp = tmpfile();
	if (!fp)
		return NULL;

	tempfile = (tempfile_t *)g_malloc(sizeof(tempfile_t));
	if (!tempfile)
		return NULL;

	tempfile->fp = fp;
	tempfile->ref_counter = 1;

	tempfile_num++;

	return tempfile;
}

static void
tempfile_destroy(tempfile_t *tempfile)
{
	if (tempfile == tempfile_current)
		tempfile_current = NULL;
	fclose((FILE *)tempfile->fp);
	free(tempfile);
	tempfile_num--;
}

static void
tempfile_ref(tempfile_t *tempfile)
{
	tempfile->ref_counter++;
}

static void
tempfile_deref(tempfile_t *tempfile)
{
	if (--tempfile->ref_counter == 0)
		tempfile_destroy(tempfile);
}

static size_t
tempfile_size(tempfile_t *tempfile)
{
	fseek((FILE *)tempfile->fp, 0L, SEEK_END);
	return (size_t)ftell((FILE *)tempfile->fp);
}

static tempfile_t *
tempfile_get(void)
{
	size_t size;

	if (!tempfile_current) {
		tempfile_current = tempfile_new();
		return tempfile_current;
	}

	/* get file size */
	size = tempfile_size(tempfile_current);

	/* if the file size reaches TEMPFILE_MAX_SIZE, return new temporary file */
	if (size > TEMPFILE_MAX_SIZE) {
		tempfile_current = tempfile_new();
		return tempfile_current;
	}

	/* increment reference counter */
	tempfile_ref(tempfile_current);

	return tempfile_current;
}

// temp_storage_t implementation

static temp_storage_t *
storage_create(void)
{
	temp_storage_t *storage;
	tempfile_t *tempfile;

	tempfile = tempfile_get();
	if (!tempfile)
		return NULL;

	storage = (temp_storage_t *)g_malloc(sizeof(temp_storage_t));
	if (!storage)
		return NULL;

	storage->tempfile = tempfile;
	storage->position = tempfile_size(storage->tempfile);

	return storage;
}

static void
storage_destroy(temp_storage_t *storage)
{
	tempfile_deref(storage->tempfile);
	free(storage);
}

/* VteImage implementation */
VteImage::VteImage(cairo_surface_t *s, gint col, gint row, gint w, int h)
	:left(col), top(row), width(w), height(h), next(NULL), prev(NULL), surface(s), storage(NULL)
{
}

VteImage::~VteImage()
{
	if (this->surface)
		cairo_surface_destroy(this->surface);
	if (this->storage)
		storage_destroy(this->storage);
}

/* Deserialize the cairo image from the temporary file */
bool
VteImage::lazyinit()
{
	if (this->surface)
		return true;

	if (! this->storage)
		return false;

	if (fseek((FILE *)this->storage->tempfile->fp, this->storage->position, SEEK_SET) != 0)
		return false;

	this->surface = cairo_image_surface_create_from_png_stream((cairo_read_func_t)read_callback, this->storage->tempfile->fp);
	if (! this->surface)
		return false;

	return true;
}

/* Serialize the image for saving RAM */
void
VteImage::hibernate()
{
	cairo_status_t status;

	if (! this->surface)
		return;

	if (! this->storage) {
		this->storage = storage_create();
		if (! this->storage)
			return;
	}

	if (fseek((FILE *)this->storage->tempfile->fp, this->storage->position, SEEK_SET) != 0)
		return;

	status = cairo_surface_write_to_png_stream(this->surface, (cairo_write_func_t)write_callback, this->storage->tempfile->fp);
	if (status == CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(this->surface);
		this->surface = NULL;
	} else {
		storage_destroy(this->storage);
		this->storage = NULL;
	}
}

/* Test whether this image includes that image */
bool
VteImage::includes(const VteImage *that)
{
	return that->left >= this->left &&
	       that->top >= this->top &&
	       that->left + that->width <= this->left + this->width &&
	       that->top + that->height <= this->top + this->height;
}

void
VteImage::combine(VteImage *that, int char_width, int char_height)
{
	cairo_t *cr;
	int offsetx = (that->left - this->left) * char_width;
	int offsety = (that->top - this->top) * char_height;
	int pixelwidth = cairo_image_surface_get_width(this->surface);
	int pixelheight = cairo_image_surface_get_height(this->surface);

	if ((! this->lazyinit()) || (! that->lazyinit()))
		return;

	cr = cairo_create(this->surface);
	cairo_rectangle(cr, offsetx, offsety, pixelwidth, pixelheight);
	cairo_clip(cr);
	cairo_set_source_surface(cr, that->surface, offsetx, offsety);
	cairo_paint(cr);
	cairo_destroy(cr);
}

void
VteImage::paint(cairo_t *cr, int offsetx, int offsety)
{
	int pixelwidth;
	int pixelheight;

	if (! this->lazyinit())
		return;

	pixelwidth = cairo_image_surface_get_width(this->surface);
	pixelheight = cairo_image_surface_get_height(this->surface);
	cairo_save(cr);
	cairo_rectangle(cr, offsetx, offsety, pixelwidth, pixelheight);
	cairo_clip(cr);
	cairo_set_source_surface(cr, this->surface, offsetx, offsety);
	cairo_paint(cr);
	cairo_restore(cr);
}

void
VteImage::destroy_recursively(void)
{
	if (next)
		next->destroy_recursively();
	delete this;
}

bool
VteImage::storage_is_full(void)
{
	return tempfile_num > TEMPFILE_MAX_NUM;
}

/* callback routines for hibernate / lazyinit */

static cairo_status_t
read_callback(void *closure, unsigned char *data, unsigned int length)
{
	size_t nread;
	FILE *fp = (FILE *)closure;

	g_assert_true(fp != NULL);

	nread = fread(data, 1, length, fp);
	if (ferror(fp) != 0) {
		clearerr(fp);
		return CAIRO_STATUS_READ_ERROR;
	}
	return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
write_callback(void *closure, const unsigned char *data, unsigned int length)
{
	size_t nwrite;
	FILE *fp = (FILE *)closure;

	nwrite = fwrite(data, 1, length, fp);
	if (nwrite != length)
		return CAIRO_STATUS_WRITE_ERROR;
	return CAIRO_STATUS_SUCCESS;
}
