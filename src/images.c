/* ************************************************************* *\
 * images.c                                                      *
 *                                                               *
 * Project:     NotificaThor                                     *
 * Author:      Christian Weber (ChristianWeber802@gmx.net)      *
 *                                                               *
 * Description: Functions regarding image files.                 *
\* ************************************************************* */

#include <cairo/cairo.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <syslog.h>

#include "cairo_guards.h"
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

char image_cache_path[FILENAME_MAX];


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
#endif /* VERBOSE */
			return image_cache[i].pattern;
		}
	}
	
	/** otherwise create it **/
#ifdef VERBOSE
	thor_log( LOG_DEBUG, "Creating pattern for '%s' in image_cache[%d]...", filename, next_im_cache);
#endif /* VERBOSE */
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
	
	next_im_cache++;
	if( next_im_cache == IMAGE_CACHE_SIZE )
		next_im_cache = 0;
	
	return pattern;
};


/*
 * Loads image_cache from file and creates cairo_patterns for every filename.
 * Returns: 0 on success, -1 on read error.
 */
int
load_image_cache()
{
	int  i;
	FILE *cache_file;
	
	
	if( (cache_file = fopen( image_cache_path, "r")) == NULL ) {
		if( errno != ENOENT ) {
			thor_errlog( LOG_ERR, "Could not open image cache file");
			return -1;
		}
		return 0;
	}
	
	fread( image_cache, sizeof(image_cache_t), IMAGE_CACHE_SIZE, cache_file);
	fclose( cache_file);
	
	for( i = 0; i < IMAGE_CACHE_SIZE; i++ ) {
		if( image_cache[i].used ) {
			cairo_status_t  status;
			cairo_matrix_t  scaling;
			cairo_surface_t *surface = cairo_image_surface_create_from_png( image_cache[i].filename);
			cairo_pattern_t *pattern = cairo_pattern_create_for_surface( surface);
			
			
			if( (status = cairo_pattern_status( pattern)) != CAIRO_STATUS_SUCCESS ) {
				cairo_surface_destroy( surface);
				cairo_pattern_destroy( pattern);
				thor_log( LOG_ERR, "Could not create pattern for file '%s': %s", image_cache[i].filename,
		                  cairo_status_to_string( status));
		        image_cache[i].used = 0;
			}
			else {
#ifdef VERBOSE
				thor_log( LOG_DEBUG, "Created image cache for '%s'...", image_cache[i].filename);
#endif
				cairo_matrix_init_scale( &scaling, cairo_image_surface_get_width( surface),
	                                     cairo_image_surface_get_height( surface));
				cairo_pattern_set_matrix( pattern, &scaling);
				cairo_surface_destroy( surface);
				image_cache[i].pattern = pattern;
			}
		}
	}
	
	return 0;
};
			
			
			
			
/*
 * Saves image_cache in a file and frees the array.
 * Returns: 0 on success, -1 on error.
 */
int
save_image_cache()
{
	int  i;
	FILE *cache_file;
	
	
#ifdef VERBOSE
	thor_log( LOG_DEBUG, "Saving image cache...");
#endif

	if( (cache_file = fopen( image_cache_path, "w")) == NULL ) {
		thor_errlog( LOG_ERR, "Could not write out image cache");
		return -1;
	}
	
	fwrite( image_cache, sizeof(image_cache_t), IMAGE_CACHE_SIZE, cache_file);
	fclose( cache_file);
	
	for( i = 0; i < IMAGE_CACHE_SIZE; i++ ) {
		if( image_cache[i].used )
			cairo_pattern_destroy( image_cache[i].pattern);
	}
	
	return 0;
};
