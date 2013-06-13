

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
	FcPattern *fc_regular = FcFontRenderPrepare( NULL, fc_default, fc_theme);
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
 * Allocates a new text_line struct and returns the new element.
 * 
 * Parameter: box - The text_box_t to which the text_line should be added.
 * 
 * Returns: A Pointer to the new element.
 */
static text_line *
alloc_line( text_box_t *box)
{
	int       index = box->nlines++;
	
	
	_realloc( box->line, text_line, box->nlines);
	memset( &box->line[index], 0, sizeof(text_line));
	
	return &box->line[index];
};


/*
 * Allocates a new text_word struct and returns the new element.
 * 
 * Parameter: box - The text_box_t to which the text_word should be added.
 * 
 * Returns: A Pointer to the new element.
 */
static text_word *
alloc_word( text_box_t *box)
{
	int       index = box->nwords++;
	
	
	_realloc( box->word, text_word, box->nwords);
	memset( &box->word[index], 0, sizeof(text_word));
	
	return &box->word[index];
};


/*
 * Allocates a new text_fragment struct and returns the new element.
 * 
 * Parameter: box - The text_box_t to which the text_fragment should be added.
 * 
 * Returns: A Pointer to the new element.
 */
static text_fragment *
alloc_frag( text_box_t *box)
{
	int           index = box->nfrags++;
	
	
	_realloc( box->frag, text_fragment, box->nfrags);
	memset( &box->frag[index], 0, sizeof(text_fragment));
	
	return &box->frag[index];
};


/*
 * Moves the glyphs contained in a text_fragment by an x and y value.
 * 
 * Parameters: frag - The text_fragment containing the glyphs to be moved.
 *             x, y - The amount by which they should be moved.
 * 
 */
static void
move_frag( text_fragment *frag, double x, double y)
{
	int g;
	
	
	for( g = 0; g < frag->nglyphs; g++ ) {
		frag->glyphs[g].x += x;
		frag->glyphs[g].y += y;
	}
};


/*
 * Calls move_frag() for each text_fragment contained by a text_word.
 * 
 * Parameters: word - The text_word containing the text_fragments to be modified-
 *             frag - The first text_fragment not contained by word.
 *             x, y - The amount by which the glyphs should be moved.
 */
static void
move_word( text_word *word, text_fragment *frag, double x, double y)
{
	int f;
	
	
	frag -= word->nfrags;
	
	for( f = 0; f <= word->nfrags; f++ )
		move_frag( &frag[f], x, y);
};


#define STYLE_REGULAR      0
#define STYLE_BOLD         (1 << 0)
#define STYLE_ITALIC       (1 << 1)
#define STYLE_BOLD_ITALIC  (STYLE_BOLD|STYLE_ITALIC)
#define STYLE( var)        (var & STYLE_BOLD_ITALIC)
#define STYLE_UNDERLINED   (1 << 2)
#define STYLE_NEWWORD      (1 << 3)
#define STYLE_NEWLINE      (1 << 4)
#define STYLE_END          (1 << 5)


/*
 * Adds a new text_fragment to a text_box_t and altering the current line, word and
 * x|y position accordingly by handling newlines, whitespaces and linesplitting.
 * 
 * Parameters: box    - The text_box_t to add the fragment to.
 *             line   - Handle to the current line, is modified when advancing to next line.
 *             word   - Handle to the current word, is modified when advancing to next word.
 *             style  - Style property flags to modify behaviour.
 *             x,y    - Pointers to current position, is modified accordingly.
 *             fwidth - Line width, that is forced upon the text. Ignored when 0.
 *             string - UTF8 encoded string to translate.
 *             len    - Length of string.
 */
