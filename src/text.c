

#include <cairo/cairo.h>
#include <cairo/cairo-ft.h>
#include <fontconfig/fontconfig.h>
#include <math.h>
#include <stdint.h>

#define CONFIG_GRAPHICAL
#define TEXT_PRIVATE
#define THEME_PRIVATE
#include "text.h"
#include "theme.h"
#include "config.h"
#include "drawing.h"
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
	double               size, pixel_size;
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
	
	FcPatternGetDouble( fc_regular, FC_PIXEL_SIZE, 0, &pixel_size);
	FcPatternGetDouble( fc_regular, FC_SIZE, 0, &size);
	
	cairo_matrix_init_scale( &font_matrix, pixel_size, pixel_size);
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
	res->ul_width = round( res->ext.descent / 4);
	res->ul_pos   = round( res->ext.descent / 2) - res->ul_width / 2;
	
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
#define STYLE_MASK         STYLE_BOLD_ITALIC
#define STYLE_UNDERLINED   (1 << 3)
static void
add_fragment( text_line *line, thor_font_t *font, double *x, double *y,
              char *string, int len, int style)
{
	if( len > 0 ) {
		int                  index = line->nfrags++;
		text_fragment        *frag;
		cairo_text_extents_t ext;
		
		
		_realloc( line->frag, text_fragment, line->nfrags);
		frag = &line->frag[index];
		memset( frag, 0, sizeof(text_fragment));
		
		if( (style & STYLE_MASK) == STYLE_REGULAR ) 
			frag->style = font->regular;
		else if( (style & STYLE_MASK) == STYLE_BOLD )
			frag->style = font->bold;
		else if( (style & STYLE_MASK) == STYLE_ITALIC )
			frag->style = font->italic;
		else if( (style & STYLE_MASK) == STYLE_BOLD_ITALIC )
			frag->style = font->bold_italic;
		
		cairo_scaled_font_text_to_glyphs( frag->style, *x, *y, string, len,
										  &frag->glyphs, &frag->nglyphs,
										  NULL, NULL, NULL);
		cairo_scaled_font_glyph_extents( frag->style, frag->glyphs,
		                                 frag->nglyphs, &ext);
		*x += ext.x_advance;
		
		frag->underlined = style & ~STYLE_MASK;
		frag->to_x = *x;
		frag->to_y = *y;
	}
};


/*
 * Adds a text_line struct to a text_box and returns a pointer to 
 * the newly allocated struct.
 * 
 * Parameters: box - text_box_t to which the line should be added.
 * 
 * Returns: A Pointer to the new element.
 */
