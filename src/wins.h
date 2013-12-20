/* ************************************************************* *\
 * wins.h                                                        *
 *                                                               *
 * Project:     NotificaThor                                     *
 * Author:      Christian Weber (ChristianWeber802@gmx.net)      *
 *                                                               *
 * Description: Function declarations for wins.c                 *
\* ************************************************************* */


int  prepare_x();
int  show_osd( thor_message *msg);
int  kill_osd();
void cleanup_x();
void query_extensions();
void parse_default_theme();
int  alloc_named_color( char *string, uint32_t *color);