static void
add_fragment( text_box_t *box, text_line **line, text_word **word, int style,
              double *x, double *y, double fwidth, char *string, int len)
{
	text_fragment        *frag = alloc_frag( box);
	cairo_text_extents_t ext;
	
	
	/** evaluate font-style **/
	if( style & STYLE_UNDERLINED )
		frag->underlined = 1;
	if( STYLE( style) == STYLE_REGULAR )
		frag->style = box->font->regular;
	else if( STYLE( style) == STYLE_BOLD )
		frag->style = box->font->bold;
	else if( STYLE( style) == STYLE_ITALIC )
		frag->style = box->font->italic;
	else if( STYLE( style) == STYLE_BOLD_ITALIC )
		frag->style = box->font->bold_italic;
	
	cairo_scaled_font_text_to_glyphs( frag->style, *x, *y, string, len,
	                                  &frag->glyphs, &frag->nglyphs,
	                                  NULL, NULL, NULL);
	cairo_scaled_font_glyph_extents( frag->style, frag->glyphs, frag->nglyphs, &ext);
	frag->free_glyph = frag->glyphs;
	
	
	if( frag->underlined )
		frag->to_x = ext.x_advance;
	
	if( fwidth > 0 ) {
		/** split line at word boundary **/
		if( (*x + ext.x_advance) > fwidth && (*line)->nwords ) {
			box->width     = ( *x > box->width ) ? *x : box->width;
			(*line)->width = *x;
			
			*line = alloc_line( box);
			move_word( *word, frag, -*x, box->font->ext.height);
			
			*y += box->font->ext.height;
			*x  = 0;
		}
		/** split line in middle of word if neccessary **/
		while( (*x + ext.x_advance) > fwidth ) {
			cairo_glyph_t       *hlp_glyphs = frag->glyphs;
			int                 hlp_nglyphs = frag->nglyphs;
			cairo_scaled_font_t *hlp_style  = frag->style;
			int                 hlp_ul      = frag->underlined;
			
			
			/** evaluate break **/
			while( (*x + ext.x_advance) > fwidth ) {
				frag->nglyphs--;
				cairo_scaled_font_glyph_extents( frag->style, frag->glyphs, frag->nglyphs, &ext);
			}
			*x += ext.x_advance;
			
			hlp_glyphs  += frag->nglyphs;
			hlp_nglyphs -= frag->nglyphs;
			
			box->width     = ( *x > box->width ) ? *x : box->width;
			(*line)->width = *x;
			
			(*word)->nfrags++;
			(*line)->nwords++;
			
			*line = alloc_line( box);
			*word = alloc_word( box);
			frag  = alloc_frag( box);
			
			frag->glyphs  = hlp_glyphs;
			frag->nglyphs = hlp_nglyphs;
			frag->style   = hlp_style;
			frag->underlined = hlp_ul;
			move_frag( frag, -*x, box->font->ext.height);
			if( frag->underlined )
				frag->to_x = ext.x_advance;
			
			cairo_scaled_font_glyph_extents( frag->style, frag->glyphs, frag->nglyphs, &ext);
			*x  = 0;
			*y += box->font->ext.height;
		}
	}
	
	*x += ext.x_advance;
	(*word)->nfrags++;
	
	if( style & STYLE_NEWLINE ) {
		(*line)->nwords++;
		(*line)->width = *x;
		*line = alloc_line( box);
		*word = alloc_word( box);
		box->width  = ( *x > box->width ) ? *x : box->width;
		*x = 0;
		*y += box->font->ext.height;
	}
	
	if( style & STYLE_NEWWORD ) {
		(*line)->nwords++;
		*word = alloc_word( box);
	}
	
	if( style & STYLE_END ) {
		(*line)->nwords++;
		(*line)->width = *x;
		box->width  = ( *x > box->width ) ? *x : box->width;
		box->height = *y + box->font->ext.height - box->font->ext.ascent;
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
prepare_text( char *text, thor_font_t *font, double fwidth)
{
	char        *ptr   = text;
	int         style  = STYLE_REGULAR;
	int         escape = 0;
	double      x      = 0;
	double      y      = font->ext.ascent;
	text_box_t  *res   = (text_box_t*)malloc( sizeof(text_box_t));
	text_line   *line;
	text_word   *word;
	
	
	fwidth = (fwidth > font->ext.max_x_advance) ? fwidth : 0;
	memset( res, 0, sizeof(text_box_t));
	res->font = font;
	line      = alloc_line( res);
	word      = alloc_word( res);
	
	while( *ptr ) {
		/** escape handling **/
		if( *ptr == '\\' ) {
			if( escape ) {
				add_fragment( res, &line, &word, style, &x, &y, fwidth,
				              text, ptr - text - 1);
				text = ptr;
				escape = 0;
			}
			else
				escape = 1;
		}
		/** newline handling **/
		else if( *ptr == '\n' && escape ) {
			add_fragment( res, &line, &word, style, &x, &y, fwidth,
				          text, ptr - text - 1);
			text = ptr;
			escape = 0;
		}
		else if( *ptr == '\n' || ( *ptr == 'n' && escape ) ) {
			add_fragment( res, &line, &word, style | STYLE_NEWLINE, &x, &y, fwidth,
				          text, ptr - text - escape);
			text = ptr + 1;
			escape = 0;
		}
		/** whitespace **/
		else if( *ptr == ' ' ) {
			add_fragment( res, &line, &word, style | STYLE_NEWWORD, &x, &y, fwidth,
			              text, ptr - text + 1);
			text = ptr + 1;
			escape = 0;
		}
		/** markup **/
		else if( *ptr == '<' ) {
			if( escape ) {
				add_fragment( res, &line, &word, style, &x, &y, fwidth,
				              text, ptr - text - 1);
				text = ptr;
				escape = 0;
			}
			else if( strncmp( ptr, "<b>", 3) == 0 ) {
				add_fragment( res, &line, &word, style, &x, &y, fwidth,
				              text, ptr - text);
				text = ptr + 3;
				style |= STYLE_BOLD;
			}
			else if( strncmp( ptr, "<i>", 3) == 0 ) {
				add_fragment( res, &line, &word, style, &x, &y, fwidth,
				              text, ptr - text);
				text = ptr + 3;
				style |= STYLE_ITALIC;
			}
			else if( strncmp( ptr, "<u>", 3) == 0 ) {
				add_fragment( res, &line, &word, style, &x, &y, fwidth,
				              text, ptr - text);
				text = ptr + 3;
				style |= STYLE_UNDERLINED;
			}
			else if( strncmp( ptr, "</b>", 4) == 0 ) {
				add_fragment( res, &line, &word, style, &x, &y, fwidth,
				              text, ptr - text);
				text = ptr + 4;
				style &= ~STYLE_BOLD;
			}
			else if( strncmp( ptr, "</i>", 4) == 0 ) {
				add_fragment( res, &line, &word, style, &x, &y, fwidth,
				              text, ptr - text);
				text = ptr + 4;
				style &= ~STYLE_ITALIC;
			}
			else if( strncmp( ptr, "</u>", 4) == 0 ) {
				add_fragment( res, &line, &word, style, &x, &y, fwidth,
				              text, ptr - text);
				text = ptr + 4;
				style &= ~STYLE_UNDERLINED;
			}
		}
		else if( escape )
			escape = 0;
		
		ptr++;
	}
	
	add_fragment( res, &line, &word, style|STYLE_END, &x, &y, fwidth,
			      text, ptr - text);
	
	/** set width to forced width **/
	if( fwidth > 0 )
		res->width = fwidth;
	
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
	int l = 0;
	int w = 0;
	int f = 0;
	cairo_matrix_t source_m, font_m;
	
	
	cairo_matrix_init_scale( &source_m, text->width, text_theme->font->ext.height);
	
	if( text_theme->align_lines == ALIGN_LEFT ) {
		cairo_translate( cr, text_theme->x, text_theme->y);
		cairo_get_matrix( cr, &font_m);
	}
	
	for( ; l < text->nlines; l++ ) {
		text_line *line = &text->line[l];
		
		
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
		
		for( ; line->nwords; line->nwords--, w++ ) {
			text_word *word = &text->word[w];
			
			
			for( ; word->nfrags; word->nfrags--, f++ ) {
				text_fragment *frag = &text->frag[f];
				int           i;
				
				
				if( !frag->nglyphs )
					continue;
				
				cairo_set_scaled_font( cr, frag->style);
				
				if( frag->underlined ) {
					cairo_move_to( cr, frag->glyphs[0].x, frag->glyphs[0].y + text->font->ul_pos);
					cairo_rel_line_to( cr, frag->to_x, 0);
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
			}
		}
	}
	
	/** free glyphs in fragments **/
	for( f = 0; f < text->nfrags; f++ )
		cairo_glyph_free( text->frag[f].free_glyph);
	
	free( text->frag);
	free( text->word);
	free( text->line);
	free( text);
};
