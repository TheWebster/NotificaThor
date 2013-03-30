
#define CONTROL_NONE             0
#define CONTROL_OUTER_BORDER     (1 << 0)
#define CONTROL_PRESERVE_MATRIX  (1 << 1)
#define CONTROL_USE_SAVED_MATRIX (1 << 2)
void draw_surface( cairo_t *cr, surface_t *surface, int control,
                   double x, double y, double width, double height);
