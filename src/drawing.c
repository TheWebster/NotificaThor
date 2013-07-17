/* ************************************************************* *\
 * drawing.c                                                     *
 *                                                               *
 * Project:     NotificaThor                                     *
 * Author:      Christian Weber (ChristianWeber802@gmx.net)      *
 *                                                               *
 * Description: Advanced drawing operations.                     *
\* ************************************************************* */


#include <cairo/cairo.h>
#include <cairo/cairo-xcb.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "cairo_guards.h"
#include "config.h"
#include "text.h"
#include "theme.h"
#include "com.h"
#include "drawing.h"
#include "NotificaThor.h"
#include "logging.h"
#include "images.h"


struct fbs_t fallback_surface;
char         *image_string;

/*
 * Draws a rectangle with rounded corners.
 * 
 * Parameters: cr              - Cairo context.
 *             x, y            - Origin of the rectangle.
 *             width, height   - Extents of rectangle.
 *             rad_tl, rad_tr,
 *             rad_br, rad_bl, - Radii of the rounded corners.
 */
static void
cairo_rounded_rectangle( cairo_t *cr, double x, double y,
                         double width, double height,
                         double rad_tl, double rad_tr, double rad_br, double rad_bl)
{
	rad_tl = ( rad_tl < 0 ) ? 0 : rad_tl;
	rad_tr = ( rad_tr < 0 ) ? 0 : rad_tr;
	rad_br = ( rad_br < 0 ) ? 0 : rad_br;
	rad_bl = ( rad_bl < 0 ) ? 0 : rad_bl;
	
	cairo_new_sub_path( cr);
	cairo_arc( cr, x+rad_tl      , y+rad_tl       , rad_tl, 2*(M_PI/2), 3*(M_PI/2));
	cairo_arc( cr, x+width-rad_tr, y+rad_tr       , rad_tr, 3*(M_PI/2), 4*(M_PI/2));
	cairo_arc( cr, x+width-rad_br, y+height-rad_br, rad_br, 0         , 1*(M_PI/2));
	cairo_arc( cr, x+rad_bl      , y+height-rad_bl, rad_bl, 1*(M_PI/2), 2*(M_PI/2));
	cairo_close_path( cr);
};


/*
 * Adds a solid-colored mesh to a mesh pattern.
 * 
 * Parameters: pattern - Mesh pattern, to which the mesh should be added.
 *             x0, y0,
 *             x1, y1,
 *             x2, y2,
 *             x3, y3  - Coordinates of the mesh.
 *             color   - Color of the mesh.
 */
static void
solid_mesh( cairo_pattern_t *pattern, double x0, double y0, double x1, double y1,
                                      double x2, double y2, double x3, double y3, uint32_t color)
{
	int i;
	
	
	cairo_mesh_pattern_begin_patch( pattern);
	
	cairo_mesh_pattern_move_to( pattern, x0, y0);
	cairo_mesh_pattern_line_to( pattern, x1, y1);
	cairo_mesh_pattern_line_to( pattern, x2, y2);
	cairo_mesh_pattern_line_to( pattern, x3, y3);
	cairo_mesh_pattern_line_to( pattern, x0, y0);
	
	for( i = 0; i < 4; i++ )
		cairo_mesh_pattern_set_corner_color_rgba( pattern, i, cairo_rgba( color));
	
	cairo_mesh_pattern_end_patch( pattern);
};


/*
 * Draws a radial gradient as corner.
 * 
 * Parameters: pattern        - Mesh pattern, to which the mesh should be added.
 *             x0, y0         - Coordinates of outer corner.
 *             dirx, diry     - Radius of corner, direction is controlled by sign.
 *             width          - Border width.
 *             color1, color2 - Colors of the gradient.
 */
