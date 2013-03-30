
int parse_conf( char *config_file);

/** config variables **/
typedef struct
{
	int           coord;
	int           abs_flag;
} coord_t;

#define MAX_THEME_LEN 64
extern char         _default_theme[];
extern unsigned int _osd_default_timeout;

#ifdef _GRAPHICAL_

#define COORDS_ABSOLUTE( coord)    (coord & 0xfffffffe)
extern coord_t        _osd_default_x;
extern coord_t        _osd_default_y;
extern int            _use_argb;
	
#endif
	
