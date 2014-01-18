/* ************************************************************* *\
 * images.h                                                      *
 *                                                               *
 * Project:     NotificaThor                                     *
 * Author:      Christian Weber (ChristianWeber802@gmx.net)      *
 *                                                               *
 * Description: Functions regarding image files.                 *
\* ************************************************************* */


#ifdef CAIRO_H
cairo_pattern_t *get_pattern_for_png( char *filename);
#else
extern char image_cache_path[];
#endif

int             load_image_cache();
int             save_image_cache();
