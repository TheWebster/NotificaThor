/* ************************************************************* *\
 * wins.c                                                        *
 *                                                               *
 * Project:     NotificaThor                                     *
 * Author:      Christian Weber (ChristianWeber802@gmx.net)      *
 *                                                               *
 * Description: Functions for setting up xcb and                 *
 *              the windows.                                     *
\* ************************************************************* */



#include <cairo/cairo.h>
#include <cairo/cairo-xcb.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/shape.h>

#define CONFIG_GRAPHICAL
#include "cairo_guards.h"
#include "com.h"
#include "text.h"
#include "theme.h"
#include "config.h"
#include "wins.h"
#include "drawing.h"
#include "NotificaThor.h"
#include "logging.h"


typedef struct
{
	xcb_window_t win;     // XCB window
	sem_t        mapped;  // value is 0 when window is unmapped and 1 if mapped
} thor_window_t;


static xcb_connection_t *con;
static xcb_screen_t     *screen;
static xcb_visualtype_t *visual = NULL;
static xcb_colormap_t   cmap = 0;
static thor_window_t    osd;
static pthread_t        xevents;
static int              has_xshape = 0;

static thor_theme       theme = {0};

/** config from NotificaThor.c **/
extern int  xerror;


/*
 * Global event loop.
 */
static void
xevent_loop()
{
	while( 1 ) {
		xcb_generic_event_t *event = xcb_wait_for_event( con);
		
		
		if( xcb_connection_has_error( con) ) {
			xerror = 1;
			kill( getpid(), SIGINT);
			return;
		}
			
		switch( event->response_type ) {			
			case XCB_MAP_NOTIFY:
				sem_post( &osd.mapped);
				break;
		}
		free( event);
	}
};


/*
 * Queries X extensions.
 */
