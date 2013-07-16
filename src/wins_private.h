/* ************************************************************* *\
 * wins_private.h                                                *
 *                                                               *
 * Project:     NotificaThor                                     *
 * Author:      Christian Weber (ChristianWeber802@gmx.net)      *
 *                                                               *
 * Description: Private declarations for wins.c and              *
 *              wins_notifications.c.
\* ************************************************************* */


#define PAD_WINS   10
#define PAD_BORDER 20
#define NOTE_WIDTH 300

typedef struct
{
	xcb_window_t win;        // XCB window
	uint32_t     extents[4]; // x, y, width, height
	timer_t      timer;      // timer for display timeout
	sem_t        mapped;     // value is 0 when window is unmapped and 1 if mapped
	int          stack_pos;  // Position in stack
} thor_window_t;


extern thor_window_t    *wins;
extern xcb_connection_t *con;
extern int              *note_stack;
extern int              next_stack;
extern uint32_t         stack_height;

thor_window_t *get_note( thor_message *msg);
void          remove_note( thor_window_t *window);
