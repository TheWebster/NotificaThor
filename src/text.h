/* ************************************************************* *\
 * text.h                                                        *
 *                                                               *
 * Project:     NotificaThor                                     *
 * Author:      Christian Weber (ChristianWeber802@gmx.net)      *
 *                                                               *
 * Description: Text API.                                        *
\* ************************************************************* */


// forward declaration
typedef struct text_t_ text_t;

#ifdef TEXT_PRIVATE

struct thor_font
{
	cairo_scaled_font_t  *regular;
	cairo_scaled_font_t  *bold;
	cairo_scaled_font_t  *italic;
	cairo_scaled_font_t  *bold_italic;
	
	cairo_font_extents_t ext;
	double               ul_pos;
	double               ul_width;
};

#endif

typedef struct thor_font thor_font_t;

typedef struct
{
	cairo_scaled_font_t *style;
	double              underlined;    // basically contains x_advance value. Set to zero for no line.
	
	cairo_glyph_t       *glyphs;
	int                 nglyphs;
	
	cairo_glyph_t       *free_glyph;   // free this pointer ONLY!
} text_fragment;


typedef struct
{
	int           nfrags;
} text_word;


typedef struct
{
	int    nwords;
	
	double width;
} text_line;


typedef struct
{
	thor_font_t   *font;
	
	text_line     *line;
	int           nlines;
	
	text_word     *word;
	int           nwords;
	
	text_fragment *frag;
	int           nfrags;
	
	double        width;
	double        height;
} text_box_t;


thor_font_t *init_font( char *font_name);
void        free_font( thor_font_t *font);

text_box_t  *prepare_text( char *text, thor_font_t *font, double fwidth);
void        draw_text( cairo_t *cr, text_box_t *text, text_t *text_theme);