static void
gradient_corner( cairo_pattern_t *pattern, double x0, double y0, int dirx, int diry,
        unsigned int width, uint32_t color1, uint32_t color2)
{
	int      radx, rady;
	
	
	if( width > abs( dirx) ) {
		if( dirx > 0 )
			radx = width;
		else
			radx = -width;
		
		if( diry > 0 )
			rady = width;
		else
			rady = -width;
	}
	else {
		radx = dirx;
		rady = diry;
	}
		
	cairo_mesh_pattern_begin_patch( pattern);
	
	cairo_mesh_pattern_move_to( pattern, x0 + dirx, y0);
	cairo_mesh_pattern_curve_to( pattern, x0 + (double)dirx/2, y0,
	                                      x0, y0 + (double)diry/2,
	                                      x0, y0 + diry);
	cairo_mesh_pattern_line_to( pattern, x0 + radx, y0 + rady);
	cairo_mesh_pattern_line_to( pattern, x0 + radx, y0 + rady);
	cairo_mesh_pattern_line_to( pattern, x0 + dirx, y0);
	
	cairo_mesh_pattern_set_corner_color_rgba( pattern, 0, cairo_rgba( color1));
	cairo_mesh_pattern_set_corner_color_rgba( pattern, 1, cairo_rgba( color2));
	cairo_mesh_pattern_set_corner_color_rgba( pattern, 2, cairo_rgba( color2));
	cairo_mesh_pattern_set_corner_color_rgba( pattern, 3, cairo_rgba( color1));
	
	cairo_mesh_pattern_end_patch( pattern);
};


/*
 * Creates a cairo pattern for drawing a split border.
 * 
 * Parameters: surface        - Surface around which a border should be drawn.
 *             x0, y0, x1, y1 - Coordinates of the pattern.
 *             color_t        - Color of top border.
 *             color_r        - Color of right border.
 *             color_b        - Color of bottom border.
 *             color_l        - Color of left border.
 * 
 * Returns: The the pattern containing the bordermap.
 */
static cairo_pattern_t *
bordermap( surface_t *surface, double x0, double y0, double x1, double y1,
           uint32_t color_t, uint32_t color_r, uint32_t color_b, uint32_t color_l)
{
	double in_x0 = x0 + surface->border.width;
	double in_x1 = x1 - surface->border.width;
	double in_y0 = y0 + surface->border.width;
	double in_y1 = y1 - surface->border.width;
	
	cairo_pattern_t *pattern = cairo_pattern_create_mesh();
	
	
	// top mesh
	solid_mesh( pattern, x0, y0, x1, y0, in_x1, in_y0, in_x0, in_y0, color_t);
	// right mesh
	solid_mesh( pattern, x1, y0, x1, y1, in_x1, in_y1, in_x1, in_y0, color_r);
	// bottom mesh
	solid_mesh( pattern, x1, y1, x0, y1, in_x0, in_y1, in_x1, in_y1, color_b);
	// left mesh
	solid_mesh( pattern, x0, y1, x0, y0, in_x0, in_y0, in_x0, in_y1, color_l);
	
	// top-left corner
	gradient_corner( pattern, x0, y0, surface->rad_tl,  surface->rad_tl,  surface->border.width,
	                 color_t, color_l);
	// top-right corner
	gradient_corner( pattern, x1, y0, -surface->rad_tr, surface->rad_tr,  surface->border.width,
	                 color_t, color_r);
	// bottom-right corner
	gradient_corner( pattern, x1, y1, -surface->rad_br, -surface->rad_br, surface->border.width,
	                 color_b, color_r);
	// bottom-left corner
	gradient_corner( pattern, x0, y1, surface->rad_bl,  -surface->rad_bl, surface->border.width,
	                 color_b, color_l);
	
	return pattern;
};


/*
 * Fills cairo's current path with the layers of a surface.
 * 
 * Parameters: cr      - Cairo context.
 *             surface - surface_t containing the layers.
 * 
 * Returns: 0 on success and -1 when nothing can be drawn.
 */
