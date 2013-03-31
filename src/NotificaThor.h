/* ********************************************** *\
 * NotificaThor.h                                 *
 *                                                *
 * Project:     NotificaThor                      *
 * Author:      Christian Weber                   *
 *                                                *
 * Description: String definitions, declaration   *
 *              of functions from 'utils.c'.      *
\* ********************************************** */
#ifndef VERSION
	#error "Define a version!"
#endif

#define APP_NAME "NotificaThor"
#define VERSION_STRING  "NotificaThor "VERSION
#define USAGE\
	"usage: notificathor [OPTIONS]\n\n"\
	"    -l, --logfile    Specify logfile to use instead of syslog.\n"\
	"    -d, --debug      Print debugging messages.\n"\
	"    -n, --nodaemon   Don't fork and print debugging messages to stderr.\n"\
	"    -h, --help       I have no idea.\n"\
	"    -V, --version    Print version info.\n"



/******** utils ********/
void use_largest( uint32_t *dest, uint32_t src);
void go_up( char *string);
char* cpycat(char* dst,char* src);
int _parse_number( char *string, int *number, int allow_neg, char *logmsg, int line);
#define parse_number( string, nptr, allow_neg)   _parse_number( string, nptr, allow_neg, log_msg, line)
#define _realloc( ptr, type, elements)           ptr = (type*)realloc( ptr, elements*sizeof(type))
