/* ********************************************** *\
 * utils.c                                        *
 *                                                *
 * Project:     NotificaThor                      *
 * Author:      Christian Weber                   *
 *                                                *
 * Description: Various recurring functions.      *
\* ********************************************** */


#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "logging.h"
#include "NotificaThor.h"


/*
 * Stores the larger value in dest.
 */
void
use_largest( uint32_t *dest, uint32_t src)
{
	if( src > *dest )
		*dest = src;
};


void
go_up( char *string)
{
	string += strlen( string);
	while( *string != '/' )
		string--;
	string[1] = '\0';
}

char*
cpycat( char* dst, char* src)
{
    while (*src!='\0') *dst++=*src++;
    *dst='\0';
    return dst;
}


/*
 * Wrapper around strtol().
 * 
 * Parameters: string    - String to be parsed.
 *             number    - pointer to the number.
 *             allow_neg - If set to 1, allow negative numbers.
 *             logmsg    - Context message for errors.
 *             line      - Line number for errors.
 * 
 * Returns: 0 on success, -1 if string is not a number or, if allow_neg is
 *          not set, if number is negative.
 */
int
_parse_number( char *string, int *number, int allow_neg, char *logmsg, int line)
{
	char *endptr;
	long prenumber;
	
	
	prenumber = strtol( string, &endptr, 10);
	
	if( !allow_neg && prenumber < 0 ) {
		thor_log( LOG_ERR, "%s%d - No negative numbers allowed.", logmsg, line);
		return -1;
	}
	
	if( *endptr != 0 ) {
		thor_log( LOG_ERR, "%s%d - '%s' is not a number.", logmsg, line, string);
		return -1;
	}
	
	*number = prenumber;
	return 0;
};
