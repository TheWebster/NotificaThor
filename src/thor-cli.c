/* ************************************************************* *\
 * thor-cli.c                                                    *
 *                                                               *
 * Project:     NotificaThor                                     *
 * Author:      Christian Weber (ChristianWeber802@gmx.net)      *
 *                                                               *
 * Description: Command line client that sends commands to       *
 *              the daemon.                                      *
\* ************************************************************* */


#include <errno.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "com.h"


#ifndef VERSION
	#error "Define a version!"
#endif

#define VERSION_STRING "thor-cli "VERSION"\n"
#define USAGE \
	"usage: thor-cli [options]\n\n" \
	"    -t, --timeout   Timeout for the popup.\n"\
	"    -b, --bar       The state of the bar in form of a fraction ( e.g. \"1/2\").\n"\
	"    -i, --image     Sends filenames of images to NotificaThor.\n"\
	"    -m, --message   Sends message string to NotificaThor.\n"\
	"        --no-image  Suppresses the image element.\n"\
	"        --no-bar    Suppresses the bar element.\n"\
	"    -h, --help      No clue.\n"\
	"    -V, --version   Print version info.\n"
	
static const char          optstring[] = "hVt:b:i:m:";
static const struct option long_opts[] =
{
	{ "timeout" , required_argument, NULL, 't'},
	{ "image"   , required_argument, NULL, 'i'},
	{ "bar"     , required_argument, NULL, 'b'},
	{ "message" , required_argument, NULL, 'm'},
	{ "no-image", no_argument      , NULL, '0'},
	{ "no-bar"  , no_argument      , NULL, '1'},
	{ "note"    , no_argument      , NULL, 'n'},
	{ "help"    , no_argument      , NULL, 'h'},
	{ "version" , no_argument      , NULL, 'V'},
	{ NULL      , 0                , NULL,  0 }
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
	thor_message       msg = {0};
	char               *env;
	ssize_t            image_len_tmp = 0;
	
	
	while( (opt = getopt_long( argc, argv, optstring, long_opts, NULL)) != -1 )
	{
		char    *endptr;
		
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
			
			case 'i':
				msg.image_len += strlen( optarg) + 1;
				msg.image      = (char*)realloc( msg.image, msg.image_len);
				cpycat( msg.image + image_len_tmp, optarg);
				image_len_tmp += msg.image_len;
				break;
			
			case 'b':
				if( parse_bar_progress( optarg, &msg) == -1 ) {
					fprintf( stderr, "'%s' is not a valid expression.\n%s", optarg, USAGE);
					return -1;
				}
				break;
			
			case 'm':
				msg.message_len = strlen( optarg) + 1;
				msg.message     = optarg;
				break;
			
			case '0': // --no-image
				msg.flags |= COM_NO_IMAGE;
				break;
			
			case '1': // --no-bar
				msg.flags |= COM_NO_BAR;
				break;
			
			case 'n': // notification
				msg.flags |= COM_NOTE;
				break;
		}
	}
	
	msg.image_len++;
	msg.image = (char*)realloc( msg.image, msg.image_len);
	msg.image[msg.image_len] = '\0';
	
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
	
	if( msg.image_len > 0  || msg.message_len > 0 ) {
		char    ack = 0;
		char    *buffer;
		ssize_t len = msg.image_len + msg.message_len;
		
		
		if( read( sockfd, &ack, 1) == -1 ) {
			perror( "Receiving acknowledge");
			return 1;
		}
		
		if( ack != MSG_ACK ) {
			fprintf( stderr, "Protocoll error: Did not receive ACK (0x%x instead) .\n", ack);
			return 1;
		}
		
		buffer = (char*)malloc( len);
		memcpy( buffer, msg.image, msg.image_len);
		memcpy( buffer + msg.image_len, msg.message, msg.message_len);
		
		if( write( sockfd, buffer, len) == -1 ) {
			perror( "Sending string");
			return 1;
		}
		
		if( msg.image_len > 0 )
			free( msg.image);
		free( buffer);
	}
	
	return 0;
};
	
