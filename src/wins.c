#define _GRAPHICAL_
#include <cairo/cairo.h>
#include <cairo/cairo-xcb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include "com.h"
#include "theme.h"
#include "config.h"
#include "wins.h"
#include "drawing.h"
#include "NotificaThor.h"
#include "logging.h"

static xcb_connection_t *con;
static xcb_screen_t     *screen;
static xcb_visualtype_t *visual = NULL;
static xcb_window_t     osd;
static int              osd_mapped = 0;

/** config from NotificaThor.c **/
extern char *config_path;
extern char *themes_path;

/*
 * Setup all X related data.
 * 
 * Returns: 0 on success, -1 on error.
 */
int
prepare_x()
{
	char                      *env_display;
	int                       scr_nbr;
	xcb_screen_iterator_t     scr_iter;
	xcb_depth_iterator_t      depth_iter;
	xcb_visualtype_iterator_t vt_iter;
	xcb_colormap_t            cmap = 0;
	xcb_intern_atom_cookie_t  wmtype_cookie, note_cookie;
	xcb_intern_atom_reply_t   *wmtype_reply, *note_reply;
	
	uint32_t cw_value[5];
	#define  CW_MASK_ARGB  XCB_CW_BACK_PIXEL|XCB_CW_BORDER_PIXEL|XCB_CW_OVERRIDE_REDIRECT|\
	                       XCB_CW_EVENT_MASK|XCB_CW_COLORMAP
	#define  CW_MASK_RGB   XCB_CW_OVERRIDE_REDIRECT|XCB_CW_EVENT_MASK
	
	
	/** open display **/
	if( (env_display = getenv( "DISPLAY")) == NULL ) {
		thor_log( LOG_CRIT, "DISPLAY variable not set.");
		return -1;
	}
	if( (con = xcb_connect( env_display, &scr_nbr)) == NULL ) {
		thor_log( LOG_CRIT, "Could not connect to display '%s'.", env_display);
		return -1;
	}
	
	/** get screen **/
	scr_iter = xcb_setup_roots_iterator( xcb_get_setup( con));
	while( scr_iter.rem && scr_nbr )
	{
		scr_nbr--;
		xcb_screen_next( &scr_iter);
	}
	screen = scr_iter.data;
	
	
	/** get atoms **/
	wmtype_cookie = xcb_intern_atom( con, 1, 19, "_NET_WM_WINDOW_TYPE");
	note_cookie   = xcb_intern_atom( con, 1, 32, "_NET_WM_WINDOW_TYPE_NOTIFICATION");
	
	/** get visual **/
	depth_iter = xcb_screen_allowed_depths_iterator( screen);
	if( _use_argb ) {
		/** get argb visual **/
		while( depth_iter.rem && depth_iter.data->depth != 32 )
			xcb_depth_next( &depth_iter);
		
		if( depth_iter.rem == 0 ) {
			_use_argb = 0;
			thor_log( LOG_ERR, "32-bit is not allowed on this screen.");
		}
		else {
			vt_iter = xcb_depth_visuals_iterator( depth_iter.data);
			while( vt_iter.rem &&
				   vt_iter.data->_class != XCB_VISUAL_CLASS_TRUE_COLOR &&
				   vt_iter.data->red_mask != 0xff0000 &&
				   vt_iter.data->green_mask != 0xff00 &&
				   vt_iter.data->blue_mask != 0xff )
				xcb_visualtype_next( &vt_iter);
			
			if( vt_iter.rem == 0 ) {
				_use_argb = 0;
				thor_log( LOG_ERR, "No appropriate 32 bit visual found.");
			}
			else {
				visual = vt_iter.data;
				cmap = xcb_generate_id( con);
				xcb_create_colormap( con, XCB_COLORMAP_ALLOC_NONE, cmap, screen->root,
				                     visual->visual_id);
			}
		}
	}
	else {
		/** get ordinary visual **/
		while( depth_iter.rem ) {
			vt_iter = xcb_depth_visuals_iterator( depth_iter.data);
			while( vt_iter.rem) {
				if( vt_iter.data->visual_id == screen->root_visual )
					goto done;
				xcb_visualtype_next( &vt_iter);
			}
			xcb_depth_next( &depth_iter);
		}
	  done:
		visual = vt_iter.data;
	}
	
	/** create window **/
	osd = xcb_generate_id( con);
	if( _use_argb ) {
		/** create argb window **/
		cw_value[0] = 0x00000000;
		cw_value[1] = 0xffffffff;
		cw_value[2] = 1;
		cw_value[3] = XCB_EVENT_MASK_EXPOSURE;
		cw_value[4] = cmap;
		xcb_create_window( con, 32, osd, screen->root,
	                   0, 0, 1, 1, 0, XCB_WINDOW_CLASS_COPY_FROM_PARENT,
	                   visual->visual_id, CW_MASK_ARGB, cw_value);
	}
	else {
		cw_value[0] = 1;
		cw_value[1] = XCB_EVENT_MASK_EXPOSURE;
		xcb_create_window( con, XCB_COPY_FROM_PARENT, osd, screen->root,
		                   0, 0, 500, 500, 0, XCB_WINDOW_CLASS_COPY_FROM_PARENT,
		                   visual->visual_id, CW_MASK_RGB, cw_value);
	}
	
	/** set _NET_WM_WINDOW_TYPE_NOTIFICATION **/
	wmtype_reply = xcb_intern_atom_reply( con, wmtype_cookie, NULL);
	note_reply   = xcb_intern_atom_reply( con, note_cookie  , NULL);
	xcb_change_property( con, XCB_PROP_MODE_REPLACE, osd, wmtype_reply->atom,
	                     XCB_ATOM_ATOM, 32, 1, &note_reply->atom);
	free( wmtype_reply);
	free( note_reply);
	return 0;
};


