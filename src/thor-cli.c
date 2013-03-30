/* ********************************************** *\
 * thor-cli.c                                     *
 *                                                *
 * Project:     NotificaThor                      *
 * Author:      Christian Weber                   *
 *                                                *
 * Description: Command line client that sends    *
 *              commands to the daemon.           *
\* ********************************************** */


#include <errno.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "com.h"

#define VERSION_STRING "thor-cli v0.1.0"
#define USAGE \
	"usage: thor-cli [options]\n\n" \
	"    -t, --timeout   Timeout for the popup.\n"\
	"    -p, --popup     The popup to display.\n"\
	"    -b, --bar       The state of the bar in form of a fraction ( e.g. \"1/2\").\n"\
	"    -h, --help      No clue.\n"\
	"    -V, --version   Print version info.\n"
	
static const char          optstring[] = "hVt:p:b:";
static const struct option long_opts[] =
{
	{ "timeout", required_argument, NULL, 't'},
	{ "popup"  , required_argument, NULL, 'p'},
	{ "bar"    , required_argument, NULL, 'b'},
	{ "help"   , no_argument      , NULL, 'h'},
	{ "version", no_argument      , NULL, 'V'},
	{ NULL     , 0                , NULL,  0 }
};


char*
cpycat( char* dst, char* src)
{
    while (*src!='\0') *dst++=*src++;
    *dst='\0';
    return dst;
}


static int
parse_bar_progress( char *string, thor_message *msg)
{
	char *endptr;
	
	
	msg->bar_part = strtol( string, &endptr, 10);
	if( *endptr != '/' )
		return -1;
	
	string = endptr + 1;
	msg->bar_elements = strtol( string, &endptr, 10);
	if( *endptr != 0 )
		return -1;
	
	return 0;
};


int
main( int argc, char *argv[])
{
	char               opt;
	int                sockfd;
	struct sockaddr_un saddr;
	thor_message       msg;
	char               *env;
	
	
	memset( &msg, 0, sizeof(thor_message));
	
	while( (opt = getopt_long( argc, argv, optstring, long_opts, NULL)) != -1 )
	{
		char *endptr;
		
		switch( opt )
		{
			case 'h':
				fputs( USAGE, stderr);
				return 0;
			
			case 'V':
				fputs( VERSION_STRING, stderr);
				return 0;
			
			case 't':
				msg.timeout = strtod( optarg, &endptr);
				if( *endptr != '\0' ) {
					fprintf( stderr, "'%s' is not a valid number.\n", optarg);
					return 1;
				}
				break;
			
			case 'p':
				msg.popup_len = strlen( optarg) + 1;
				msg.popup     = optarg;
				break;
			
			case 'b':
				if( parse_bar_progress( optarg, &msg) == -1 ) {
					fprintf( stderr, "'%s' is not a valid expression.\n%s", optarg, USAGE);
					return -1;
				}
				break;
		}
	}
	
	if( (sockfd = socket( AF_UNIX, SOCK_STREAM, 0)) == -1 ) {
		perror( "Creating socket");
		return 1;
	}
	
	env = getenv( "XDG_CACHE_HOME");
	if( !env || !*env) {
		env = getenv( "HOME");
		
		cpycat( cpycat( saddr.sun_path, env), "/.cache/NotificaThor/socket");
	}
	else {
		cpycat( cpycat( saddr.sun_path, env), "/NotificaThor/socket");
	}
	saddr.sun_family = AF_UNIX;
	
	if( connect( sockfd, (struct sockaddr*)&saddr, sizeof(struct sockaddr_un)) == -1 ) {
		perror( "Connecting to server");
		return 1;
	}
	
	if( write( sockfd, &msg, sizeof(thor_message)) == -1 ) {
		perror( "Sending message");
		return 1;
	}
	
	if( msg.popup_len > 0 ) {
		char    ack = 0;
		char    *buffer;
		ssize_t len = msg.popup_len;
		
		
		if( read( sockfd, &ack, 1) == -1 ) {
			perror( "Receiving acknowledge");
			return 1;
		}
		
		buffer = (char*)malloc( len);
		strcpy( buffer, msg.popup);
		// strcpy( buffer + msg.popup_len, other_string);
		
		if( write( sockfd, buffer, len) == -1 ) {
			perror( "Sending string");
			return 1;
		}
		
		free( buffer);
	}
	
	return 0;
};
	
