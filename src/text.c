

#include <cairo/cairo.h>
#include <cairo/cairo-ft.h>
#include <fontconfig/fontconfig.h>
#include <stdint.h>

#define CONFIG_GRAPHICAL
#define TEXT_PRIVATE
#include "text.h"
#include "theme.h"
#include "config.h"
#include "NotificaThor.h"


/*
 * Creates a thor_font_t object by combining config_default_font and
 * font_name to a pattern.
 * 
 * Parameters: font_name - String describing the font used.
 * 
 * Returns: thor_font_t structure.
 */
thor_font_t *
init_font( char *font_name)
{
	double               size;
	cairo_font_face_t    *face;
	cairo_matrix_t       font_matrix, user_matrix;
	cairo_font_options_t *fopts = cairo_font_options_create();
	
	thor_font_t *res = (thor_font_t*)malloc( sizeof(thor_font_t));
	
	FcPattern *fc_default = FcNameParse( (FcChar8*)config_default_font);
	FcPattern *fc_theme   = FcNameParse( (FcChar8*)font_name);
	FcPattern *fc_regular = FcFontRenderPrepare( NULL, fc_theme, fc_default);
	FcPattern *fc_bold    = FcPatternDuplicate( fc_regular);
	FcPattern *fc_italic  = FcPatternDuplicate( fc_regular);
	FcPattern *fc_bitalic = FcPatternDuplicate( fc_regular);
	
	
	FcPatternDestroy( fc_default);
	FcPatternDestroy( fc_theme);
	
	FcPatternAddInteger( fc_bold   , FC_WEIGHT, FC_WEIGHT_BOLD);
	FcPatternAddInteger( fc_italic , FC_SLANT , FC_SLANT_ITALIC);
	FcPatternAddInteger( fc_bitalic, FC_WEIGHT, FC_WEIGHT_BOLD);
	FcPatternAddInteger( fc_bitalic, FC_SLANT , FC_SLANT_ITALIC);
	
	FcConfigSubstitute( NULL, fc_regular, FcMatchFont);
	FcConfigSubstitute( NULL, fc_bold   , FcMatchFont);
	FcConfigSubstitute( NULL, fc_italic , FcMatchFont);
	FcConfigSubstitute( NULL, fc_bitalic, FcMatchFont);
	FcDefaultSubstitute( fc_regular);
	FcDefaultSubstitute( fc_bold);
	FcDefaultSubstitute( fc_italic);
	FcDefaultSubstitute( fc_bitalic);
	
	FcPatternGetDouble( fc_regular, FC_PIXEL_SIZE, 0, &size);
	
	cairo_matrix_init_scale( &font_matrix, size, size);
	cairo_matrix_init_identity( &user_matrix);
	
	// create regular face
	face         = cairo_ft_font_face_create_for_pattern( fc_regular);
	res->regular = cairo_scaled_font_create( face, &font_matrix, &user_matrix, fopts);
	cairo_font_face_destroy( face);
	
	// create bold face
	face         = cairo_ft_font_face_create_for_pattern( fc_bold);
	res->bold    = cairo_scaled_font_create( face, &font_matrix, &user_matrix, fopts);
	cairo_font_face_destroy( face);
	
	// create italic face
	face         = cairo_ft_font_face_create_for_pattern( fc_italic);
	res->italic  = cairo_scaled_font_create( face, &font_matrix, &user_matrix, fopts);
	cairo_font_face_destroy( face);
	
	// create bold-italic face
	face              = cairo_ft_font_face_create_for_pattern( fc_bitalic);
	res->bold_italic  = cairo_scaled_font_create( face, &font_matrix, &user_matrix, fopts);
	cairo_font_face_destroy( face);
	
	cairo_scaled_font_extents( res->regular, &res->ext);
	
	// clean
	FcPatternDestroy( fc_regular);
	FcPatternDestroy( fc_bold);
	FcPatternDestroy( fc_italic);
	FcPatternDestroy( fc_bitalic);
	cairo_font_options_destroy( fopts);
	
	return res;
};


/*
 * Frees thor_font _t object.
 * 
 * Parameters: font - thor_font_t to free.
 */
void
free_font( thor_font_t *font)
{
	if( font != NULL ) {
		cairo_scaled_font_destroy( font->regular);
		cairo_scaled_font_destroy( font->italic);
		cairo_scaled_font_destroy( font->bold_italic);
		cairo_scaled_font_destroy( font->bold);
		
		free( font);
	}
};


