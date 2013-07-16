/* ************************************************************* *\
 * NotificaThor.c                                                *
 *                                                               *
 * Project:     NotificaThor                                     *
 * Author:      Christian Weber (ChristianWeber802@gmx.net)      *
 *                                                               *
 * Description: Main function, event loop and event handling.    *
\* ************************************************************* */


#define _SOSD_MAIN_

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/inotify.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "com.h"
#include "config.h"
#include "wins.h"
#include "NotificaThor.h"
#include "logging.h"


static sig_atomic_t sig_received = 0;
static int          sockfd = 0;
static char*        socket_path;

int xerror = 0;
int inofd = -1;


#ifdef VERBOSE
#pragma message( "VERBOSE mode defining 'print_message()'...")
static void
print_message( thor_message *msg)
{
	char *str_read = NULL;
	int  i;
	
	
	if( msg->image_len ) {
		str_read = (char*)malloc( msg->image_len);
		memcpy( str_read, msg->image, msg->image_len);
		for( i = 0; i < msg->image_len - 1; i++ ) {
			if( str_read[i] == '\0' && str_read[i+1] != 0 )
				str_read[i] = ':';
		}
	}
	
	thor_log( LOG_DEBUG, "  Query PID = %d", msg->flags & COM_QUERY);
	thor_log( LOG_DEBUG, "  No Image  = %d", (msg->flags & COM_NO_IMAGE) >> 1);
	thor_log( LOG_DEBUG, "  No Bar    = %d", (msg->flags & COM_NO_BAR) >> 2);
	thor_log( LOG_DEBUG, "  Timeout   = %f", msg->timeout);
	thor_log( LOG_DEBUG, "  Image_len = %d", msg->image_len);
	thor_log( LOG_DEBUG, "  Images    = \"%s\"", str_read);
	thor_log( LOG_DEBUG, "  Message   = \"%s\"", msg->message);
	thor_log( LOG_DEBUG, "  Bar       = %d/%d", msg->bar_part, msg->bar_elements);
	
	if( msg->image_len )
		free( str_read);
};
#endif /* VERBOSE */


/*
 * Handles a message on the NotificaThor-socket
 * 
 * Parameters: sockfd - the filedescriptor of the socket
 *             timer  - the timer
 * 
 * Returns: 0 on success, -1 on error.
 */
static int
handle_message( int sockfd)
{
	int            ret = 0;
	fd_set         set;
	int            clsockfd;
	thor_message   msg = {0};
	struct timeval timeout =
	{
		.tv_sec  = 1,
		.tv_usec = 0
	};
	
	
	/** Accepting connections **/
	if( (clsockfd = accept( sockfd, NULL, NULL)) == -1 ) {
		if( !sig_received )
			thor_errlog( LOG_CRIT, "Accepting connections from socket");
		return -1;
	}
	
	/** select to introduce 1 second connection timeout **/
	FD_ZERO( &set);
	FD_SET( clsockfd, &set);
	
	switch( select( clsockfd + 1, &set, NULL, NULL, &timeout) )
	{
		case 0:
			thor_log( LOG_ERR, "Connection timeout.");
			goto end;
		
		case -1:
			if( !sig_received )
				thor_errlog( LOG_CRIT, "select()");
			ret = -1;
			goto end;
	}
	
	/** reading **/
	if( receive_message( clsockfd, &msg) == -1) {
		thor_errlog( LOG_ERR, "Receiving message");
		goto end;
	}
	
#ifdef VERBOSE
	thor_log( LOG_DEBUG, "Received message over socket:");
	print_message( &msg);
#endif /* VERBOSE */
	
	/** query pid **/
	if( msg.flags & COM_QUERY ) {
		pid_t mypid = getpid();
		write( clsockfd, &mypid, sizeof(pid_t));
		goto end;
	}
	
	/** initializing the popup **/
	if( msg.timeout == 0 ) {
		if( msg.flags & COM_NOTE )
			msg.timeout = config_note_default_timeout;
		else
			msg.timeout = config_osd_default_timeout;
	}
		
	show_win( &msg);
		
	free_message( &msg);
	
  end:
	close( clsockfd);
	return ret;
};


/*
 * Signal handler
 */
static void
sig_handler( int sig)
{
	sig_received = sig;
	return;
};


/*
 * Create socket, timer, sighandler and play main loop.
 * 
 * Returns: 0 on success, -1 on error.
 */
