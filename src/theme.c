/* ********************************************** *\
 * theme.c                                        *
 *                                                *
 * Project:     NotificaThor                      *
 * Author:      Christian Weber                   *
 *                                                *
 * Description: Theme parsing.                    *
\* ********************************************** */


#include <cairo/cairo.h>
#include <cairo/cairo-xcb.h>
#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "com.h"
#include "theme.h"
#include "NotificaThor.h"
#include "logging.h"
#include "theme_symbols.h"

#define MAX_TOK_LEN      FILENAME_MAX + 128
#define MAX_BLOCK_DEPTH  4


#define t_theme			((thor_theme*)target)
#define t_surface       ((surface_t*)target)
#define t_layer         ((layer_t*)target)
#define t_border        ((border_t*)target)
#define t_bar           ((bar_t*)target)
#define t_image         ((image_t*)target)

static unsigned int     line;
static unsigned int     block_depth;
static char             log_msg[128];
static int              *custom_dim = 0;


/*
 * Gets a token of a NotificaThor themefile.
 * 
 * Parameters: stream - file stream to themefile
 *             buffer - a buffer of size MAX_TOK_LEN, gets filled
 *                      with token.
 * 
 * Returns: the delimiter to indicate type of token
 *          EOF if EOF was encountered
 *          -1 if EOF was encountered and no characters have been read
 *          the last read character if MAX_TOK_LEN was reached
 */
static int
fgettok( FILE *stream, char *buffer)
{
	int           c = 0, i = 0;
	unsigned char slash = 0, comment = 0, ast = 0, quotes = 0;
	
	*buffer = '\0';
	while( i < MAX_TOK_LEN )
	{
		c = fgetc( stream);		
		
		/** comment mode **/
		if( comment ) {
			if( c == '/' && ast ) { // end comment
				comment = 0;
				ast = 0;
			}
			else if( c == '*' )
				ast = 1;
			else
				ast = 0;
		}
		
		/** regular mode **/
		else {
			if( !quotes && (c == ' ' || c == '\t') );  // ignore these
			
			else if( c == '"' ) {  // start quote-mode
				if( quotes )
					quotes = 0;
				else
					quotes = 1;
			}
			else if( c == '/' ) {
				if( slash ) { // comment till end of line
					slash = 0;
					while( (c = fgetc( stream)) != '\n' );
					line++;
				}
				else
					slash = 1;
			}
			else if( c == '*' && slash ) {
				comment = 1;
				slash = 0;
			}
					
			else if( c == '\n' )
				line++;
			else if( c == '{' || c == ';' || c == '}' )
				break;
			else if( c == EOF ) {
				if( i == 0 )
					return -1;
				else
					break;
			}
			else {
				if( slash ) {
					buffer[i++] = '/';
					slash = 0;
				}
					
				buffer[i++] = c;
			}
		}
	}
	
	buffer[i] = '\0';
	return c;
};


/*
 * Takes a string of a 12bit, 16bit, 24bit or 32bit [a]rgb value
 * and stores it in an pointer to an int.
 * 
 * Parameters: ptr   - Pointer to string.
 *             color - Pointer to color.
 * 
 * Returns: 0 on success, -1 on error.
 */
static int
parse_color( char *ptr, uint32_t *color)
{
	char *endptr;
	
	
	if( *ptr != '#' ) {
		thor_log( LOG_ERR, "%s%d - Invalid color format (missing leading '#').", log_msg, line);
		return -1;
	}
	ptr++;
	
	*color = strtoll( ptr, &endptr, 16);
	if( *endptr != '\0' ) {
		thor_log( LOG_ERR, "%s%d - '%s' is not a valid hex value.", log_msg, line, ptr);
		return -1;
	}
	switch( strlen( ptr) ) {
		case 3: // nibble rgb '#fff' -> #f0f0f0
			*color = 0xff000000 | 
			         ((*color&0xf00) << 12) |
			         ((*color&0x0f0) << 8)  |
			         ((*color&0x00f) << 4);
			break;
		
		case 4: // nibble argb '#abcd' -> #a0b0c0d0
			*color = ((*color&0xf000) << 16) |
			         ((*color&0x0f00) << 12) |
			         ((*color&0x00f0) << 8)  |
			         ((*color&0x000f) << 4);
			break;
		
		case 6: // byte rgb
			*color |= 0xff000000;
			break;
		
		case 8: // byte argb
			break;
		
		default: // invalid format
			thor_log( LOG_ERR, "%s%d - '%s' is not a valid color format.", log_msg, line, ptr);
			return -1;
	}
	
	return 0;
};


