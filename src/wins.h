/* ************************************************************* *\
 * wins.h                                                        *
 *                                                               *
 * Project:     NotificaThor                                     *
 * Author:      Christian Weber (ChristianWeber802@gmx.net)      *
 *                                                               *
 * Description: Function declarations for wins.c                 *
\* ************************************************************* */


int  prepare_x();
int  show_win( thor_message *msg);
void close_win( int window);
void cleanup_x();
void query_extensions();
void parse_default_theme();
