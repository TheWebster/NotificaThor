/* ************************************************************* *\
 * theme.h                                                       *
 *                                                               *
 * Project:     NotificaThor                                     *
 * Author:      Christian Weber (ChristianWeber802@gmx.net)      *
 *                                                               *
 * Description: Definition of theme struct and                   *
 *              declaration of functions.                        *
\* ************************************************************* */


/** color macros **/
#define D_ALPHA( color)                    (double)((color & 0xff000000) >> 24)/ 255
#define D_RED( color)                      (double)((color & 0x00ff0000) >> 16)/ 255
#define D_GREEN( color)                    (double)((color & 0x0000ff00) >> 8) / 255
#define D_BLUE( color)                     (double)(color & 0x000000ff)        / 255
#define cairo_rgba( color)                 D_RED( color), D_GREEN( color), D_BLUE( color), D_ALPHA( color)


typedef struct
{
	cairo_operator_t operator;
	
	#define BORDER_TYPE_SOLID    0
	#define BORDER_TYPE_TOPLEFT  1
	#define BORDER_TYPE_TOPRIGHT 2
	int type;
	
	unsigned int  width;
	uint32_t color;
	uint32_t topcolor;
} border_t;

#define PATTYPE_SOLID   0
#define PATTYPE_LINEAR  1
#define PATTYPE_RADIAL  2
#define PATTYPE_PNG     3
typedef struct
{
	cairo_operator_t operator;
	cairo_pattern_t  *pattern;
} layer_t;

typedef struct
{
	#define  SURFACE_MAX_PATTERN  255
	unsigned int nlayers;
	layer_t      *layer;
	
	border_t     border;
	
	unsigned int rad_tl;
	unsigned int rad_tr;
	unsigned int rad_br;
	unsigned int rad_bl;
} surface_t;


typedef struct
{
	unsigned int  x;
	unsigned int  y;
	unsigned int  width;
	unsigned int  height;
	
	#define   FILL_EMPTY_RELATIVE  0
	#define   FILL_FULL_RELATIVE   1
	int fill_rule;
	
	#define   ORIENT_LEFTRIGHT     0
	#define   ORIENT_RIGHTLEFT     1
	#define   ORIENT_BOTTOMTOP     2
	#define   ORIENT_TOPBOTTOM     3
	int orientation;
	
	surface_t empty;
	surface_t full;
} bar_t;


typedef struct
{	
	
	unsigned int x;
	unsigned int y;
	unsigned int width;
	unsigned int height;
	
	surface_t    picture;
} image_t;


typedef struct
{
	unsigned int x;
	unsigned int y;
	unsigned int width;
	unsigned int height;
	
	thor_font_t  *font;
	
	#define ALIGN_LEFT    0
	#define ALIGN_CENTER  1
	#define ALIGN_RIGHT   2
	int          align;
	surface_t    surface;
} text_t;

typedef struct
{
	unsigned int  padtoborder_x;
	unsigned int  padtoborder_y;
	int           custom_dimensions;
	
	surface_t background;
	bar_t     bar;
	image_t   image;
	text_t    text;
} thor_theme;


int  parse_theme( char *name, thor_theme *theme);
void free_theme( thor_theme *theme);
