/* ************************************************************* *\
 * config.c                                                      *
 *                                                               *
 * Project:     NotificaThor                                     *
 * Author:      Christian Weber (ChristianWeber802@gmx.net       *
 *                                                               *
 * Description: Config file parsing.                             *
\* ************************************************************* */


#define CONFIG_GRAPHICAL

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/inotify.h>

#include "com.h"
#include "NotificaThor.h"
#include "config.h"
#include "logging.h"


char          config_default_theme[MAX_THEME_LEN + 1] = {0};
double        config_osd_default_timeout              = 2;
coord_t       config_osd_default_x                    = {0, 0};
coord_t       config_osd_default_y                    = {0, 0};
int           config_use_argb                         = 1;
int           config_use_xshape                       = 0;
char          config_default_font[MAX_FONT_LEN + 1]   = "-12";


#define MAX_LINE_LEN      FILENAME_MAX + 64

static char log_msg[] = "Parsing config file: line ";
static int  line;

extern int inofd;

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
 * Parse boolean expression.
 * 
 * Parameters: string - Boolean expression to parse.
 *             target - Target to store value into.
 */
static void
parse_bool( char *string, int *target)
{
	if( strcmp( string, "0")         == 0 ||
	    strcasecmp( string, "false") == 0 ||
	    strcasecmp( string, "no")    == 0 )
		 *target = 0;
	else if( strcmp( string, "1")        == 0 ||
	         strcasecmp( string, "true") == 0 ||
	         strcasecmp( string, "yes")  == 0 )
		 *target = 1;
	else
		thor_log( LOG_ERR, "%s%d - '%s' is not a boolean expression.", log_msg, line, string);
};


/*
 * Parse coordinate.
 * 
 * Parameters: string - String to parse.
 *             target - Coordinate to store value in.
 */
static void
parse_coord( char *string, coord_t *target)
{
	int absflag = 0;
			
	if( *string == ':' ) {
		absflag = 1;
		string++;
	}
	
	if( parse_number( string, &target->coord, 1) == 0 )
		target->abs_flag = absflag;
};


/*
 * Parses the config file.
 * 
 * Returns: 0 on success, -1 on error.
 */
int
parse_conf()
{
	FILE *fconf;
	char buffer[MAX_LINE_LEN + 1];
	int  c;
#ifndef TESTING
	char *config_file = get_home_config();
#else
	char config_file[FILENAME_MAX];
#endif
	
	
#ifndef TESTING
	strcat( config_file, "/rc.conf");
	
	if( (fconf = fopen( config_file, "r")) == NULL ) {
#endif
		cpycat( config_file, DEFAULT_CONFIG);
		
		if( (fconf = fopen( config_file, "r")) == NULL ) {
			thor_errlog( LOG_ERR, "Opening config file");
#ifdef VERBOSE
			thor_log( LOG_DEBUG, "DEFAULT CONFIGURATION:\n"
								 "\tuse_argb            = %d\n"
								 "\tuse_xshape          = %d\n"
								 "\tdefault_theme       = \"%s\"\n"
								 "\tosd_default_timeout = %f seconds\n"
								 "\tosd_default_x       = %d, abs = %d\n"
								 "\tosd_default_y       = %d, abs = %d",
					  config_use_argb, config_use_xshape, config_default_theme, config_osd_default_timeout,
					  config_osd_default_x.coord, config_osd_default_x.abs_flag,
					  config_osd_default_y.coord, config_osd_default_y.abs_flag);
#endif
			return -1;
		}
#ifndef TESTING
	}
#endif
	
	thor_log( LOG_DEBUG, "Config file: '%s'", config_file);
	
	if( inofd != -1 ) {
		if( inotify_add_watch( inofd, config_file, IN_MODIFY|IN_DELETE_SELF|IN_MOVE_SELF|IN_CLOSE_WRITE) == -1 )
			thor_ferrlog( LOG_ERR, "Installing Inotify watch on '%s'", config_file);
	}
	
	while( (c = fgetline( fconf, buffer)) != -1 )
	{
		char *key, *value;
		
		
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
		
		if( *value == '\0' ) {
			thor_log( LOG_ERR, "%s%d - missing value.", log_msg, line);
			continue;
		}
		
		if( strcmp( key, "use_argb") == 0 )
			parse_bool( value, &config_use_argb);
		else if( strcmp( key, "use_xshape") == 0 ) {
			if( strcmp( value, "whole") == 0 )
				config_use_xshape = 2;
			else
				parse_bool( value, &config_use_xshape);
		}
		else if( strcmp( key, "default_theme") == 0 )
			strncpy( config_default_theme, value, MAX_THEME_LEN);
		else if( strcmp( key, "default_font") == 0 )
			strncpy( config_default_font, value, MAX_FONT_LEN);
		else if( strcmp( key, "osd_default_timeout") == 0 ) {
			char   *endptr;
			double to = strtod( value, &endptr);
			
			if( *endptr == 0 )
				config_osd_default_timeout = to;
			else
				thor_log( LOG_ERR, "%s%d - '%s' is not a valid number.", log_msg, line, value);
		}
		else if( strcmp( key, "osd_default_x") == 0 )
			parse_coord( value, &config_osd_default_x);
		else if( strcmp( key, "osd_default_y") == 0 )
			parse_coord( value, &config_osd_default_y);
		else {
			thor_log( LOG_ERR, "%s%d - unknown key '%s'.", log_msg, line, key);
		}
	}
	fclose( fconf);
	
#ifdef VERBOSE
	thor_log( LOG_DEBUG, "CONFIGURATION:\n"
						 "\tuse_argb            = %d\n"
						 "\tuse_xshape          = %d\n"
						 "\tdefault_theme       = \"%s\"\n"
						 "\tosd_default_timeout = %f seconds\n"
						 "\tosd_default_x       = %d, abs = %d\n"
						 "\tosd_default_y       = %d, abs = %d",
			  config_use_argb, config_use_xshape, config_default_theme, config_osd_default_timeout,
			  config_osd_default_x.coord, config_osd_default_x.abs_flag,
			  config_osd_default_y.coord, config_osd_default_y.abs_flag);
#endif
	          
	return 0;
};
