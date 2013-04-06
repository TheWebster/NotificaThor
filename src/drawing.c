/* ********************************************** *\
 * drawing.c                                      *
 *                                                *
 * Project:     NotificaThor                      *
 * Author:      Christian Weber                   *
 *                                                *
 * Description: Advanced drawing operations.      *
\* ********************************************** */


#include <cairo/cairo.h>
#include <cairo/cairo-xcb.h>
#include <math.h>
#include <stdlib.h>

#include "config.h"
#include "theme.h"
#include "com.h"
#include "drawing.h"
#include "NotificaThor.h"
#include "logging.h"


struct fbs_t fallback_surface;

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
 * Parameters: pattern        - Mesh pattern to use.
 *             x0, y0, x1, y1 - Coordinates of the pattern.
 *             color_t        - Color of top border.
 *             color_r        - Color of right border.
 *             color_b        - Color of bottom border.
 *             color_l        - Color of left border.
 *             surface        - Surface around which a border should be drawn.
 */
static void
bordermap( cairo_pattern_t *pattern, double x0, double y0, double x1, double y1,
           uint32_t color_t, uint32_t color_r, uint32_t color_b, uint32_t color_l,
           surface_t *surface)
{
	double in_x0 = x0 + surface->border.width;
	double in_x1 = x1 - surface->border.width;
	double in_y0 = y0 + surface->border.width;
	double in_y1 = y1 - surface->border.width;
	
	
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
	int i;
	
	
	cairo_rounded_rectangle( cr, x, y, width, height, surface->rad_tl, surface->rad_tr,
	                         surface->rad_br, surface->rad_bl);
	
	if( control & CONTROL_USE_SAVED )
		cairo_restore( cr);
	else {
		cairo_translate( cr, x, y);
		cairo_scale( cr, width, height);
	}
	
	if( surface->nlayers ) {
		for( i = 0; i < surface->nlayers; i++ ) {
			cairo_set_source( cr, surface->layer[i].pattern);
			cairo_set_operator( cr, surface->layer[i].operator);
			cairo_fill_preserve( cr);
		}
	}
	else {    //fallback
		cairo_set_source_rgba( cr, cairo_rgba( fallback_surface.surf_color));
		cairo_set_operator( cr, fallback_surface.surf_op);
		cairo_fill_preserve( cr);
	}
	
	if( control & CONTROL_PRESERVE_MATRIX )
		cairo_save( cr);
	
	cairo_clip( cr);
	
	if( control & CONTROL_PRESERVE_CLIP )
		cairo_save( cr);
	
	cairo_identity_matrix( cr);
	
	if( surface->border.type != BORDER_TYPE_NONE ) {
		cairo_pattern_t *bmap;
		double          snap  = (double)surface->border.width / 2;
		
		
		if( control & CONTROL_OUTER_BORDER ) {
			x      -= surface->border.width;
			y      -= surface->border.width;
			width  += 2*surface->border.width;
			height += 2*surface->border.width;
			
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
		
		if( surface->border.type == BORDER_TYPE_SOLID )
			bmap = cairo_pattern_create_rgba( cairo_rgba( surface->border.color));
		else {
			bmap = cairo_pattern_create_mesh();
			
			if( surface->border.type == BORDER_TYPE_TOPLEFT )
				bordermap( bmap, 0, 0, width, height, surface->border.topcolor, surface->border.color,
				           surface->border.color, surface->border.topcolor, surface);
			else
				bordermap( bmap, 0, 0, width, height, surface->border.topcolor, surface->border.topcolor,
				           surface->border.color, surface->border.color, surface);
				
		}
		
		cairo_set_line_width( cr, surface->border.width);
		cairo_set_operator( cr, surface->border.operator);
		cairo_set_source( cr, bmap);
		cairo_stroke( cr);
		cairo_pattern_destroy( bmap);
	}
	
	cairo_reset_clip( cr);
	cairo_identity_matrix( cr);
};
	
	
	
