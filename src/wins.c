/* ************************************************************* *\
 * wins.c                                                        *
 *                                                               *
 * Project:     NotificaThor                                     *
 * Author:      Christian Weber (ChristianWeber802@gmx.net)      *
 *                                                               *
 * Description: Functions for setting up xcb and                 *
 *              the windows.                                     *
\* ************************************************************* */


#define CONFIG_GRAPHICAL

#include <cairo/cairo.h>
#include <cairo/cairo-xcb.h>
#include <errno.h>
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

#include "com.h"
#include "theme.h"
#include "config.h"
#include "wins.h"
#include "wins_private.h"
#include "drawing.h"
#include "NotificaThor.h"
#include "logging.h"
#include "images.h"


xcb_connection_t *con;
thor_window_t    *wins;


static uint32_t         stack_height = PAD_BORDER;
static xcb_screen_t     *screen;
static xcb_visualtype_t *visual = NULL;
static pthread_t        xevents;
static int              has_xshape = 0;

static thor_theme       theme;

/** config from NotificaThor.c **/
extern int  xerror;


/*
 * Handler for timeout.
 */
static void
timeout_handler( union sigval sv)
{
	xcb_unmap_window( con, wins[sv.sival_int].win);
	xcb_flush(con);
	sem_wait( &wins[sv.sival_int].mapped);
	
	if( sv.sival_int < config_notifications ) {
		stack_height -= (wins[sv.sival_int].extents[3] + PAD_WINS);
		remove_note( &wins[sv.sival_int]);
	}
};


/*
 * Sets a timer to an amount of seconds
 * 
 * Parameters: timer   - The ID of the timer to set
 *             seconds - The time to set the timer to
 */
static void
settimer( timer_t timer, double seconds)
{
	struct itimerspec t_spec =
	{
		.it_interval = { 0, 0 },
		.it_value    = { (time_t)seconds, (seconds - (time_t)seconds)*1000000000 }
	};
	
	timer_settime( timer, 0, &t_spec, NULL);
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
			int i;
						
			case XCB_MAP_NOTIFY:
				for( i = 0; i <= config_notifications; i++ ) {
					if( wins[i].win == ((xcb_map_notify_event_t*)event)->window ) {
						sem_post( &wins[i].mapped);
						break;
					}
				}
				break;
		}
		free( event);
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
	int i;
	char                      *env_display;
	int                       scr_nbr;
	xcb_screen_iterator_t     scr_iter;
	xcb_depth_iterator_t      depth_iter;
	xcb_visualtype_iterator_t vt_iter;
	xcb_colormap_t            cmap = 0;
	xcb_intern_atom_cookie_t  wmtype_cookie, note_cookie;
	xcb_intern_atom_reply_t   *wmtype_reply, *note_reply;
	struct sigevent           ev_timeout = {{0}};
	
	uint32_t cw_value[5];
	uint32_t cw_mask;
	uint8_t  cw_depth;
	
	
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
	
	/** get atoms **/
	wmtype_reply = xcb_intern_atom_reply( con, wmtype_cookie, NULL);
	note_reply   = xcb_intern_atom_reply( con, note_cookie  , NULL);
	
	/** set window attributes **/
	if( config_use_argb ) {
		cw_mask     = XCB_CW_BACK_PIXEL|XCB_CW_BORDER_PIXEL|XCB_CW_OVERRIDE_REDIRECT|XCB_CW_EVENT_MASK|XCB_CW_COLORMAP;
		cw_value[0] = 0xff000000;
		cw_value[1] = 0xffffffff;
		cw_value[2] = 1;
		cw_value[3] = XCB_EVENT_MASK_EXPOSURE|XCB_EVENT_MASK_STRUCTURE_NOTIFY;
		cw_value[4] = cmap;
		cw_depth    = 32;
	}
	else {
		cw_mask     = XCB_CW_OVERRIDE_REDIRECT|XCB_CW_EVENT_MASK;
		cw_value[0] = 1;
		cw_value[1] = XCB_EVENT_MASK_EXPOSURE|XCB_EVENT_MASK_STRUCTURE_NOTIFY;
		cw_depth    = XCB_COPY_FROM_PARENT;
	}
	
	/** create wins **/
	wins = (thor_window_t*)malloc( (config_notifications + 1) * sizeof(thor_window_t));
	note_stack = (int*)malloc( config_notifications * sizeof(int));
	for( i = 0; i <= config_notifications; i++ ) {
		wins[i].win = xcb_generate_id( con);
		xcb_create_window( con, cw_depth, wins[i].win, screen->root,
		                   0, 0, 1, 1, 0, XCB_WINDOW_CLASS_COPY_FROM_PARENT,
		                   visual->visual_id, cw_mask, cw_value);
		xcb_change_property( con, XCB_PROP_MODE_REPLACE, wins[i].win, wmtype_reply->atom,
	                         XCB_ATOM_ATOM, 32, 1, &note_reply->atom);
	    
	    /** init "mapped" semaphore **/
		sem_init( &wins[i].mapped, 0, 0);
		
		/** install timer **/
		ev_timeout.sigev_notify           = SIGEV_THREAD;
		ev_timeout.sigev_value.sival_int  = i;
		ev_timeout.sigev_notify_function  = timeout_handler;
		timer_create( CLOCK_REALTIME, &ev_timeout, &wins[i].timer);
		
		/** initialize stack **/
		wins[i].stack_pos = -1;
	}
	
	/** start xevent_loop thread **/
	pthread_create( &xevents, NULL, (void*)xevent_loop, NULL);
	
	free( wmtype_reply);
	free( note_reply);
	
	return 0;
};


