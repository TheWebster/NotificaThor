/* ********************************************** *\
 * logging.h                                      *
 *                                                *
 * Project:     NotificaThor                      *
 * Author:      Christian Weber                   *
 *                                                *
 * Description: Declaration of logging functions  *
 *              and additional macros.            *
\* ********************************************** */


#define LOGGER_STDERR    1
#define LOGGER_DEBUG     2
void setup_logger( int method, char *logfile);

void close_logger();

void thor_log( int level, const char *format, ...);
#define thor_errlog( level, msg)\
		thor_log( level, msg": %s", strerror( errno));
#define thor_ferrlog( level, format, ...)\
		thor_log( level, format": %s", __VA_ARGS__, strerror( errno))