int
set_layer( cairo_t *cr, layer_t *layer)
{
	cairo_pattern_t *pat;
	cairo_status_t  status;
	
	
	/** empty png pattern **/
	if( layer->pattern == NULL ) {
		if( !image_string || !*image_string )
			return -1;
		
		pat    = get_pattern_for_png( image_string);
		if( (status = cairo_pattern_status( pat)) != CAIRO_STATUS_SUCCESS ) {
			thor_log( LOG_ERR, "Reading '%s': %s.", image_string, cairo_status_to_string( status));
			return -1;
		}
		
		image_string += strlen( image_string) + 1;
	}
	else
		pat = layer->pattern;
	
	cairo_set_source( cr, pat);
	cairo_set_operator( cr, layer->operator);
	
	return 0;
};
	
/*
 * Draws a surface.
 * 
 * Parameters: cr            - Cairo context.
 *             surface       - Surface structure.
 *             control       - Control flags.
 *             x, y,
 *             width, height - Coordinates.
 */
void
draw_surface( cairo_t *cr, surface_t *surface, int control,
              double x, double y, double width, double height)
{
	int i = -1;
	
	
	cairo_rounded_rectangle( cr, x, y, width, height, surface->rad_tl, surface->rad_tr,
	                         surface->rad_br, surface->rad_bl);
	
	if( control & CONTROL_USE_MATRIX )
		cairo_restore( cr);
	else {
		cairo_translate( cr, x, y);
		cairo_scale( cr, width, height);
	}
	
	if( surface->nlayers == 0 ) {
		cairo_set_source_rgba( cr, cairo_rgba( fallback_surface.surf_color));
		cairo_set_operator( cr, fallback_surface.surf_op);
		cairo_fill_preserve( cr);
	}
	else {
		for( i = 0; i < surface->nlayers; i++ ) {
			if( set_layer( cr, &surface->layer[i]) == 0 )
				cairo_fill_preserve( cr);
		}
	}
			
	cairo_clip( cr);
	if( control & CONTROL_SAVE_MATRIX )
		cairo_save( cr);
	cairo_identity_matrix( cr);
};


void
draw_border( cairo_t *cr, surface_t *surface, int outer,
             double x, double y, double width, double height)
{
	if( surface->border.width > 0 ) {
		cairo_pattern_t *bmap = NULL;
		double          snap = (double)surface->border.width / 2;
		
		
		if( outer ) {
			x      -= surface->border.width;
			y      -= surface->border.width;
			width  += 2 * surface->border.width;
			height += 2 * surface->border.width;
			
			cairo_rounded_rectangle( cr, x, y, width, height, surface->rad_tl, surface->rad_tr,
									 surface->rad_br, surface->rad_bl);
			cairo_reset_clip( cr);
			cairo_clip( cr);
		}
		
		cairo_translate( cr, x, y);
		cairo_rounded_rectangle( cr, snap, snap,
									 width - surface->border.width, height - surface->border.width,
									 surface->rad_tl - snap, surface->rad_tr - snap,
									 surface->rad_br - snap, surface->rad_bl - snap);
		
		switch( surface->border.type ) {
			case BORDER_TYPE_SOLID:
				bmap = cairo_pattern_create_rgba( cairo_rgba( surface->border.color));
				break;
			
			case BORDER_TYPE_TOPLEFT:
				bmap = bordermap( surface, -0.5, 0, width, height, surface->border.topcolor, surface->border.color,
								  surface->border.color, surface->border.topcolor);
				break;
			
			case BORDER_TYPE_TOPRIGHT:
				bmap = bordermap( surface, 0, 0, width, height, surface->border.topcolor, surface->border.topcolor,
								  surface->border.color, surface->border.color);
				break;
		}
		
		cairo_set_line_width( cr, surface->border.width);
		cairo_set_operator( cr, surface->border.operator);
		cairo_set_source( cr, bmap);
		cairo_stroke( cr);
		
		cairo_pattern_destroy( bmap);
		cairo_identity_matrix( cr);
	}
	
	cairo_reset_clip( cr);
};
	
	