/*
 * Creates a new layer for a surface.
 * 
 * Parameters: pat_type - The type of the pattern to create.
 *             surface  - The surface to modify.
 *             value    - String with extra data.
 * 
 * Returns: Pointer to new created layer on success.
 *          NULL on error.
 */
static layer_t*
create_layer( unsigned char pat_type, surface_t *surface, char *value)
{
	cairo_pattern_t *pat = NULL;
	cairo_status_t  status;
	unsigned int    newi;
	
	
	if( surface->nlayers == SURFACE_MAX_PATTERN ) {
		thor_log( LOG_ERR, "%s%d - Reached maximum number of patterns.", log_msg, line);
		return NULL;
	}
	
	/** create patterns **/
	if( pat_type == PATTYPE_SOLID ) {
		uint32_t color;
		
		if( parse_color( value, &color) == -1 )
			return NULL;
		
		pat = cairo_pattern_create_rgba( cairo_rgba( color));
	}
	else if( pat_type == PATTYPE_LINEAR ) {
		pat = cairo_pattern_create_linear( 0.5, 0, 0.5, 1);
	}
	else if( pat_type == PATTYPE_RADIAL) {
		pat = cairo_pattern_create_radial( 0.5, 0.5, 0, 0.5, 0.5, 0.5);
	}
	else if( pat_type == PATTYPE_PNG ) {
		cairo_surface_t *surface = cairo_image_surface_create_from_png( value);
		cairo_matrix_t  scaling;
		
		
		if( (status = cairo_surface_status( surface)) != CAIRO_STATUS_SUCCESS ) {
			thor_log( LOG_ERR, "%s%d - '%s' %s.", log_msg, line, value, cairo_status_to_string( status));
			return NULL;
		}
		
		cairo_matrix_init_scale( &scaling, cairo_image_surface_get_width( surface),
		                                   cairo_image_surface_get_height( surface));		
		
		pat = cairo_pattern_create_for_surface( surface);
		cairo_pattern_set_matrix( pat, &scaling);
		cairo_surface_destroy( surface);
	}
	
	if( (status = cairo_pattern_status( pat)) != CAIRO_STATUS_SUCCESS ) {
		thor_log( LOG_ERR, "%s%d - Creating pattern: %s", log_msg, line,
		                   cairo_status_to_string( status));
		return NULL;
	}
	
	newi = surface->nlayers++;
	_realloc( surface->layer, layer_t, surface->nlayers);
	surface->layer[newi].pattern  = pat;
	surface->layer[newi].operator = CAIRO_OPERATOR_OVER;
	
	return &surface->layer[newi];
};


/*
 * Adds a stop to a linear or radial pattern.
 * 
 * Parameters: pattern - Pattern to modify.
 *             value   - Value of the stop.
 *             instant - If set create an instant change.
 * 
 * Returns: 0 on success, -1 on error.
 */
static int
parse_stop( cairo_pattern_t *pattern, char *value, unsigned char instant)
{
	char     *col, *off, *endptr;
	uint32_t color;
	double   offset;
	
	
	col = strtok_r( value, "|", &off);
	if( parse_color( col, &color) == -1 )
		return -1;
	
	offset = strtod( off, &endptr);
	if( *endptr != 0 ) {
		thor_log( LOG_ERR, "%s%d - '%s' is not a valid number.", log_msg, line, off);
		return -1;
	}
	
	if( instant ) {
		int    count;
		double red, green, blue, alpha;
		
		cairo_pattern_get_color_stop_count( pattern, &count);
		cairo_pattern_get_color_stop_rgba( pattern, count - 1, NULL, &red, &green, &blue, &alpha);
		cairo_pattern_add_color_stop_rgba( pattern, offset, red, green, blue, alpha);
	}
	
	cairo_pattern_add_color_stop_rgba( pattern, offset, cairo_rgba( color));
	return 0;
};


