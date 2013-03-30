
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include "com.h"
#include "NotificaThor.h"
#include "logging.h"
	

static FILE *flog   = NULL;
static int  logmask = LOG_MASK(LOG_CRIT) | LOG_MASK(LOG_ERR) | LOG_MASK(LOG_INFO);


/*
 * Print log message.
 * Parameters: level  - Level of logging (see syslog).
 *             format - Format string (see printf).
 *             ...    - Format arguments.
 */
void
thor_log( int level, const char *format, ...)
{
	if( logmask & LOG_MASK( level) )
	{
		va_list arg_list;
		
		
		va_start( arg_list, format);
		
		if( flog == NULL )            // syslog
			vsyslog( level, format, arg_list);
		
		else {
			time_t t            = time( NULL);
			char   *date_string = ctime( &t);
			
			date_string[24] = '\0';
			
			fprintf( flog, "%s - ", date_string);
			vfprintf( flog, format, arg_list);
			fputc( '\n', flog);
		}
		
		va_end( arg_list);
	}
	
	return;
};


/*
 * Setup a logging method.
 * 
 * Parameters: method  - Method to use for logging.
 *             logfile - If not NULL, use string as logfile.
 */
void
setup_logger( int method, char *logfile)
{
	if( method & LOGGER_DEBUG )
		logmask |= LOG_MASK(LOG_DEBUG);
		
	if( method & LOGGER_STDERR )
		flog = stderr;
	else if( logfile != NULL ) {
		if( (flog = fopen( logfile, "a")) == NULL ) {
			openlog( "NotificaThor", LOG_PID, LOG_USER);
			thor_ferrlog( LOG_ERR, "Opening logfile %s", logfile);
		}
	}
	else
		openlog( "NotificaThor", LOG_PID, LOG_USER);
	
	return;
};


/*
 * Close the logging method.
 */
void
close_logger()
{
	if( flog == NULL )
		closelog();
	else
		fclose( flog);
	
	return;
};
