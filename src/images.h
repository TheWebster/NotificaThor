/* ************************************************************* *\
 * images.h                                                      *
 *                                                               *
 * Project:     NotificaThor                                     *
 * Author:      Christian Weber (ChristianWeber802@gmx.net)      *
 *                                                               *
 * Description: Functions regarding image files.                 *
\* ************************************************************* */

cairo_pattern_t *get_pattern_for_png( char *filename);
void            free_image_cache();