/*
 * Rotate a pattern.
 * 
 * Parameters: pattern - Pattern to modify.
 *             value   - String of the angle.
 * 
 * Returns: 0 on success, -1 on error.
 */
static int
parse_angle( cairo_pattern_t *pattern, char *value)
{
	unsigned int   angle;
	cairo_matrix_t rotation;
	
	if( parse_number( value, (int*)&angle, 0) == -1 )
		return -1;
	
	
	/** simple rotation **/
	cairo_matrix_init_translate( &rotation, 0.5, 0.5);
	cairo_matrix_rotate( &rotation, (double)angle*M_PI/180);
	cairo_matrix_translate( &rotation, -0.5, -0.5);
	
	cairo_pattern_set_matrix( pattern, &rotation);
	
	return 0;
};


/*
 * Parses a offset for radial pattern.
 * 
 * Parameters: pattern  - Pattern to modify.
 *             offset_x - String for x offset or NULL.
 *             offset_y - String for y offset or NULL.
 * 
 * Returns: 0 on success, -1 on error.
 */
static int
parse_offset( cairo_pattern_t *pattern, char *offset, int xy)
{
	char           *endptr;
	double         off;
	cairo_matrix_t new_matrix, old_matrix, comb_matrix;
	
	
	cairo_pattern_get_matrix( pattern, &old_matrix);
	
	
	off = strtod( offset, &endptr);
	if( *endptr != 0 ) {
		thor_log( LOG_ERR, "%s%d - '%s' is not a valid number.", log_msg, line, offset);
		return -1;
	}
	
	if( xy == 0 )
		cairo_matrix_init_translate( &new_matrix, off, 0);
	else
		cairo_matrix_init_translate( &new_matrix, 0, off);
		
	cairo_matrix_multiply( &comb_matrix, &old_matrix, &new_matrix);
	cairo_pattern_set_matrix( pattern, &comb_matrix);
	
	return 0;
};


/*
 * Iterates through a list of symbols and assigns corresponding value.
 * 
 * Parameters: target   - Pointer to the target value.
 *             key      - String that should be matched.
 *             sym_list - List of symbols.
 * 
 * Returns: 0 on success, -1 if no symbol was found.
 */
static int
parse_symbol( int *target, char *key, theme_symbol_t *sym_list)
{
	int i = 0;
	
	
	while( strcmp( sym_list[i].string, key) != 0 ) {
		if( sym_list[i].string == NULL ) {
			thor_log( LOG_ERR, "%s%d - Unknown symbol '%s'.", log_msg, line, key);
			return -1;
		}
		i++;
	}
	
	*target = sym_list[i].value;
	return 0;
};


/*
 * Parses a block in braces '{}'.
 * 
 * Parameters: ftheme - Stream of the theme file.
 *             buffer - String buffer to use.
 *             level  - The level to operate on.
 *             target - Pointer to element to operate on, cast to void*.
 * 
 * Returns: 0 on success, -1 on error.
 */