/*
 * Maps and draws OSD.
 * 
 * Parameters: msg - Pointer to message struct.
 * 
 * Returns: 0 on success, -1 on error.
 */
int
show_osd( thor_message *msg)
{
	thor_theme      theme = {0};
	uint32_t        cval[4] = {0};
	cairo_t         *cr = NULL;
	cairo_surface_t *cr_osd = NULL;
	
	
	/** set default theme **/
	theme.background.border.operator    = CAIRO_OPERATOR_OVER;
	theme.image.picture.border.operator = CAIRO_OPERATOR_OVER;
	theme.bar.empty.border.operator     = CAIRO_OPERATOR_OVER;
	theme.bar.full.border.operator      = CAIRO_OPERATOR_OVER;
	
	/** parse global theme **/
	if( *_default_theme != 0 ) {
		strcat( themes_path, _default_theme);
		parse_theme( themes_path, &theme);
		go_up( themes_path);
	}
	
	/** parse popup from message **/
	if( msg->popup_len != 0 ) {
		strcat( config_path, msg->popup); 
		parse_theme( config_path, &theme);
		go_up( config_path);
	}
		
	/** map and wait for Expose (or parse errors if any) **/
	if( !osd_mapped ) {
		xcb_map_window( con, osd);
		xcb_flush( con);
		while( !osd_mapped )
		{
			xcb_generic_event_t *event = xcb_wait_for_event( con);
			switch( event->response_type )
			{
				case XCB_EXPOSE:
					osd_mapped = 1;
					break;
				/*
				 * error handling
				 */
			}
			free( event);
		}
		thor_log( LOG_DEBUG, "OSD mapped");
	}
	else
		thor_log( LOG_DEBUG, "OSD remapped");
	
	/** get custom geometry **/
	cval[2] = 0;
	cval[3] = theme.padtoborder_y;
	if( theme.custom_dimensions ) {
		if( theme.image.width > 0 && theme.image.height > 0 ) {
			cval[2] = theme.image.x + theme.image.width;
			cval[3] = theme.image.y + theme.image.height;
		}
		if( theme.bar.width > 0 && theme.bar.height > 0 ) {
			use_largest( &cval[2], theme.bar.x + theme.bar.width);
			use_largest( &cval[3], theme.bar.y + theme.bar.height);
		}
	}
	/** get default geometry **/
	else {
		// dimensions
		if( theme.image.width > 0 && theme.image.height > 0 ) {
			theme.image.y = cval[3];
			cval[2] = theme.image.width;
			cval[3] += theme.image.height;
		}
		if( theme.bar.width > 0 && theme.bar.height > 0 ) {
			if( cval[3] > theme.padtoborder_y )
				cval[3] += 20;
			
			theme.bar.y = cval[3];
			use_largest( &cval[2], theme.bar.width);
			cval[3] += theme.bar.height;
		}
		cval[2] += 2 * theme.padtoborder_x;
		cval[3] += theme.padtoborder_y;
		
		// positions
		theme.image.x = (cval[2] / 2) - (theme.image.width / 2);
		theme.bar.x = (cval[2] / 2) - (theme.bar.width / 2);
	}
	
	/** set osd position **/
	if( _osd_default_x.abs_flag )  // x
		cval[0] = _osd_default_x.coord;
	else
		cval[0] = (screen->width_in_pixels  / 2) - ( cval[2] / 2 ) + _osd_default_x.coord; // x
	
	if( _osd_default_y.abs_flag )  // y
		cval[1] = _osd_default_y.coord;
	else
		cval[1] = (screen->height_in_pixels / 2) - ( cval[3] / 2 ) + _osd_default_y.coord; // y
	
	/** configure window x, y, width, height**/
	xcb_configure_window( con, osd, 15, cval);
	
	/** initialize cairo **/
	cr_osd = cairo_xcb_surface_create( con, osd, visual, cval[2], cval[3]);
	cr = cairo_create( cr_osd);
	
	/** draw background **/
	draw_surface( cr, &theme.background, CONTROL_NONE, 0, 0, cval[2], cval[3]);
	
	
	/** draw image **/
	if( theme.image.width > 0 && theme.image.height > 0 ) {
		draw_surface( cr, &theme.image.picture, CONTROL_NONE, theme.image.x, theme.image.y, theme.image.width, theme.image.height);
	}
	
	/** draw bar **/
	if( theme.bar.width > 0 && theme.bar.height > 0 ) {
		double fraction = (double)msg->bar_part / msg->bar_elements;
		int    flags    = 0;
		
		
		if( theme.bar.fill_rule == FILL_EMPTY_RELATIVE )
			flags = CONTROL_PRESERVE_MATRIX|CONTROL_USE_SAVED_MATRIX;
		
		draw_surface( cr, &theme.bar.empty, CONTROL_OUTER_BORDER|(flags & CONTROL_PRESERVE_MATRIX),
		              theme.bar.x, theme.bar.y, theme.bar.width, theme.bar.height);
		switch( theme.bar.orientation ) {
			case ORIENT_LEFTRIGHT:
				draw_surface( cr, &theme.bar.full, flags & CONTROL_USE_SAVED_MATRIX,
				              theme.bar.x, theme.bar.y, theme.bar.width * fraction,
				              theme.bar.height);
				break;
			
			case ORIENT_RIGHTLEFT:
				draw_surface( cr, &theme.bar.full, flags & CONTROL_USE_SAVED_MATRIX,
				              theme.bar.x + theme.bar.width * (1 - fraction),
				              theme.bar.y, theme.bar.width * fraction,
				              theme.bar.height);
				break;
			
			case ORIENT_TOPBOTTOM:
				draw_surface( cr, &theme.bar.full, flags & CONTROL_USE_SAVED_MATRIX,
				              theme.bar.x, theme.bar.y, theme.bar.width,
				              theme.bar.height * fraction);
				break;
				
			case ORIENT_BOTTOMTOP:
				draw_surface( cr, &theme.bar.full, flags & CONTROL_USE_SAVED_MATRIX,
				              theme.bar.x, theme.bar.y + theme.bar.height * (1 - fraction),
				              theme.bar.width, theme.bar.height * fraction);
				break;
		}		
	}
		
	xcb_flush( con);
	
	/** clean up **/
	cairo_destroy( cr);
	cairo_surface_destroy( cr_osd);
	free_theme( &theme);
		
	return 0;
};


/*
 * Cleanup all X related data.
 */
void
cleanup_x()
{
	xcb_destroy_window( con, osd);
	xcb_flush( con);
	xcb_disconnect( con);
	
	return;
};


/*
 * Unmap OSD.
 */
int
kill_osd()
{
	xcb_unmap_window( con, osd);
	xcb_flush( con);
	osd_mapped = 0;
	return 0;
};
