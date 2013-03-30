#define _SOSD_MAIN_

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
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
static char         *config_file;
static char         *pidfile;
static char         *socket_path;

char *config_path;
char *themes_path;


/*
 * Sets a timer to an amount of seconds
 * 
 * Parameters: timer   - The ID of the timer to set
 *             seconds - The time to set the timer to
 */
 static void
settimer( timer_t timer, double seconds)
{
	struct itimerspec t_spec =
	{
		.it_interval = { 0, 0 },
		.it_value    = { (time_t)seconds, (seconds - (time_t)seconds)*1000000000 }
	};
	
	timer_settime( timer, 0, &t_spec, NULL);
};


/*
 * Handles a message on the NotificaThor-socket
 * 
 * Parameters: sockfd - the filedescriptor of the socket
 *             timer  - the timer
 * 
 * Returns: 0 on success, -1 on error.
 */
static int
handle_message( int sockfd, timer_t timer)
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
	
	/** initializing the popup **/
	if( msg.timeout == 0 )
		msg.timeout = _osd_default_timeout;
		
	if( show_osd( &msg) == 0 )
		settimer( timer, msg.timeout);
		
	free_message( &msg);
	
  end:
	close( clsockfd);
	return ret;
};


/*
 * Checks for running instances of the daemon via PID file.
 * 
 * Returns: 0 if no instance is running, -1 if an instance
 *          is running or an error occurred.
 */
static int
check_for_running()
{
	pid_t pid;
	FILE  *fpid;
	char  string[10];
	char  *endptr;
	
	
	/** read existing file **/
	if( (fpid = fopen( pidfile, "r")) == NULL ) {
		if( errno == ENOENT )
			goto write_file;
		else {
			thor_errlog( LOG_CRIT, "Opening PID file");
			return -1;
		}
	}
	fgets( string, 9, fpid);
	fclose( fpid);
	if( (pid = strtol( string, &endptr, 10)) != 0 ) {
		if( kill( pid, 0) == 0 ) {
			thor_log( LOG_CRIT, "An instance of NotificaThor is already running ( PID = %d ).", pid);
			return -1;
		}
	}
	remove( socket_path);
	
  write_file:
	mkdir( pidfile, 0755);
	strcat( pidfile, "/"APP_NAME);
	mkdir( pidfile, 0700);
	strcat( pidfile, "/PID");
	
	if( (fpid = fopen( pidfile, "w")) == NULL ) {
		thor_errlog( LOG_CRIT, "Writing PID file");
		return -1;
	}
	fprintf( fpid, "%d", getpid());
	fclose( fpid);
	
	return 0;
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
 * Handler for timeout.
 */
static void
timeout_handler()
{
	thor_log( LOG_DEBUG, "Popup timeout reached.");
	kill_osd();
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
	int                 sockfd = 0;
	struct sockaddr_un  saddr;
	struct sigaction    term_sa    = {{0}};
	struct sigevent     ev_timeout = {{0}};
	timer_t             timer;
	char                *env;
	
		
	/** generate config paths **/
	env = getenv( "XDG_CONFIG_HOME");
	if( !env || !*env ) {
		env = getenv( "HOME");
		
		config_path = (char*)malloc( strlen( env) + sizeof("/.config/"APP_NAME"/") + MAX_THEME_LEN);
		cpycat( cpycat( config_path, env), "/.config/"APP_NAME"/");
	}
	else {
		config_path = (char*)malloc( strlen( env) + sizeof("/"APP_NAME"/") + MAX_THEME_LEN);
		cpycat( cpycat( config_path, env), "/"APP_NAME"/");
	}
	config_file = (char*)malloc( strlen( config_path) + sizeof("rc.conf"));
	cpycat( cpycat( config_file, config_path), "rc.conf");
	
	themes_path = (char*)malloc( strlen( config_path) + sizeof("themes/"));
	cpycat( cpycat( themes_path, config_path), "themes/");
	
	/** generate cache paths **/
	env = getenv( "XDG_CACHE_HOME");
	socket_path = saddr.sun_path;
	if( !env || !*env ) {
		env = getenv( "HOME");
		
		cpycat( cpycat( socket_path, env), "/.cache");
	}
	pidfile = (char*)malloc( strlen( socket_path) + sizeof("/"APP_NAME"/PID"));
	cpycat( pidfile, socket_path);
	strcat( socket_path, "/"APP_NAME"/socket");
	
	/** install signalhandler **/
	term_sa.sa_handler = sig_handler;
	sigaction( SIGINT , &term_sa, NULL);
	sigaction( SIGTERM, &term_sa, NULL);
	sigaction( SIGHUP , &term_sa, NULL);
	
	/** check for running instances **/
	if( check_for_running() == -1 )
		return -1;
	
	/** parse config file **/
	parse_conf( config_file);
	
	/** install timer **/
	ev_timeout.sigev_notify = SIGEV_THREAD;
	ev_timeout.sigev_notify_function = timeout_handler;
	timer_create( CLOCK_REALTIME, &ev_timeout, &timer);
	
	/** prepare window **/
	if( prepare_x() == -1 )
		goto err;
		
	/** opening socket **/
	if( (sockfd = socket( AF_UNIX, SOCK_STREAM, 0)) == -1 ) {
		thor_errlog( LOG_CRIT, "Creating socket");
		goto err_x;
	}
	
	saddr.sun_family = AF_UNIX;
	
	if( bind( sockfd, (struct sockaddr*)&saddr, sizeof(struct sockaddr_un)) == -1 ) {
		thor_errlog( LOG_CRIT, "Binding socket");
		goto err_x;
	}
	if( listen( sockfd, 5) == -1 ) {
		thor_errlog( LOG_CRIT, "Listening on socket");
		goto err_x;
	}
	
	
	thor_log( LOG_DEBUG, "NotificaThor started (%d). Awaiting connections.", getpid());
  loop:
	while( 1 )
	{
		fd_set set;
		
		FD_ZERO( &set);
		FD_SET( sockfd, &set);
		
		if( select( sockfd + 1, &set, NULL, NULL, NULL) == -1 ) {
			if( errno != EINTR )
				thor_log( LOG_CRIT, "select()");
			goto err_x;
		}
		
		if( FD_ISSET( sockfd, &set) ) {
			if( handle_message( sockfd, timer) == -1 )
				goto err_x;
		}		
	}
		
	return 0;
	
	/** cleaning up **/
  err_x:
	cleanup_x();
  err:
	if( sig_received ) {
		thor_log( LOG_DEBUG, "Received signal %s.", strsignal( sig_received));
		if( sig_received == SIGHUP ) {
			parse_conf( config_file);
			thor_log( LOG_DEBUG, "Reread config file.");
			goto loop;
		}
	}
	thor_log( LOG_DEBUG, "Exiting NotificaThor...");
	close( sockfd);
	remove( socket_path);
	remove( pidfile);
	go_up( pidfile);
	remove( pidfile);
	free( config_path);
	free( config_file);
	free( themes_path);
	free( pidfile);
	timer_delete( timer);
	close_logger();
	
	return -1;
};


/*
 * MAIN FUNCTION.
 * Do option parsing, setup logging method and fork
 */
int
main( int argc, char *argv[])
{
	char opt;
	char *logfile       = NULL;
	int  logging_method = 0;
	
	const char          optstring[] = "c:l:vnhV";
	const struct option long_opts[] =
	{
		{ "config"  , required_argument, NULL, 'c'},
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
			case 'c':
				config_file = optarg;
				break;
				
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
	
	setup_logger( logging_method, logfile);
	
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
				chdir( "/");
				
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
