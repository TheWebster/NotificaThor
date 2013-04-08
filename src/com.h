/* ************************************************************* *\
 * com.h                                                         *
 *                                                               *
 * Project:     NotificaThor                                     *
 * Author:      Christian Weber (ChristianWeber802@gmx.net       *
 *                                                               *
 * Description: Function declarations and message struct for     *
 *              communication between daemon and client.         *
\* ************************************************************* */


#define MSG_ACK  6


/***** sosd_message struct *****/
typedef struct
{
	double       timeout;
	ssize_t      popup_len;
	char         *popup;
	unsigned int bar_elements;
	unsigned int bar_part;
} thor_message;


/***** functions *****/
int  receive_message( int fd, thor_message *msg);
void free_message( thor_message *msg);