void
query_extensions()
{
	const xcb_query_extension_reply_t *qext_reply;
	
	
	// SHAPE extension
	if( config_use_xshape ) {
		qext_reply = xcb_get_extension_data( con, &xcb_shape_id);
		has_xshape = qext_reply->present;
		if( !has_xshape )
			thor_log( LOG_DEBUG, "SHAPE extension not activated.");
	}
};


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
	con = xcb_connect( env_display, &scr_nbr);
	if( xcb_connection_has_error( con) ) {
		thor_log( LOG_CRIT, "Could not connect to display '%s'.", env_display);
		return -1;
	}
	
	/** get screen **/
	scr_iter = xcb_setup_roots_iterator( xcb_get_setup( con));
	while( scr_iter.rem && scr_nbr ){
		scr_nbr--;
		xcb_screen_next( &scr_iter);
	}
	screen = scr_iter.data;
	
	/** Query X extensions **/
	query_extensions();
	
	/** get atoms **/
	wmtype_cookie = xcb_intern_atom( con, 1, 19, "_NET_WM_WINDOW_TYPE");
	note_cookie   = xcb_intern_atom( con, 1, 32, "_NET_WM_WINDOW_TYPE_NOTIFICATION");
	
	/** get visual **/
	depth_iter = xcb_screen_allowed_depths_iterator( screen);
	if( config_use_argb ) {
		/** get argb visual **/
		while( depth_iter.rem && depth_iter.data->depth != 32 )
			xcb_depth_next( &depth_iter);
		
		if( depth_iter.rem == 0 ) {
			config_use_argb = 0;
			thor_log( LOG_ERR, "32-bit is not allowed on this screen.");
		}
		else {
			vt_iter = xcb_depth_visuals_iterator( depth_iter.data);
			while( vt_iter.rem && vt_iter.data->_class != XCB_VISUAL_CLASS_TRUE_COLOR )
				xcb_visualtype_next( &vt_iter);
			
			if( vt_iter.rem == 0 ) {
				config_use_argb = 0;
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
	osd.win = xcb_generate_id( con);
	if( config_use_argb ) {
		/** create argb window **/
		cw_value[0] = 0x00000000;
		cw_value[1] = 0xffffffff;
		cw_value[2] = 1;
		cw_value[3] = XCB_EVENT_MASK_STRUCTURE_NOTIFY;
		cw_value[4] = cmap;
		xcb_create_window( con, 32, osd.win, screen->root,
	                   0, 0, 1, 1, 0, XCB_WINDOW_CLASS_COPY_FROM_PARENT,
	                   visual->visual_id, CW_MASK_ARGB, cw_value);
	}
	else {
		cw_value[0] = 1;
		cw_value[1] = XCB_EVENT_MASK_STRUCTURE_NOTIFY;
		xcb_create_window( con, XCB_COPY_FROM_PARENT, osd.win, screen->root,
		                   0, 0, 1, 1, 0, XCB_WINDOW_CLASS_COPY_FROM_PARENT,
		                   visual->visual_id, CW_MASK_RGB, cw_value);
	}
	
	/** init "mapped" semaphore **/
	sem_init( &osd.mapped, 0, 0);
	
	/** start xevent_loop thread **/
	pthread_create( &xevents, NULL, (void*)xevent_loop, NULL);
	
	/** set _NET_WM_WINDOW_TYPE_NOTIFICATION **/
	wmtype_reply = xcb_intern_atom_reply( con, wmtype_cookie, NULL);
	note_reply   = xcb_intern_atom_reply( con, note_cookie  , NULL);
	xcb_change_property( con, XCB_PROP_MODE_REPLACE, osd.win, wmtype_reply->atom,
	                     XCB_ATOM_ATOM, 32, 1, &note_reply->atom);
	
	/** set WM_NAME **/
	xcb_change_property( con, XCB_PROP_MODE_REPLACE, osd.win, XCB_ATOM_WM_NAME,
	                     XCB_ATOM_STRING, 8, 12, "NotificaThor");
	
	free( wmtype_reply);
	free( note_reply);
	
	return 0;
};


void
parse_default_theme()
{
	free_theme( &theme);
	
	/** set default theme **/
	theme.background.border.operator    = CAIRO_OPERATOR_OVER;
	theme.image.picture.border.operator = CAIRO_OPERATOR_OVER;
	theme.bar.empty.border.operator     = CAIRO_OPERATOR_OVER;
	theme.bar.full.border.operator      = CAIRO_OPERATOR_OVER;
	
	/** parse global theme **/
	if( *config_default_theme != '\0') {
		parse_theme( config_default_theme, &theme);
	}
	/** fallback **/
	else {
		theme.padtoborder_x = 15;
		theme.padtoborder_y = 15;
		
		theme.bar.width     = 200;
		theme.bar.height    = 20;
		
		theme.image.width   = 100;
		theme.image.height  = 100;
	}
	
	if( theme.text.font == NULL ) {
		theme.text.font = init_font( "");
	}
};


#ifdef VERBOSE
#pragma message( "VERBOSE mode defining 'print_coords()'...")
static void
print_coords( uint32_t *cval, thor_theme *theme, text_box_t *text)
{
	thor_log( LOG_DEBUG, "  Window x|y: %u|%u", cval[0], cval[1]);
	thor_log( LOG_DEBUG, "  Window w|h: %u|%u", cval[2], cval[3]);
	thor_log( LOG_DEBUG, "  Image x|y:  %u|%u", theme->image.x, theme->image.y);
	thor_log( LOG_DEBUG, "  Image w|h:  %u|%u", theme->image.width, theme->image.height);
	thor_log( LOG_DEBUG, "  Bar x|y:    %u|%u", theme->bar.x, theme->bar.y);
	thor_log( LOG_DEBUG, "  Bar w|h:    %u|%u", theme->bar.width, theme->bar.height);
	if( text ) {
		thor_log( LOG_DEBUG, "  Text x|y:   %u|%u", theme->text.x, theme->text.y);
		thor_log( LOG_DEBUG, "  Text w|h:   %f|%f", text->width, text->height);
	}
	else
		thor_log( LOG_DEBUG, "  No Text.");
};
#endif

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
	uint32_t        cval[4]   = {0};
	cairo_t         *cr       = NULL;
	cairo_surface_t *surf_buf = NULL;
	cairo_surface_t *surf_osd = NULL;
	text_box_t      *text     = NULL;
	
	
	/** stop here if there is nothing to be done **/
	if( msg->flags & COM_NO_IMAGE && msg->flags & COM_NO_BAR && msg->message_len < 2 ) {
		thor_log( LOG_DEBUG, "No elements to be drawn.");
		return -1;
	}
		
	if( msg->image_len > 0 )
		image_string = msg->image;
	
	/** get custom geometry **/
	cval[2] = 0;
	cval[3] = theme.padtoborder_y;
	if( theme.custom_dimensions ) {
		if( !(msg->flags & COM_NO_IMAGE) ) {
			cval[2] = theme.image.x + theme.image.width;
			cval[3] = theme.image.y + theme.image.height;
			theme.image.x += theme.padtoborder_x;
			theme.image.y += theme.padtoborder_y;
		}
		if( !(msg->flags & COM_NO_BAR) ) {
			cval[2] = ( theme.bar.x + theme.bar.width > cval[2] ) ? theme.bar.x + theme.bar.width
			                                                      : cval[2];
			cval[3] = ( theme.bar.y + theme.bar.height > cval[3] ) ? theme.bar.y + theme.bar.height
			                                                       : cval[3];
			theme.bar.x += theme.padtoborder_x;
			theme.bar.y += theme.padtoborder_y;
		}
		if( msg->message_len > 1 ) {
			text = prepare_text( msg->message, theme.text.font, theme.text.width);
			
			cval[2] = ( theme.text.x + text->width  > cval[2] ) ? theme.text.x + text->width
			                                                    : cval[2];
			cval[3] = ( theme.text.y + text->height > cval[3] ) ? theme.text.y + text->height
			                                                    : cval[3];
			theme.text.x += theme.padtoborder_x;
			theme.text.y += theme.padtoborder_y;
		}
		
		cval[2] += 2 * theme.padtoborder_x;
		cval[3] += 2 * theme.padtoborder_y;
		
		#ifdef VERBOSE
		thor_log( LOG_DEBUG, "Using custom dimensions:");
		#endif
	}
	/** get default geometry **/
	else {
		// dimensions
		if( !(msg->flags & COM_NO_IMAGE) ) {
			theme.image.y = cval[3];
			cval[2]  = theme.image.width;
			cval[3] += theme.image.height;
		}
		if( !(msg->flags & COM_NO_BAR) ) {
			if( cval[3] > theme.padtoborder_y )
				cval[3] += 20;
			
			theme.bar.y = cval[3];
			cval[2]     = ( theme.bar.width > cval[2] ) ? theme.bar.width : cval[2];
			cval[3]    += theme.bar.height;
		}
		if( msg->message_len > 1 ) {
			text         = prepare_text( msg->message, theme.text.font, theme.text.width);
			if( cval[3] > theme.padtoborder_y )
				cval[3] += 20;
			theme.text.y = cval[3];
			cval[2]      = ( text->width > cval[2] ) ? text->width : cval[2];
			cval[3]     += text->height;
		}
		
		cval[2] += 2 * theme.padtoborder_x;
		cval[3] += theme.padtoborder_y;
		
		// positions
		theme.image.x = (cval[2] / 2) - (theme.image.width / 2);
		theme.bar.x   = (cval[2] / 2) - (theme.bar.width / 2);
		if( msg->message_len > 1 ) {
			if( theme.text.align_text == ALIGN_LEFT )
				theme.text.x = theme.padtoborder_x;
			else if( theme.text.align_text == ALIGN_RIGHT )
				theme.text.x = cval[2] - theme.padtoborder_x - text->width;
			else if( theme.text.align_text == ALIGN_CENTER )
				theme.text.x = (cval[2] / 2) - (text->width / 2);
		}
		
		#ifdef VERBOSE
		thor_log( LOG_DEBUG, "Using default dimensions:");
		#endif
	}
	
	/** set osd position **/
	if( config_osd_default_x.abs_flag )  // x
		cval[0] = config_osd_default_x.coord;
	else
		cval[0] = (screen->width_in_pixels  / 2) - ( cval[2] / 2 ) + config_osd_default_x.coord; // x
	
	if( config_osd_default_y.abs_flag )  // y
		cval[1] = config_osd_default_y.coord;
	else
		cval[1] = (screen->height_in_pixels / 2) - ( cval[3] / 2 ) + config_osd_default_y.coord; // y
	
	#ifdef VERBOSE
	print_coords( cval, &theme, text);
	#endif
	
	/** initialize cairo **/
	surf_osd = cairo_xcb_surface_create( con, osd.win, visual, cval[2], cval[3]);
	surf_buf = cairo_image_surface_create( CAIRO_FORMAT_ARGB32, cval[2], cval[3]);
	cr       = cairo_create( surf_buf);
	
	/** draw background to buffering surface**/
	fallback_surface.surf_color   = 0xff000000;
	fallback_surface.surf_op      = CAIRO_OPERATOR_OVER;
	draw_surface( cr, &theme.background, 0, 0, 0, cval[2], cval[3]);
	draw_border( cr, &theme.background, 0, 0, 0, cval[2], cval[3]);
	
	
	/** draw image to buffering surface**/
	if( !(msg->flags & COM_NO_IMAGE) ) {
		fallback_surface.surf_color = 0;
		fallback_surface.surf_op    = CAIRO_OPERATOR_OVER;
		draw_surface( cr, &theme.image.picture, 0, theme.image.x, theme.image.y,
		              theme.image.width, theme.image.height);
		draw_border( cr, &theme.image.picture, 0, theme.image.x, theme.image.y,
		             theme.image.width, theme.image.height);
	}
	/** draw bar draw to buffering surface**/
	if( !(msg->flags & COM_NO_BAR) ) {
		double x      = theme.bar.x;
		double y      = theme.bar.y;
		double width  = theme.bar.width;
		double height = theme.bar.height;
		int    flags;
		double fraction = (double)msg->bar_part / msg->bar_elements;
		
		
		if( fraction > 1 )	
			fraction = 1;
		
		if( theme.bar.fill_rule == FILL_EMPTY_RELATIVE )
			flags = CONTROL_SAVE_MATRIX|CONTROL_USE_MATRIX;
		else
			flags = CONTROL_NONE;
		
		fallback_surface.surf_color = 0;
		draw_surface( cr, &theme.bar.empty, flags & CONTROL_SAVE_MATRIX,
		              theme.bar.x, theme.bar.y, theme.bar.width, theme.bar.height);
		
		fallback_surface.surf_color = 0xffffffff;
		fallback_surface.surf_op    = CAIRO_OPERATOR_DIFFERENCE;
		switch( theme.bar.orientation ) {
			case ORIENT_RIGHTLEFT:
				x += (1 - fraction) * theme.bar.width;
			
			case ORIENT_LEFTRIGHT:
				width = fraction * theme.bar.width;
				break;
			
			case ORIENT_BOTTOMTOP:
				y += (1 - fraction) * theme.bar.height;
			
			case ORIENT_TOPBOTTOM:
				height = fraction * theme.bar.height;
				break;
		}
		draw_surface( cr, &theme.bar.full, flags & CONTROL_USE_MATRIX, x, y, width, height);
		draw_border( cr, &theme.bar.full, 0, x, y, width, height);
		
		draw_border( cr, &theme.bar.empty, 1, theme.bar.x, theme.bar.y,
		             theme.bar.width, theme.bar.height);
		
	}
	/** draw text to buffering surface **/
	if( msg->message_len > 1 ) {
		fallback_surface.surf_color = 0xffffffff;
		fallback_surface.surf_op    = CAIRO_OPERATOR_DIFFERENCE;
		draw_text( cr, text, &theme.text);
	}
	
	cairo_destroy( cr);
	
	/** reset dimensions **/
	if( theme.custom_dimensions ) {
		if( !(msg->flags & COM_NO_IMAGE) ) {
			theme.image.x -= theme.padtoborder_x;
			theme.image.y -= theme.padtoborder_y;
		}
		if( !(msg->flags & COM_NO_BAR) ) {
			theme.bar.x -= theme.padtoborder_x;
			theme.bar.y -= theme.padtoborder_y;
		}
		if( msg->message_len > 1 ) {
			theme.text.x -= theme.padtoborder_x;
			theme.text.y -= theme.padtoborder_y;
		}
	}
	
	/** wait for window to be mapped **/
	if( sem_trywait( &osd.mapped) == -1 ) {
		xcb_map_window( con, osd.win);
		xcb_flush( con);
		sem_wait( &osd.mapped);
	}
	
	/** configure window x, y, width, height**/
	xcb_configure_window( con, osd.win, 15, cval);
	
	/** copy buffering surface to window **/
	cr = cairo_create( surf_osd);
	cairo_set_source_surface( cr, surf_buf, 0, 0);
	cairo_set_operator( cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint( cr);
	cairo_destroy( cr);
	
	xcb_flush( con);
	
	/** apply SHAPE **/
	if( has_xshape ) {
		xcb_pixmap_t    bm_shape   = xcb_generate_id( con);
		cairo_surface_t *surf_shape;
		
		
		xcb_create_pixmap( con, 1, bm_shape, osd.win, cval[2], cval[3]);
		surf_shape = cairo_xcb_surface_create_for_bitmap( con, screen, bm_shape, cval[2], cval[3]);
		cr         = cairo_create( surf_shape);
		
		/** clear bitmap (completely click-through) **/
		cairo_set_operator( cr, CAIRO_OPERATOR_CLEAR);
		cairo_paint( cr);
		
		/** use buffering surface as mask **/
		if( config_use_xshape != 2 ) {
			cairo_set_operator( cr, CAIRO_OPERATOR_OVER);
			cairo_mask_surface( cr, surf_buf, 0, 0);
		}
		
		cairo_destroy( cr);
		cairo_surface_destroy( surf_shape);
		
		xcb_shape_mask( con, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_INPUT, osd.win, 0, 0, bm_shape);
		xcb_flush( con);
	}
	
	/** clean up **/
	cairo_surface_destroy( surf_buf);
	cairo_surface_destroy( surf_osd);
	sem_post( &osd.mapped);
	
	return 0;
};


/*
 * Cleanup all X related data.
 */
void
cleanup_x()
{
	xcb_destroy_window( con, osd.win);
	sem_destroy( &osd.mapped);
	pthread_cancel( xevents);
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
	xcb_unmap_window( con, osd.win);
	sem_wait( &osd.mapped);
	xcb_flush( con);
	return 0;
};


/*
 * Queries the RGB values for a named color from the X Server.
 * Parameters: string - The name of the color.
 *             color  - Pointer to where the color should be stored.
 * Returns: 0 on success, -1 on error.
 */
int
alloc_named_color( char *string, uint32_t *color)
{
	int                            ret       = -1;
	xcb_generic_error_t            *err      = NULL;
	xcb_alloc_named_color_cookie_t color_ck  = xcb_alloc_named_color( con, cmap, strlen(string), string);
	xcb_alloc_named_color_reply_t  *color_rp = xcb_alloc_named_color_reply( con, color_ck, &err);
	
	
	if( err == NULL ) {
		*color = 0xff000000 |
				 ((color_rp->exact_red / 256) << 16) |
				 ((color_rp->exact_green / 256) << 8) |
				 (color_rp->exact_blue / 256);
		ret = 0;
	}
	
	free( color_rp);
	
	return ret;
};
