/* ********************************************** *\
 * drawing.h                                      *
 *                                                *
 * Project:     NotificaThor                      *
 * Author:      Christian Weber                   *
 *                                                *
 * Description: Declaration of the draw_surface() *
 *              function.                         *
\* ********************************************** */

struct fbs_t
{
	uint32_t         surf_color;
	cairo_operator_t surf_op;
};

extern struct fbs_t fallback_surface;


#define CONTROL_NONE             0
#define CONTROL_OUTER_BORDER     (1 << 0)
#define CONTROL_PRESERVE_MATRIX  (1 << 1)
#define CONTROL_PRESERVE_CLIP    (1 << 2)
#define CONTROL_USE_MATRIX       (1 << 3)
#define CONTROL_USE_CLIP         (1 << 4)
#define CONTROL_PRESERVE         (CONTROL_PRESERVE_CLIP|CONTROL_PRESERVE_MATRIX)
#define CONTROL_USE              (CONTROL_USE_CLIP|CONTROL_USE_MATRIX)
void draw_surface( cairo_t *cr, surface_t *surface, int control,
                   double x, double y, double width, double height);