/*
 * Allocates a new frag-element and set its glyph array and its font and increases cursor
 * position if the string length is > 0.
 * 
 * Parameters: box    - text_box_t to add the element to.
 *             font   - thor_font_t which contains all font styles.
 *             x, y   - Pointers to the current position of a virtual cursor.
 *                      The x value is automatically increased.
 *             string - A pointer to an UTF8 string.
 *             len    - Length of string.
 *             style  - Flags that indicate the font to be used.
 */
#define STYLE_REGULAR      0
#define STYLE_BOLD         (1 << 1)
#define STYLE_ITALIC       (1 << 2)
#define STYLE_BOLD_ITALIC  (STYLE_BOLD|STYLE_ITALIC)
static void
add_fragment( text_box_t *box, thor_font_t *font, double *x, double *y,
              char *string, int len, int style)
{
	if( len > 0 ) {
		int                  index = box->nfrags++;
		text_fragment        *frag;
		cairo_text_extents_t ext;
		
		
		_realloc( box->frag, text_fragment, box->nfrags);
		frag = &box->frag[index];
		memset( frag, 0, sizeof(text_fragment));
		
		if( style == STYLE_REGULAR ) 
			frag->style = font->regular;
		else if( style == STYLE_BOLD )
			frag->style = font->bold;
		else if( style == STYLE_ITALIC )
			frag->style = font->italic;
		else if( style == STYLE_BOLD_ITALIC )
			frag->style = font->bold_italic;
		
		cairo_scaled_font_text_to_glyphs( frag->style, *x, *y, string, len,
										  &frag->glyphs, &frag->nglyphs,
										  NULL, NULL, NULL);
		cairo_scaled_font_glyph_extents( frag->style, frag->glyphs,
		                                 frag->nglyphs, &ext);
		*x += ext.x_advance; 
	}
};

/*
 * Converts a UTF8-string to a set of glyphs depending on selected font and text
 * formating and returns the results.
 * 
 * Parameters: text - The UTF8-string to convert.
 *             font - The thor_font_t that contains the font.
 * 
 * Returns: text_box_t containing the glyphs and dimensions.
 */
text_box_t *
prepare_text( char *text, thor_font_t *font)
{
	char       *ptr  = text;
	int        style = STYLE_REGULAR;
	double     x     = 0;
	double     y     = font->ext.ascent;
	text_box_t *res  = (text_box_t*)malloc( sizeof(text_box_t));
	
	
	
	memset( res, 0, sizeof(text_box_t));
	
	
	while( *ptr != '\0' ) {
		/** formating elements **/
		if( *ptr == '\n' ) {
			add_fragment( res, font, &x, &y, text, ptr - text, style);
			text       = ptr + 1;
			res->width = ( x > res->width ) ? x : res->width;
			x          = 0;
			y         += font->ext.height;
		}
		
		/** XML markup elements **/
		if( *ptr == '<' ) {
			if( strncmp( ptr, "<b>", 3) == 0 ) {
				add_fragment( res, font, &x, &y, text, ptr - text, style);
				text   = ptr + 3;
				style |= STYLE_BOLD;
			}
			else if( strncmp( ptr, "<i>", 3) == 0 ) {
				add_fragment( res, font, &x, &y, text, ptr - text, style);
				text   = ptr + 3;
				style |= STYLE_ITALIC;
			}
			else if( strncmp( ptr, "</b>", 3) == 0 ) {
				add_fragment( res, font, &x, &y, text, ptr - text, style);
				text   = ptr + 4;
				style &= ~STYLE_BOLD;
			}
			else if( strncmp( ptr, "</i>", 3) == 0 ) {
				add_fragment( res, font, &x, &y, text, ptr - text, style);
				text   = ptr + 4;
				style &= ~STYLE_ITALIC;
			}
		}
		
		ptr++;
	}
	
	add_fragment( res, font, &x, &y, text, ptr - text, style);
	res->width  = ( x > res->width ) ? x : res->width;
	res->height = y + font->ext.height - font->ext.ascent;
	
	return res;
};


/*
 * Draws the glyphs contained in 'text'.
 * 
 * Parameters: cr   - Cairo context.
 *             text - text_box_t containing the glyphs to show.
 */
void
draw_text( cairo_t *cr, text_box_t *text)
{
	int i;
	
	
	for( i = 0; i < text->nfrags; i++) {
		cairo_set_scaled_font( cr, text->frag[i].style);
		cairo_show_glyphs( cr, text->frag[i].glyphs,
						   text->frag[i].nglyphs);
		text->frag[i].nglyphs = 0;
		cairo_glyph_free( text->frag[i].glyphs);
	}
	
	free( text->frag);
	free( text);
};