#define LEVEL_THEME   0
#define LEVEL_SURFACE 1
#define LEVEL_LINEAR  2
#define LEVEL_BORDER  3
#define LEVEL_BAR     4
#define LEVEL_IMAGE   5
#define LEVEL_PNG     6
#define LEVEL_RADIAL  7
static int
parse_block( FILE *ftheme, char *buffer, unsigned char level, void *target)
{
	unsigned char eoblock = 0;
	int           c;
	
	
	if( ++block_depth > MAX_BLOCK_DEPTH ) {
		thor_log( LOG_ERR, "%s%d - Maximum block depth (%d) reached.", log_msg, line, MAX_BLOCK_DEPTH);
		return -1;
	}
	
	while( !eoblock && (c = fgettok( ftheme, buffer)) != -1 )
	{
		char *key, *value;
		
		if( c == EOF ) {
			thor_log( LOG_ERR, "%s%d - Unexpected end of file.", log_msg, line);
			return -1;
		}
		
		switch( c ) {
			/** new block **/
			case '{':
				switch( level ) {
					case LEVEL_THEME:
				/** THEME level **/
					if( strcmp( buffer, "BACKGROUND") == 0 ) {
						if( parse_block( ftheme, buffer, LEVEL_SURFACE, (void*)&t_theme->background) == -1 )
							return -1;
					}
					else if( strcmp( buffer, "BAR") == 0 ) {
						if( parse_block( ftheme, buffer, LEVEL_BAR, (void*)&t_theme->bar) == -1 )
							return -1;
					}
					else if( strcmp( buffer, "IMAGE") == 0 ) {
						if( parse_block( ftheme, buffer, LEVEL_IMAGE, (void*)&t_theme->image) == -1 )
							return -1;
					}
					else
						goto no_block;
					break;
				
				/** SURFACE level **/
					case LEVEL_SURFACE:
						if( strcmp( buffer, "linear") == 0 ) {
							layer_t *newlayer = NULL;
							
							if( (newlayer = create_layer( PATTYPE_LINEAR, t_surface, NULL)) == NULL )
								return -1;
							
							if( parse_block( ftheme, buffer, LEVEL_LINEAR, (void*)newlayer) == -1 )
								return -1;
						}
						else if( strcmp( buffer, "radial") == 0 ) {
							layer_t *newlayer = NULL;
							
							if( (newlayer = create_layer( PATTYPE_RADIAL, t_surface, NULL)) == NULL )
								return -1;
							
							if( parse_block( ftheme, buffer, LEVEL_RADIAL, (void*)newlayer) == -1 )
								return -1;
						}
						else if( strncmp( buffer, "png", 3) == 0 ) {
							layer_t *newlayer = NULL;
							char    *file = buffer + 3;
							
							if( (newlayer = create_layer( PATTYPE_PNG, t_surface, file)) == NULL )
								return -1;
							
							if( parse_block( ftheme, buffer, LEVEL_PNG, (void*)newlayer) == -1 )
								return -1;
						}
						else if( strcmp( buffer, "border") == 0 ) {
							if( parse_block( ftheme, buffer, LEVEL_BORDER, (void*)&t_surface->border) == -1 )
								return -1;
						}
						else
							goto no_block;
						break;
				
				/** BAR level **/
					case LEVEL_BAR:
						if( strcmp( buffer, "empty") == 0 ) {
							if( parse_block( ftheme, buffer, LEVEL_SURFACE, (void*)&t_bar->empty) == -1 )
								return -1;
						}
						
						else if( strcmp( buffer, "full") == 0 ) {
							if( parse_block( ftheme, buffer, LEVEL_SURFACE, (void*)&t_bar->full) == -1 )
								return -1;
						}
						else
							goto no_block;
						break;
				
				/** IMAGE level **/
					case LEVEL_IMAGE:
						if( strcmp( buffer, "picture") == 0 ) {
							if( parse_block( ftheme, buffer, LEVEL_SURFACE, (void*)&t_image->picture) == -1 )
								return -1;
						}
						else
							goto no_block;
						break;
				}
				break;
			
			/** key value pairs **/
			case '}':
				eoblock = 1;
				if( *buffer == 0 )
					break;
					
			case ';':
				key = strtok_r( buffer, ":", &value);
				
				switch( level ) {
				/** THEME level **/
					case LEVEL_THEME:
						if( strcmp( key, "pad_to_border_x") == 0 ) {
							if( parse_number( value, (int*)&t_theme->padtoborder_x, 0) == -1 )
								return -1;
						}
						else if( strcmp( key, "pad_to_border_y") == 0 ) {
							if( parse_number( value, (int*)&t_theme->padtoborder_y, 0) == -1 )
								return -1;
						}
						else
							goto no_key;
						break;
				
				/** SURFACE level **/
					case LEVEL_SURFACE:
						if( strcmp( key, "color") == 0 ) {
							if( create_layer( PATTYPE_SOLID, t_surface, value) == NULL )
								return -1;
						}
						else if( strcmp( key, "radius_topleft") == 0 ) {
							if( parse_number( value, (int*)&t_surface->rad_tl, 0) == -1 )
								return -1;
						}
						else if( strcmp( key, "radius_topright") == 0 ) {
							if( parse_number( value, (int*)&t_surface->rad_tr, 0) == -1 )
								return -1;
						}
						else if( strcmp( key, "radius_bottomleft") == 0 ) {
							if( parse_number( value, (int*)&t_surface->rad_bl, 0) == -1 )
								return -1;
						}
						else if( strcmp( key, "radius_bottomright") == 0 ) {
							if( parse_number( value, (int*)&t_surface->rad_br, 0) == -1 )
								return -1;
						}
						else
							goto no_key;
						break;
				
				/** LINEAR level **/
					case LEVEL_LINEAR:
						if( strcmp( key, "stop") == 0 ) {
							if( parse_stop( t_layer->pattern, value, 0) == -1 )
								return -1;
						}
						else if( strcmp( key, "switch") == 0 ) {
							if( parse_stop( t_layer->pattern, value, 1) == -1 )
								return -1;
						}
						else if( strcmp( key, "angle") == 0 ) {
							if( parse_angle( t_layer->pattern, value) == -1 )
								return -1;
						}
						else if( strcmp( key, "operator") == 0 ) {
							if( parse_symbol( (int*)&t_layer->operator, value, operators) == -1 )
								return -1;
						}
						else
							goto no_key;
						break;
				
				/** RADIAL level **/
					case LEVEL_RADIAL:
						if( strcmp( key, "stop") == 0 ) {
							if( parse_stop( t_layer->pattern, value, 0) == -1 )
								return -1;
						}
						else if( strcmp( key, "switch") == 0 ) {
							if( parse_stop( t_layer->pattern, value, 1) == -1 )
								return -1;
						}
						else if( strcmp( key, "offset_x") == 0 ) {
							if( parse_offset( t_layer->pattern, value, 0) == -1 )
								return -1;
						}
						else if( strcmp( key, "offset_y") == 0 ) {
							if( parse_offset( t_layer->pattern, value, 1) == -1 )
								return -1;
						}
						else if( strcmp( key, "operator") == 0 ) {
							if( parse_symbol( (int*)&t_layer->operator, value, operators) == -1 )
								return -1;
						}
						else
							goto no_key;
						break;
				
				/** PNG level **/
					case LEVEL_PNG:
						if( strcmp( key, "operator") == 0 ) {
							if( parse_symbol( (int*)&t_layer->operator, value, operators) == -1 )
								return -1;
						}
						else
							goto no_key;
						break;
				
				/** BORDER level **/
					case LEVEL_BORDER:
						if( strcmp( key, "type") == 0 ) {
							if( parse_symbol( &t_border->type, value, border_types) == -1 )
								return -1;
						}							
						else if( strcmp( key, "width") == 0 ) {
							if( parse_number( value, (int*)&t_border->width, 0) == -1 )
								return -1;
						}
						else if( strcmp( key, "color") == 0 ) {
							if( parse_color( value, &t_border->color) == -1 )
								return -1;
						}
						else if( strcmp( key, "top-color") == 0 ) {
							if( parse_color( value, &t_border->topcolor) == -1 )
								return -1;
						}
						else if( strcmp( key, "operator") == 0 ) {
							if( parse_symbol( (int*)&t_layer->operator, value, operators) == -1 )
								return -1;
						}
						else
							goto no_key;
						break;
				
				/** BAR level **/
					case LEVEL_BAR:
						if( strcmp( key, "x") == 0 ) {
							if( parse_number( value, (int*)&t_bar->x, 0) == -1 )
								return -1;
								
							*custom_dim = 1;
						}
						else if( strcmp( key, "y") == 0 ) {
							if( parse_number( value, (int*)&t_bar->y, 0) == -1 )
								return -1;
							
							*custom_dim = 1;
						}
						else if( strcmp( key, "width") == 0 ) {
							if( parse_number( value, (int*)&t_bar->width, 0) == -1 )
								return -1;
						}
						else if( strcmp( key, "height") == 0 ) {
							if( parse_number( value, (int*)&t_bar->height, 0) == -1 )
								return -1;
						}
						else if( strcmp( key, "fill") == 0 ) {
							if( parse_symbol( &t_bar->fill_rule, value, fill_rules) == -1 )
								return -1;
						}
						else if( strcmp( key, "orientation") == 0 ) {
							if( parse_symbol( &t_bar->orientation, value, orientations) == -1 )
								return -1;
						}
						else
							goto no_key;
						break;
				
				/** IMAGE level **/
					case LEVEL_IMAGE:
						if( strcmp( key, "x") == 0 ) {
							if( parse_number( value, (int*)&t_image->x, 0) == -1 )
								return -1;
								
							*custom_dim = 1;
						}
						else if( strcmp( key, "y") == 0 ) {
							if( parse_number( value, (int*)&t_image->y, 0) == -1 )
								return -1;
							
							*custom_dim = 1;
						}
						else if( strcmp( key, "width") == 0 ) {
							if( parse_number( value, (int*)&t_image->width, 0) == -1 )
								return -1;
						}
						else if( strcmp( key, "height") == 0 ) {
							if( parse_number( value, (int*)&t_image->height, 0) == -1 )
								return -1;
						}
						else
							goto no_key;
						break;
				}
				break;
			
			default:
				thor_log( LOG_ERR, "%s%d - Token too long.", log_msg, line);
				return -1;
		}
		continue;
		
	  no_block:
		thor_log( LOG_ERR, "%s%d - '%s' unknown block in this context.", log_msg, line, buffer);
		continue;
		
	  no_key:
		thor_log( LOG_ERR, "%s%d - '%s' unknown key in this context.", log_msg, line, key);
	}
	
	block_depth--;
	return 0;
  
};


