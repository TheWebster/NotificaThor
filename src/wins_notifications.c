/* ************************************************************* *\
 * wins_notifications.c                                          *
 *                                                               *
 * Project:     NotificaThor                                     *
 * Author:      Christian Weber (ChristianWeber802@gmx.net)      *
 *                                                               *
 * Description: Functions for notifications.                     *
\* ************************************************************* */


#include <semaphore.h>
#include <stdint.h>
#include <syslog.h>
#include <unistd.h>
#include <xcb/xcb.h>

#include "logging.h"
#include "com.h"
#include "wins.h"
#include "wins_private.h"
#include "config.h"


int      *note_stack;
int      next_stack   = 0;
uint32_t stack_height = PAD_BORDER;


/*
 * Get an empty window.
 * 
 * Parameters: msg - thor_struct.
 * 
 * Returns: A pointer to a thor_window_t struct, NULL on error.
 */
thor_window_t
*get_note( thor_message *msg)
{
	int i = 0;
	
	
	if( next_stack == config_notifications ) {
		return NULL;
	}
	
	
	while( wins[i].stack_pos != -1 )
		i++;
	
	wins[i].stack_pos     = next_stack;
	note_stack[next_stack] = i;
	next_stack++;
	
	return &wins[i];
};


/*
 * Moves windows above the the removed window down.
 * 
 * Parameters: window - Window to remove.
 */
void
remove_note( thor_window_t *window)
{
	int i = window->stack_pos;
	
	
	next_stack--;
	while( i < next_stack ) {
		// move abstract
		note_stack[i] = note_stack[i + 1];
		i++;
		wins[note_stack[i]].stack_pos--;
		
		// move physically
		wins[note_stack[i]].extents[1] += window->extents[3] + PAD_WINS;
		xcb_configure_window( con, wins[note_stack[i]].win, 15, wins[note_stack[i]].extents);
		xcb_flush( con);
	}
	
	stack_height     -= (window->extents[3] + PAD_WINS);
	window->stack_pos = -1;
	note_stack[i]     = -1;
};
