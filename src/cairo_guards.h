/* ************************************************************* *\
 * cairo_guards.h                                                *
 * Project:     NotificaThor                                     *
 * Author:      Christian Weber (ChristianWeber802@gmx.net)      *
 *                                                               *
 * Description: Compile guards checking for cairo features.      *
\* ************************************************************* */


// version >= 1.12 because of mesh patterns
#if CAIRO_VERSION < CAIRO_VERSION_ENCODE( 1, 12, 0)
	#error "Cairo 1.12.0 or later is needed!"
#endif /* CAIRO_VERSION < CAIRO_VERSION_ENCODE( 1, 12, 0) */

#ifndef CAIRO_HAS_FC_FONT
	#error "Cairo was compiled without fontconfig support!"
#endif /* Not CAIRO_HAS_FC_FONT */

#ifndef CAIRO_HAS_FT_FONT
	#error "Cairo was compiled without freetype2 support!"
#endif /* Not CAIRO_HAS_FT_FONT */

#ifndef CAIRO_HAS_PNG_FUNCTIONS
	#error "Cairo was compiled without png support!"
#endif /* Not CAIRO_HAS_PNG_FUNCTIONS */

#ifndef CAIRO_HAS_IMAGE_SURFACE
	#error "Cairo was compiled without image surface support!"
#endif /* Not CAIRO_HAS_IMAGE_SURFACE */

#ifndef CAIRO_HAS_XCB_SURFACE
	#error "Cairo was compiled without xcb support!"
#endif /* Not CAIRO_HAS_XCB_SURFACE */