static text_line *
add_text_line( text_box_t *box)
{
	int index = box->nlines++;
	
	
	_realloc( box->line, text_line, box->nlines);
	memset( &box->line[index], 0, sizeof(text_line));
	
	return &box->line[index];
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
	char       *ptr   = text;
	int        escape = 0;
	int        style  = STYLE_REGULAR;
	double     x      = 0;
	double     y      = font->ext.ascent;
	text_box_t *res   = (text_box_t*)malloc( sizeof(text_box_t));
	text_line  *line;
	
	
	memset( res, 0, sizeof(text_box_t));
	res->font = font;
	line = add_text_line( res);
	
	while( *ptr != '\0' ) {
		/** escape sequences **/
		if( *ptr == '\\' ) {
			if( escape ) {
				add_fragment( line, font, &x, &y, text, ptr - text - 1, style);
				text = ptr;
				escape = 0;
			}
			else {
				escape = 1;
			}
		}
		
		/** formating elements **/
		else if( *ptr == '\n' && escape ) {
			add_fragment( line, font, &x, &y, text, ptr - text - 1, style);
			text = ptr + 1;
			escape = 0;
		}
		else if( *ptr == '\n' || (*ptr == 'n' && escape) ) {
			add_fragment( line, font, &x, &y, text, ptr - text - escape, style);
			text       = ptr + 1;
			
			res->width  = ( x > res->width ) ? x : res->width;
			line->width = x;
			
			line = add_text_line( res);
			
			x  = 0;
			y += font->ext.height;
			escape = 0;
		}
		/** XML markup elements **/
		else if( *ptr == '<' ) {
			if( escape ) {
				add_fragment( line, font, &x, &y, text, ptr - text - 1, style);
				text = ptr;
				escape = 0;
			}
			else if( strncmp( ptr, "<b>", 3) == 0 ) {
				add_fragment( line, font, &x, &y, text, ptr - text, style);
				text   = ptr + 3;
				style |= STYLE_BOLD;
			}
			else if( strncmp( ptr, "<i>", 3) == 0 ) {
				add_fragment( line, font, &x, &y, text, ptr - text, style);
				text   = ptr + 3;
				style |= STYLE_ITALIC;
			}
			else if( strncmp( ptr, "<u>", 3) == 0 ) {
				add_fragment( line, font, &x, &y, text, ptr - text, style);
				text   = ptr + 3;
				style |= STYLE_UNDERLINED;
			}
			else if( strncmp( ptr, "</b>", 4) == 0 ) {
				add_fragment( line, font, &x, &y, text, ptr - text, style);
				text   = ptr + 4;
				style &= ~STYLE_BOLD;
			}
			else if( strncmp( ptr, "</i>", 4) == 0 ) {
				add_fragment( line, font, &x, &y, text, ptr - text, style);
				text   = ptr + 4;
				style &= ~STYLE_ITALIC;
			}
			else if( strncmp( ptr, "</u>", 4) == 0 ) {
				add_fragment( line, font, &x, &y, text, ptr - text, style);
				text   = ptr + 4;
				style &= ~STYLE_UNDERLINED;
			}
			/** unimplemented markup to ignore **/
			else if( strncmp( ptr, "<a href=", 8) == 0 ) {
				add_fragment( line, font, &x, &y, text, ptr - text, style);
				text   = ptr + 8;
				while( *text++ != '>' );
			}
			else if( strncmp( ptr, "<img src=", 9) == 0 ) {
				add_fragment( line, font, &x, &y, text, ptr - text, style);
				text   = ptr + 9;
				while( *text++ != '>' );
			}
			else if( strncmp( ptr, "</a>", 4) == 0 ) {
				add_fragment( line, font, &x, &y, text, ptr - text, style);
				text   = ptr + 4;
			}
		}
		else if( escape )
			escape = 0;
		
		ptr++;
	}
	
	add_fragment( line, font, &x, &y, text, ptr - text, style);
	
	line->width = x;
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
draw_text( cairo_t *cr, text_box_t *text, text_t *text_theme)
{
	int l, f;
	cairo_matrix_t source_m, font_m;
	
	
	cairo_matrix_init_scale( &source_m, text->width, text_theme->font->ext.height);
	
	if( text_theme->align_lines == ALIGN_LEFT ) {
		cairo_translate( cr, text_theme->x, text_theme->y);
		cairo_get_matrix( cr, &font_m);
	}
	
	for( l = 0; l < text->nlines; l++ ) {
		text_line      *line = &text->line[l];
		
		
		/** aligning **/
		if( text_theme->align_lines == ALIGN_CENTER ) {
			cairo_identity_matrix( cr);
			cairo_translate( cr, text_theme->x + (text->width / 2) - (line->width / 2), text_theme->y);
			cairo_get_matrix( cr, &font_m);
		}
		else if( text_theme->align_lines == ALIGN_RIGHT ) {
			cairo_identity_matrix( cr);
			cairo_translate( cr, text_theme->x + text->width - line->width, text_theme->y);
			cairo_get_matrix( cr, &font_m);
		}
		
		for( f = 0; f < line->nfrags; f++ ) {
			int           i;
			text_fragment *frag = &line->frag[f];
			
			
			cairo_set_scaled_font( cr, frag->style);
			
			if( frag->underlined ) {
				cairo_move_to( cr, frag->glyphs[0].x, frag->glyphs[0].y + text->font->ul_pos);
				cairo_line_to( cr, frag->to_x, frag->to_y + text->font->ul_pos);
				cairo_set_line_width( cr, text->font->ul_width);
			}
			
			/** fallback **/
			if( text_theme->surface.nlayers == 0 ) {
				cairo_translate( cr, 0, l * text_theme->font->ext.height);
				cairo_transform( cr, &source_m);
				cairo_set_source_rgba( cr, cairo_rgba( fallback_surface.surf_color));
				cairo_set_operator( cr, fallback_surface.surf_op);
				cairo_set_matrix( cr, &font_m);
				cairo_show_glyphs( cr, frag->glyphs, frag->nglyphs);
				cairo_stroke( cr);
			}
			else {
				for( i = 0; i < text_theme->surface.nlayers; i++ ) {
					cairo_translate( cr, 0, l * text_theme->font->ext.height);
					cairo_transform( cr, &source_m);
					set_layer( cr, &text_theme->surface.layer[i]);
					cairo_set_matrix( cr, &font_m);
					cairo_show_glyphs( cr, frag->glyphs, frag->nglyphs);
					cairo_stroke_preserve( cr);
				}
				
				cairo_new_path( cr);
			}
			
			frag->nglyphs = 0;
			cairo_glyph_free( frag->glyphs);
		}
		
		free( line->frag);
	}
	
	free( text->line);
	
	free( text);
};
