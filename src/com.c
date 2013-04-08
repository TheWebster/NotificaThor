/* ************************************************************* *\
 * com.c                                                         *
 *                                                               *
 * Project:     NotificaThor                                     *
 * Author:      Christian Weber (ChristianWeber802@gmx.net)      *
 *                                                               *
 * Description: Functions for communication                      *
 *              between daemon and client.                       *
\* ************************************************************* */


#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "com.h"


/*
 * Wrapper around read(), that checks size.
 * 
 * Parameters: fd   - Filedescriptor to read.
 *             buf  - Pointer to buffer to read in.
 *             size - Number of bytes to read.
 * 
 * Returns: -1 on error and sets errno, 0 on success.
 */
static ssize_t
read_chksize( int fd, void *buf, ssize_t size)
{
	ssize_t ret = read( fd, buf, size);
	
	if( ret == size ) // success
		return ret;
	else if( ret == 0 )
		errno = ECONNABORTED;
	else if( ret == -1 );
	else
		errno = EBADMSG;
	
	return -1;
};


/*
 * Wrapper around write(), that checks size.
 * 
 * Parameters: fd   - Filedescriptor to write to.
 *             buf  - Pointer to buffer to write from.
 *             size - Number of bytes to write.
 * 
 * Returns: -1 on error and sets errno, 0 on success.
 */
static ssize_t
write_chksize( int fd, void *buf, ssize_t size)
{
	ssize_t ret = write( fd, buf, size);
	
	if( ret == size ) // success
		return ret;
	else if( ret == 0 )
		errno = ECONNABORTED;
	else if( ret == -1 );
	else
		errno = EBADMSG;
	
	return -1;
};


/*
 * Reads a message from filedescriptor.
 * 
 * Parameters: fd  - Filedescriptor to communicate with.
 *             msg - Pointer to message struct.
 * 
 * Returns: 0 on success, -1 on error.
 */
int
receive_message( int fd, thor_message *msg)
{
	ssize_t len = 0;
	char *buffer;
	char ack = MSG_ACK;
	
	/** get struct **/
	if( read_chksize( fd, msg, sizeof(thor_message)) == -1 )
		return -1;
		
	len = msg->popup_len;
	if( len > 0 ) {
		if( write_chksize( fd, &ack, 1) == -1 )
			return -1;
			
		buffer = (char*)malloc( len);
		
		if( read_chksize( fd, buffer, len) == -1 )
			goto err;
		
		msg->popup = buffer;
	}
	
	return 0;
	
  err:
	free( buffer);
	return -1;
};


/*
 * Frees message struct.
 * 
 * Parameters: msg - Pointer to message to free.
 */
void
free_message( thor_message *msg)
{
	if( msg->popup_len > 0 )
		free( msg->popup);
	
	return;
};