/*
 * (Re-)read theme from default_theme.
 */
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
};


/*
 * Render a window as specified in the message.
 * 
 * Parameters: msg - Pointer to thor_message.
 * 
 * Returns: 0 on success and -1 on error.
 */
int
show_win( thor_message *msg)
{
	thor_window_t   *window;
	cairo_t         *cr       = NULL;
	cairo_surface_t *surf_buf = NULL;
	cairo_surface_t *surf_win = NULL;
	
	
	if( msg->flags & COM_NOTE ) {
		if( (window = get_note( msg)) == NULL )
			return -1;
		
		/** get geometry **/
		window->extents[2] = NOTE_WIDTH;
		window->extents[3] = theme.image.height + 2*theme.padtoborder_y;
		
		theme.image.x = theme.padtoborder_x;
		theme.image.y = window->extents[3] / 2 - theme.image.height / 2;
		
		/** set window position (bottom right) **/
		window->extents[0] = screen->width_in_pixels  - (window->extents[2] + PAD_BORDER);
		window->extents[1] = screen->height_in_pixels - stack_height - window->extents[3];
		stack_height      += window->extents[3] + PAD_WINS;
	}
	else {
		window = &wins[config_notifications];
		
		if( (msg->flags & (COM_NO_IMAGE|COM_NO_BAR)) == (COM_NO_IMAGE|COM_NO_BAR) ) {
			thor_log( LOG_DEBUG, "No elements to be drawn.");
			return -1;
		}
		
		/** get custom geometry **/
		window->extents[2] = 0;
		window->extents[3] = theme.padtoborder_y;
		if( theme.custom_dimensions ) {
			if( !(msg->flags & COM_NO_IMAGE) ) {
				window->extents[2] = theme.image.x + theme.image.width;
				window->extents[3] = theme.image.y + theme.image.height;
				theme.bar.x += theme.padtoborder_x;
				theme.bar.y += theme.padtoborder_y;
			}
			if( !(msg->flags & COM_NO_BAR) ) {
				use_largest( &window->extents[2], theme.bar.x + theme.bar.width);
				use_largest( &window->extents[3], theme.bar.y + theme.bar.height);
				theme.bar.x += theme.padtoborder_x;
				theme.bar.y += theme.padtoborder_y;
			}
			window->extents[2] += 2 * theme.padtoborder_x;
			window->extents[3] += 2 * theme.padtoborder_y;
		}
		/** get default geometry **/
		else {
			// dimensions
			if( !(msg->flags & COM_NO_IMAGE) ) {
				theme.image.y = window->extents[3];
				window->extents[2]  = theme.image.width;
				window->extents[3] += theme.image.height;
			}
			if( !(msg->flags & COM_NO_BAR) ) {
				if( window->extents[3] > theme.padtoborder_y )
					window->extents[3] += 20;
				
				theme.bar.y = window->extents[3];
				use_largest( &window->extents[2], theme.bar.width);
				window->extents[3] += theme.bar.height;
			}
			window->extents[2] += 2 * theme.padtoborder_x;
			window->extents[3] += theme.padtoborder_y;
			
			// positions
			theme.image.x = (window->extents[2] / 2) - (theme.image.width / 2);
			theme.bar.x   = (window->extents[2] / 2) - (theme.bar.width / 2);
		}
		
		/** set osd position **/
		if( config_osd_default_x.abs_flag )  // x
			window->extents[0] = config_osd_default_x.coord;
		else
			window->extents[0] = (screen->width_in_pixels  / 2)
			                   - ( window->extents[2] / 2 )
			                   + config_osd_default_x.coord; // x
		
		if( config_osd_default_y.abs_flag )  // y
			window->extents[1] = config_osd_default_y.coord;
		else
			window->extents[1] = (screen->height_in_pixels / 2)
			                   - ( window->extents[3] / 2 )
			                   + config_osd_default_y.coord; // y
	}
	
	/** initialize cairo **/
	surf_win = cairo_xcb_surface_create( con, window->win, visual, window->extents[2], window->extents[3]);
	surf_buf = cairo_image_surface_create( CAIRO_FORMAT_ARGB32, window->extents[2], window->extents[3]);
	cr       = cairo_create( surf_buf);
	
	if( msg->image_len != 0 )
		image_string = msg->image;
	
	/** draw background to buffering surface**/
	fallback_surface.surf_color   = 0xff000000;
	fallback_surface.surf_op      = CAIRO_OPERATOR_OVER;
	draw_surface( cr, &theme.background, 0, 0, 0, window->extents[2], window->extents[3]);
	draw_border( cr, &theme.background, 0, 0, 0, window->extents[2], window->extents[3]);
	
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
	if( !(msg->flags & (COM_NO_BAR|COM_NOTE)) ) {
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
	}
	
	/** wait for window to be mapped **/
	if( sem_trywait( &window->mapped) == -1 ) {
		xcb_map_window( con, window->win);
		xcb_flush( con);
		sem_wait( &window->mapped);
	}
	
	/** configure window x, y, width, height**/
	xcb_configure_window( con, window->win, 15, window->extents);
	
	/** copy buffering surface to window **/
	cr = cairo_create( surf_win);
	cairo_set_source_surface( cr, surf_buf, 0, 0);
	cairo_set_operator( cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint( cr);
	cairo_destroy( cr);
	
	xcb_flush( con);
	
	/** apply SHAPE **/
	if( has_xshape ) {
		xcb_pixmap_t    bm_shape   = xcb_generate_id( con);
		cairo_surface_t *surf_shape;
		
		
		xcb_create_pixmap( con, 1, bm_shape, window->win, window->extents[2], window->extents[3]);
		surf_shape = cairo_xcb_surface_create_for_bitmap( con, screen, bm_shape, window->extents[2], window->extents[3]);
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
		
		xcb_shape_mask( con, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_INPUT, window->win, 0, 0, bm_shape);
		xcb_free_pixmap( con, bm_shape);
		xcb_flush( con);
	}
	
	/** clean up **/
	cairo_surface_destroy( surf_buf);
	cairo_surface_destroy( surf_win);
	sem_post( &window->mapped);
	
	settimer( window->timer, msg->timeout);
	
	return 0;
};
	


/*
 * Cleanup all X related data.
 */
void
cleanup_x()
{
	int i;
	
	
	pthread_cancel( xevents);
	free_image_cache();
	
	/** destroy notes **/
	for( i = 0; i <= config_notifications; i++ ) {
		xcb_destroy_window( con, wins[i].win);
		sem_destroy( &wins[i].mapped);
		timer_delete( wins[i].timer);
	}
	free( wins);
	xcb_flush( con);
	xcb_disconnect( con);
};
