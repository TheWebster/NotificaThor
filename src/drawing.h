/* ************************************************************* *\
 * drawing.h                                                     *
 *                                                               *
 * Project:     NotificaThor                                     *
 * Author:      Christian Weber (ChristianWeber802@gmx.net       *
 *                                                               *
 * Description: Declaration of the draw_surface() function.      *
\* ************************************************************* */

struct fbs_t
{
	uint32_t         surf_color;
	cairo_operator_t surf_op;
};

extern struct fbs_t fallback_surface;
extern char         *image_string;


#define CONTROL_NONE             0
#define CONTROL_SAVE_MATRIX      (1 << 0)
#define CONTROL_USE_MATRIX       (1 << 1)
void draw_surface( cairo_t *cr, surface_t *surface, int control,
                   double x, double y, double width, double height);

void draw_border( cairo_t *cr, surface_t *surface, int outer,
                  double x, double y, double width, double height);
