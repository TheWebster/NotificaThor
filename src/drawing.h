/* ********************************************** *\
 * drawing.h                                      *
 *                                                *
 * Project:     NotificaThor                      *
 * Author:      Christian Weber                   *
 *                                                *
 * Description: Declaration of the draw_surface() *
 *              function.                         *
\* ********************************************** */


#define CONTROL_NONE             0
#define CONTROL_OUTER_BORDER     (1 << 0)
#define CONTROL_PRESERVE_MATRIX  (1 << 1)
#define CONTROL_PRESERVE_CLIP    (1 << 2)
#define CONTROL_USE_SAVED        (1 << 3)
#define CONTROL_PRESERVE         (CONTROL_PRESERVE_CLIP|CONTROL_PRESERVE_MATRIX)
void draw_surface( cairo_t *cr, surface_t *surface, int control,
                   double x, double y, double width, double height);