/*
 * Parse themefile.
 * 
 * Parameters: name  - Filename (not path!) of the theme.
 *             theme - Pointer to theme struct.
 * 
 * Returns: 0 on success, -1 on error.
 */
int
parse_theme( char *file, thor_theme *theme)
{
	FILE *ftheme;
	int  ret;
	char *buffer = (char*)malloc( MAX_TOK_LEN + 1);
	
	
	if( (ftheme = fopen( file, "r")) == NULL ) {
		thor_ferrlog( LOG_ERR, "Opening theme '%s'", file);
		return -1;
	}
	
	/** create log message **/
	strcpy( log_msg, "Parsing themefile '");
	strcat( log_msg, file);
	strcat( log_msg, "': line ");
	
	line        = 1;
	block_depth = 0;
	custom_dim  = &theme->custom_dimensions;
	
	ret = parse_block( ftheme, buffer, LEVEL_THEME, (void*)theme);
	
	free( buffer);
	fclose( ftheme);
	return ret;
};


/*
 * Free a surface.
 * 
 * Paramters: surface - Surface to free.
 */
static void
free_surface( surface_t *surface)
{	
	if( surface->nlayers > 0 ) {
		int i;
		
		for( i = 0; i < surface->nlayers; i++ )
			cairo_pattern_destroy( surface->layer[i].pattern);
		
		free( surface->layer);
	}
};


/*
 * Free a theme.
 * 
 * Parameters: theme - Theme to free.
 */
void
free_theme( thor_theme *theme)
{
	/** free background **/
	free_surface( &theme->background);
	
	/** free bar **/
	free_surface( &theme->bar.empty);
	free_surface( &theme->bar.full);
	
	/** free image **/
	free_surface( &theme->image.picture);
};
