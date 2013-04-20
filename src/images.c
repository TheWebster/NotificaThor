/* ************************************************************* *\
 * images.c                                                      *
 *                                                               *
 * Project:     NotificaThor                                     *
 * Author:      Christian Weber (ChristianWeber802@gmx.net)      *
 *                                                               *
 * Description: Functions regarding image files.                 *
\* ************************************************************* */

#include <cairo/cairo.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <syslog.h>

#include "NotificaThor.h"
#include "logging.h"
#include "images.h"


#define IMAGE_CACHE_SIZE   32

typedef struct
{
	int             used;
	cairo_pattern_t *pattern;
	char            filename[FILENAME_MAX];
} image_cache_t;

static image_cache_t image_cache[IMAGE_CACHE_SIZE] = {{0}};
static int           next_im_cache = 0;


/*
 * Search for the given filename in the image ringbuffer or create a new pattern.
 * 
 * Parameters: filename - The path to the PNG-file.
 * 
 * Returns: cairo_pattern_t* for filename.
 */
cairo_pattern_t *
get_pattern_for_png( char *filename)
{
	int             i;
	cairo_surface_t *surface;
	cairo_pattern_t *pattern;
	cairo_matrix_t  scaling;
	
	/** search for surface to be already present **/
	for( i = 0; i < IMAGE_CACHE_SIZE && image_cache[i].used == 1; i++ ) {
		if( strcmp( image_cache[i].filename, filename) == 0 ) {
#ifdef VERBOSE
			thor_log( LOG_DEBUG, "Found '%s' in image_cache[%d]...", filename, i);
#endif
			return image_cache[i].pattern;
		}
	}
	
	/** otherwise create it **/
#ifdef VERBOSE
	thor_log( LOG_DEBUG, "Creating pattern for '%s' in image_cache[%d]...", filename, next_im_cache);
#endif
	surface = cairo_image_surface_create_from_png( filename);
	pattern = cairo_pattern_create_for_surface( surface);
	if( cairo_pattern_status( pattern) != CAIRO_STATUS_SUCCESS ) {
		cairo_surface_destroy( surface);
		return pattern;
	}
	
	cairo_matrix_init_scale( &scaling, cairo_image_surface_get_width( surface),
	                                   cairo_image_surface_get_height( surface));
	cairo_pattern_set_matrix( pattern, &scaling);
	cairo_surface_destroy( surface);
	
	if( image_cache[next_im_cache].used == 1 )
		cairo_pattern_destroy( image_cache[next_im_cache].pattern);
	
	image_cache[next_im_cache].used = 1;
	image_cache[next_im_cache].pattern = pattern;
	cpycat( image_cache[next_im_cache].filename, filename);
	
	return pattern;
};


/*
 * Free the image cache
 */
void
free_image_cache()
{
	int i;
	
	
	for( i = 0; i < IMAGE_CACHE_SIZE; i++ ) {
		if( image_cache[i].used )
			cairo_pattern_destroy( image_cache[i].pattern);
	}
};
