/* ************************************************************* *\
 * config.h                                                      *
 *                                                               *
 * Project:     NotificaThor                                     *
 * Author:      Christian Weber (ChristianWeber802@gmx.net)      *
 *                                                               *
 * Description: Extern config variable declarations.             *
\* ************************************************************* */


int parse_conf( char *config_file);

/** config variables **/
typedef struct
{
	int           coord;
	int           abs_flag;
} coord_t;

#define MAX_THEME_LEN 64
extern char         _default_theme[];
extern double       _osd_default_timeout;

#ifdef _GRAPHICAL_

#define COORDS_ABSOLUTE( coord)    (coord & 0xfffffffe)
extern coord_t        _osd_default_x;
extern coord_t        _osd_default_y;
extern int            _use_argb;
extern int            _use_xshape;
	
#endif
	
