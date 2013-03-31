/* ********************************************** *\
 * config.c                                       *
 *                                                *
 * Project:     NotificaThor                      *
 * Author:      Christian Weber                   *
 *                                                *
 * Description: Config file parsing.              *
\* ********************************************** */


#define _CONFIG_
#define _GRAPHICAL_

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "com.h"
#include "NotificaThor.h"
#include "config.h"
#include "logging.h"


char          _default_theme[MAX_THEME_LEN + 1] = {0};
double        _osd_default_timeout = 2;
coord_t       _osd_default_x = {0, 0};
coord_t       _osd_default_y = {0, 0};
int           _use_argb = 1;


#define MAX_LINE_LEN      FILENAME_MAX + 64

static char log_msg[] = "Parsing config file: line ";
static int  line;

/*
 * Reads pre-formated lines to a buffer.
 * 
 * Parameters: stream - File stream to read from.
 *             buffer - Buffer to read to.
 * 
 * Returns: '\n'           - A line was read successfully.
 *          any other char - Line was to long.
 *          '\0'           - Empty line.
 *          EOF            - EOF was reached before '\n'.
 */
static int
fgetline( FILE *stream, char *buffer)
{
	int c, i = 0;
	
	
	*buffer = '\0';
	while( i < MAX_LINE_LEN )
	{
		c = fgetc( stream);		
		
		if( c == ' ' || c == '\t' );
		else if( c == '\n' ) {
			line++;
			break;
		}
		else if( c == EOF ) {
			if( i == 0 )
				return -1;
			else
				break;
		}
		else
			buffer[i++] = c;
	}
	
	buffer[i] = '\0';
	return c;
};


/*
 * Parses the config file.
 * 
 * Parameters: config_file - Path to config file.
 * 
 * Returns: 0 on success, -1 on error.
 */
int
parse_conf( char *config_file)
{
	FILE *fconf;
	char buffer[MAX_LINE_LEN + 1];
	int  c;
	
	
	if( (fconf = fopen( config_file, "r")) == NULL ) {
		thor_errlog( LOG_ERR, "Opening config file");
		return -1;
	}
	
	while( (c = fgetline( fconf, buffer)) != -1 )
	{
		char *key, *value;
		void *target = NULL;
		
		
		line++;
		if( c != '\n') {
			if( !feof( fconf) ) {
				thor_log( LOG_ERR, "%s%d - line too long ( MAX = %d ).", log_msg, line, MAX_LINE_LEN);
				while( (c = fgetc( fconf)) != '\n' && c != EOF );
				continue;
			}
		}
		if( *buffer == '\0' )
			continue;
			
		/** get the key **/
		key = strtok_r( buffer, "=", &value);
		if( *key == '#' )     // comment
			continue;
		
		if( strcmp( key, "use_argb") == 0 )
			target = (void*)&_use_argb;
		else if( strcmp( key, "default_theme") == 0 )
			target = (void*)_default_theme;
		else if( strcmp( key, "osd_default_timeout") == 0 )
			target = (void*)&_osd_default_timeout;
		else if( strcmp( key, "osd_default_x") == 0 )
			target = (void*)&_osd_default_x;
		else if( strcmp( key, "osd_default_y") == 0 )
			target = (void*)&_osd_default_y;
		else {
			thor_log( LOG_ERR, "%s%d - unknown key '%s'.", log_msg, line, key);
		}
		
		if( *value == '\0' ) {
			thor_log( LOG_ERR, "%s%d - missing value.", log_msg, line);
			continue;
		}
		
		if( target == (void*)&_use_argb ) {
			if( strcmp( value, "0")         == 0 ||
			    strcasecmp( value, "false") == 0 ||
			    strcasecmp( value, "no")    == 0 )
				_use_argb = 0;
			else if( strcmp( value, "1")        == 0 ||
			         strcasecmp( value, "true") == 0 ||
			         strcasecmp( value, "yes")  == 0 )
				_use_argb = 1;
			else
				thor_log( LOG_ERR, "%s%d - '%s' is not a valid boolean expression.", log_msg, line, value);
		}
		else if( target == (void*)_default_theme)
			strncpy( (char*)target, value, MAX_THEME_LEN);
		else if( target == (void*)&_osd_default_timeout ) {
			char   *endptr;
			double to = strtod( value, &endptr);
			
			if( *endptr == 0 )
				*(double*)target = to;
			else
				thor_log( LOG_ERR, "%s%d - '%s' is not a valid number.", log_msg, line, value);
		}
		else if( target == (void*)&_osd_default_x || target == (void*)&_osd_default_y ) {
			int absflag = 0;
			
			if( *value == ':' ) {
				absflag = 1;
				value++;
			}
			
			if( parse_number( value, &((coord_t*)target)->coord, 1) == 0 )
				((coord_t*)target)->abs_flag = absflag;
		}
	}
	fclose( fconf);
	return 0;
};
