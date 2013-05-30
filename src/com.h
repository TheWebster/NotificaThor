/* ************************************************************* *\
 * com.h                                                         *
 *                                                               *
 * Project:     NotificaThor                                     *
 * Author:      Christian Weber (ChristianWeber802@gmx.net)      *
 *                                                               *
 * Description: Function declarations and message struct for     *
 *              communication between daemon and client.         *
\* ************************************************************* */


#define MSG_ACK  6


/***** sosd_message struct *****/
typedef struct
{
	#define COM_QUERY    (1 << 0)
	#define COM_NO_IMAGE (1 << 1)
	#define COM_NO_BAR   (1 << 2)
	uint32_t     flags;
	double       timeout;
	ssize_t      image_len;
	char         *image;
	ssize_t      message_len;
	char         *message;
	unsigned int bar_elements;
	unsigned int bar_part;
} thor_message;


/***** functions *****/
int  receive_message( int fd, thor_message *msg);
void free_message( thor_message *msg);
