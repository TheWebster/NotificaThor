/* ************************************************************* *\
 * config.h                                                      *
 *                                                               *
 * Project:     NotificaThor                                     *
 * Author:      Christian Weber (ChristianWeber802@gmx.net)      *
 *                                                               *
 * Description: Extern config variable declarations.             *
\* ************************************************************* */


int parse_conf();

/** config variables **/
typedef struct
{
	int           coord;
	int           abs_flag;
} coord_t;

#define MAX_THEME_LEN 64
extern char   config_default_theme[];
extern double config_osd_default_timeout;

#ifdef CONFIG_GRAPHICAL

#define COORDS_ABSOLUTE( coord)    (coord & 0xfffffffe)
extern coord_t config_osd_default_x;
extern coord_t config_osd_default_y;
extern int     config_use_argb;
extern int     config_use_xshape;
#define MAX_FONT_LEN 64
extern char    config_default_font[];
	
#endif
	
