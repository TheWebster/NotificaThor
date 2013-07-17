/* ************************************************************* *\
 * NotificaThor.h                                                *
 *                                                               *
 * Project:     NotificaThor                                     *
 * Author:      Christian Weber (ChristianWeber802@gmx.net)      *
 *                                                               *
 * Description: String definitions, declaration                  *
 *              of functions from 'utils.c'.                     *
\* ************************************************************* */
#ifndef VERSION
	#error "Define a version!"
#endif


#ifndef TESTING
	#define DEFAULT_CONFIG "/etc/NotificaThor/rc.conf"
	#define DEFAULT_THEMES "/etc/NotificaThor/themes/"
#else
	#define DEFAULT_CONFIG "etc/NotificaThor/rc.conf"
	#define DEFAULT_THEMES "etc/NotificaThor/themes/"
#endif


#define APP_NAME "NotificaThor"
#define VERSION_STRING  "NotificaThor "VERSION"\n"
#define USAGE\
	"usage: notificathor [OPTIONS]\n\n"\
	"    -l, --logfile    Specify logfile to use instead of syslog.\n"\
	"    -v, --verbose    Print debugging messages.\n"\
	"    -n, --nodaemon   Don't fork and print debugging messages to stderr.\n"\
	"    -h, --help       I have no idea.\n"\
	"    -V, --version    Print version info.\n"


#define MSG_QUEUE_LEN 16


/******** utils ********/
void go_up( char *string);
char* cpycat(char* dst,char* src);
int _parse_number( char *string, int *number, int allow_neg, char *logmsg, int line);
#define parse_number( string, nptr, allow_neg)   _parse_number( string, nptr, allow_neg, log_msg, line)
#define thor_realloc( ptr, type, elements)           ptr = (type*)realloc( ptr, elements*sizeof(type))
char *get_home_config();
char *get_xdg_cache();