static int
event_loop()
{
	int                 ret        = 1;
	struct sigaction    term_sa    = {{0}};
	
	
	/** install signalhandler **/
	term_sa.sa_handler = sig_handler;
	sigaction( SIGINT , &term_sa, NULL);
	sigaction( SIGTERM, &term_sa, NULL);
	sigaction( SIGHUP , &term_sa, NULL);
	
	if( (inofd = inotify_init()) == -1 )
		thor_errlog( LOG_ERR, "Initializing Inotify");
		
	/** parse config file and theme**/
	parse_conf();
	parse_default_theme();
	
	/** prepare window **/
	if( prepare_x() == -1 )
		goto err;
	
	if( listen( sockfd, 5) == -1 ) {
		thor_errlog( LOG_CRIT, "Listening on socket");
		goto err_x;
	}
	
	/** event loop **/
	thor_log( LOG_DEBUG, "NotificaThor started (%d). Awaiting connections.", getpid());
	while( 1 )
	{
		fd_set set;
		int    maxfd = sockfd;
		
		
		FD_ZERO( &set);
		FD_SET( sockfd, &set);
		if( inofd != -1 ) {
			FD_SET( inofd, &set);
			maxfd = ( inofd > maxfd ) ? inofd : maxfd;
		}
		
		
		if( select( maxfd + 1, &set, NULL, NULL, NULL) == -1 ) {
			if( errno != EINTR )
				thor_log( LOG_CRIT, "select()");
			goto err_x;
		}
		
		/** message via thor-cli **/
		if( FD_ISSET( sockfd, &set) ) {
			if( handle_message( sockfd) == -1 )
				goto err_x;
		}
		/** Inotify event **/
		else if( FD_ISSET( inofd, &set) ) {
			thor_log( LOG_DEBUG, "Rereading config file...");
			sleep(1);
			close( inofd);
			cleanup_x();
			
			inofd = inotify_init();
			parse_conf();
			parse_default_theme();
			if( prepare_x() == -1 )
				goto err;
		}
	}
	
	/** cleaning up **/
  err_x:
	cleanup_x();
  err:
	if( sig_received ) {
		if( xerror )
			thor_log( LOG_DEBUG, "X11: IO-Error ocurred (most likely X-Server exited).");
		else {
			thor_log( LOG_DEBUG, "Received signal %s.", strsignal( sig_received));
			ret = 0;
		}
	}
	thor_log( LOG_DEBUG, "Exiting NotificaThor...");
	close( sockfd);
	close( inofd);
	remove( socket_path);
	go_up( socket_path);
	remove( socket_path);
	close_logger();
	
	return ret;
};


/*
 * MAIN FUNCTION.
 * Do option parsing, setup logging method and fork
 */
int
main( int argc, char *argv[])
{
	thor_message        query_msg = { COM_QUERY };
	pid_t               running_pid;
	struct sockaddr_un  saddr;
	char opt;
	char *logfile       = NULL;
	int  logging_method = 0;
	
	const char          optstring[] = "l:vnhV";
	const struct option long_opts[] =
	{
		{ "logfile" , required_argument, NULL, 'l'},
		{ "verbose" , no_argument      , NULL, 'v'},
		{ "nodaemon", no_argument      , NULL, 'n'},
		{ "help"    , no_argument      , NULL, 'h'},
		{ "version" , no_argument      , NULL, 'V'},
		{ NULL      , 0                , NULL, 0  }
	};
	
	
	/** option parsing **/
	while( (opt = getopt_long( argc, argv, optstring, long_opts, NULL)) != -1 )
	{
		switch( opt )
		{
			case 'l':
				logfile = optarg;
				break;
			
			case 'v':
				logging_method |= LOGGER_DEBUG;
				break;
				
			case 'n':
				logging_method |= LOGGER_STDERR;
				break;
			
			case 'h':
				fputs( USAGE, stderr);
				return 0;
			
			case 'V':
				fputs( VERSION_STRING, stderr);
				return 0;
		}
	}
	
	/** opening socket **/
	if( (sockfd = socket( AF_UNIX, SOCK_STREAM, 0)) == -1 ) {
		thor_errlog( LOG_CRIT, "Creating socket");
		return 1;
	}
	
	socket_path = get_xdg_cache();
	mkdir( socket_path, 0755);
	strcat( socket_path, "/NotificaThor");
	mkdir( socket_path, 0700);
	strcat( socket_path, "/socket");
	
	cpycat( saddr.sun_path, socket_path);
	saddr.sun_family = AF_UNIX;
	
  bind_socket:
	if( bind( sockfd, (struct sockaddr*)&saddr, sizeof(struct sockaddr_un)) == -1 ) {
		if( errno == EADDRINUSE ) {
			if( connect( sockfd, (struct sockaddr*)&saddr, sizeof(struct sockaddr_un)) == 0 ) {
				// socket is active
				if( write( sockfd, &query_msg, sizeof(thor_message)) == -1 ) {
					perror( "Sending PID query");
					return 1;
				}
				if( read( sockfd, &running_pid, sizeof(pid_t)) == -1 ) {
					perror( "Receiving PID");
					return 1;
				}
				fprintf( stderr, "There is already an instance of NotificaThor running (%d).\n", running_pid);
				return 1;
			}
			
			// socket is stale
			if( remove( socket_path) == -1 ) {
				perror( "Removing stale socket");
				return 1;
			}
			goto bind_socket;
		}
		else {
			thor_errlog( LOG_CRIT, "Binding socket");
			return 1;
		}
	}
	
	setup_logger( logging_method, logfile);
	
#ifndef TESTING
	chdir( "/");
#endif
	
	/** forking **/
	if( logging_method & LOGGER_STDERR ) {
		if( event_loop() == -1 )
			return 1;
	}
	else {
		switch( fork() )
		{
			case 0:			// child
				fclose( stdin);
				fclose( stdout);
				fclose( stderr);
				
				if( event_loop() == -1 )
					return 1;
				break;
			
			case -1:		// forking failed
				thor_errlog( LOG_CRIT, "fork()");
				return 1;
		}
	}
	
	return 0;
};
