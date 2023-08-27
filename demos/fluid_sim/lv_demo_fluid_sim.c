/**
 * @file lv_demo_scroll.c
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_demo_fluid_sim.h"

#if LV_USE_DEMO_FLUID_SIM

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    void* buf;
    uint32_t w;
    uint32_t h;
} buf_t;

typedef struct {
    double density;
    uint32_t num_x;
    uint32_t num_y;
    uint32_t num_cells;
    double h;
    buf_t u;
    buf_t v;
    buf_t new_u;
    buf_t new_v;
    buf_t p;
    buf_t s;
    buf_t m;
    buf_t new_m;
} fluid_sim_t;
/**********************
 *  STATIC PROTOTYPES
 **********************/
static void canvas_update_timer(lv_timer_t* timer);
static void fluid_sim_init(fluid_sim_t* fluid_sim, uint32_t num_x, uint32_t num_y, double density);
/**********************
 *  STATIC VARIABLES
 **********************/
static fluid_sim_t g_fluid_sim;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void lv_demo_fluid_sim(void)
{
    lv_color32_t* buf = (lv_color32_t*)lv_malloc(320 * 240 * sizeof(lv_color32_t));
    lv_obj_t* canvas = lv_canvas_create(lv_scr_act());

    lv_canvas_set_buffer(canvas, buf, 320, 240, LV_COLOR_FORMAT_NATIVE_WITH_ALPHA);

    fluid_sim_init(&g_fluid_sim, 322, 242, 1000);

    lv_timer_create(canvas_update_timer, 10, canvas);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static buf_t* buf_create(uint32_t w, uint32_t h, uint32_t type_size)
{
    buf_t* buf = (buf_t*)lv_malloc(sizeof(buf_t));
    buf->buf = lv_malloc(w * h * type_size);
    buf->w = w;
    buf->h = h;
    return buf;
}
static void buf_destroy(buf_t* buf)
{
    lv_free(buf->buf);
    lv_free(buf);
}
static void buf_fill_double(buf_t* buf, double val)
{
    for (uint32_t i = 0; i < buf->w * buf->h; ++i) {
        ((double*)buf->buf)[i] = val;
    }
}
static void buf_set_double(buf_t* buf, uint32_t x, uint32_t y, double val)
{
    if (x >= buf->w || y >= buf->h) {
        return;
    }
    ((double*)buf->buf)[y * buf->w + x] = val;
}
static double buf_get_double(buf_t* buf, uint32_t x, uint32_t y)
{
    if (x >= buf->w || y >= buf->h) {
        return 0;
    }
    return ((double*)buf->buf)[y * buf->w + x];
}

static void fluid_sim_init(fluid_sim_t* fluid_sim, uint32_t num_x, uint32_t num_y, double density)
{
    fluid_sim->density = density;
    fluid_sim->num_x = num_x;
    fluid_sim->num_y = num_y;
    fluid_sim->num_cells = num_x * num_y;
    fluid_sim->h = 1.0 / num_x;
    fluid_sim->u = *buf_create(num_x, num_y, sizeof(double));
    fluid_sim->v = *buf_create(num_x, num_y, sizeof(double));
    fluid_sim->new_u = *buf_create(num_x, num_y, sizeof(double));
    fluid_sim->new_v = *buf_create(num_x, num_y, sizeof(double));
    fluid_sim->p = *buf_create(num_x, num_y, sizeof(double));
    fluid_sim->s = *buf_create(num_x, num_y, sizeof(double));
    fluid_sim->m = *buf_create(num_x, num_y, sizeof(double));
    fluid_sim->new_m = *buf_create(num_x, num_y, sizeof(double));

    buf_fill_double(&fluid_sim->s, 1);
    for (int i = 0; i < num_y; ++i) {
        buf_set_double(&fluid_sim->s, i, 0, 1.0);
    }
    buf_fill_double(&fluid_sim->m, 1);
}

static void fluid_sim_destroy(fluid_sim_t* fluid_sim)
{
    buf_destroy(&fluid_sim->u);
    buf_destroy(&fluid_sim->v);
    buf_destroy(&fluid_sim->new_u);
    buf_destroy(&fluid_sim->new_v);
    buf_destroy(&fluid_sim->p);
    buf_destroy(&fluid_sim->s);
    buf_destroy(&fluid_sim->m);
    buf_destroy(&fluid_sim->new_m);
}

static void integrate(fluid_sim_t* fluid_sim, double dt, double gravity)
{
    uint32_t n = fluid_sim->num_y;
    for (uint32_t i = 1; i < fluid_sim->num_x; i++) {
        for (uint32_t j = 1; j < fluid_sim->num_y - 1; j++) {
            if (buf_get_double(&fluid_sim->s, i, j) != 0.0 && buf_get_double(&fluid_sim->s, i, j - 1) != 0.0) {
                buf_set_double(&fluid_sim->v, i, j, buf_get_double(&fluid_sim->v, i, j) + gravity * dt);
            }
        }
    }
}

static void solve_incompressibility(fluid_sim_t* fluid_sim, uint32_t num_iters, double dt)
{
    uint32_t n = fluid_sim->num_y;
    double cp = fluid_sim->density * fluid_sim->h / dt;

    for (uint32_t iter = 0; iter < num_iters; iter++) {
        for (uint32_t i = 1; i < fluid_sim->num_x - 1; i++) {
            for (uint32_t j = 1; j < fluid_sim->num_y - 1; j++) {
                if (buf_get_double(&fluid_sim->s, i, j) == 0.0) {
                    continue;
                }
                double s = buf_get_double(&fluid_sim->s, i, j);
                double sx0 = buf_get_double(&fluid_sim->s, i - 1, j);
                double sx1 = buf_get_double(&fluid_sim->s, i + 1, j);
                double sy0 = buf_get_double(&fluid_sim->s, i, j - 1);
                double sy1 = buf_get_double(&fluid_sim->s, i, j + 1);
                s = sx0 + sx1 + sy0 + sy1;
                if (s == 0.0) {
                    continue;
                }
                double div = buf_get_double(&fluid_sim->u, i + 1, j) - buf_get_double(&fluid_sim->u, i, j) + buf_get_double(&fluid_sim->v, i, j + 1) - buf_get_double(&fluid_sim->v, i, j);
                double p = -div / s;
                p *= 1.0;
                buf_set_double(&fluid_sim->p, i, j, buf_get_double(&fluid_sim->p, i, j) + cp * p);
                buf_set_double(&fluid_sim->u, i, j, buf_get_double(&fluid_sim->u, i, j) - sx0 * p);
                buf_set_double(&fluid_sim->u, i + 1, j, buf_get_double(&fluid_sim->u, i + 1, j) + sx1 * p);
                buf_set_double(&fluid_sim->v, i, j, buf_get_double(&fluid_sim->v, i, j) - sy0 * p);
                buf_set_double(&fluid_sim->v, i, j + 1, buf_get_double(&fluid_sim->v, i, j + 1) + sy1 * p);
            }
        }
    }
}

static void extrapolate(fluid_sim_t* fluid_sim)
{
    uint32_t n = fluid_sim->num_y;
    for (uint32_t i = 0; i < fluid_sim->num_x; i++) {
        buf_set_double(&fluid_sim->u, i, 0, buf_get_double(&fluid_sim->u, i, 1));
        buf_set_double(&fluid_sim->u, i, fluid_sim->num_y - 1, buf_get_double(&fluid_sim->u, i, fluid_sim->num_y - 2));
    }
    for (uint32_t j = 0; j < fluid_sim->num_y; j++) {
        buf_set_double(&fluid_sim->v, 0, j, buf_get_double(&fluid_sim->v, 1, j));
        buf_set_double(&fluid_sim->v, fluid_sim->num_x - 1, j, buf_get_double(&fluid_sim->v, fluid_sim->num_x - 2, j));
    }
}

static double sample_field(fluid_sim_t* fluid_sim, double x, double y, buf_t* field)
{
    uint32_t n = fluid_sim->num_y;
    double h = fluid_sim->h;
    double h1 = 1.0 / h;
    double h2 = 0.5 * h;

    x = LV_MAX(LV_MIN(x, fluid_sim->num_x * h), h);
    y = LV_MAX(LV_MIN(y, fluid_sim->num_y * h), h);

    double dx = 0.0;
    double dy = 0.0;

    double* f;

    f = (double*)field->buf;
    dy = h2;

    uint32_t x0 = LV_MIN((uint32_t)((x - dx) * h1), fluid_sim->num_x - 1);
    double tx = ((x - dx) - x0 * h) * h1;
    uint32_t x1 = LV_MIN(x0 + 1, fluid_sim->num_x - 1);

    uint32_t y0 = LV_MIN((uint32_t)((y - dy) * h1), fluid_sim->num_y - 1);
    double ty = ((y - dy) - y0 * h) * h1;
    uint32_t y1 = LV_MIN(y0 + 1, fluid_sim->num_y - 1);

    double sx = 1.0 - tx;
    double sy = 1.0 - ty;

    double val = sx * sy * f[x0 * n + y0] + tx * sy * f[x1 * n + y0] + tx * ty * f[x1 * n + y1] + sx * ty * f[x0 * n + y1];

    return val;
}

static double avg_u(fluid_sim_t* fluid_sim, uint32_t i, uint32_t j)
{
    uint32_t n = fluid_sim->num_y;
    double u = (buf_get_double(&fluid_sim->u, i, j - 1) + buf_get_double(&fluid_sim->u, i, j) + buf_get_double(&fluid_sim->u, (i + 1), j - 1) + buf_get_double(&fluid_sim->u, (i + 1), j)) * 0.25;
    return u;
}

static double avg_v(fluid_sim_t* fluid_sim, uint32_t i, uint32_t j)
{
    uint32_t n = fluid_sim->num_y;
    double v = (buf_get_double(&fluid_sim->v, (i - 1), j) + buf_get_double(&fluid_sim->v, i, j) + buf_get_double(&fluid_sim->v, (i - 1), j + 1) + buf_get_double(&fluid_sim->v, i, j + 1)) * 0.25;
    return v;
}

static void advect(fluid_sim_t* fluid_sim, double dt)
{
    uint32_t n = fluid_sim->num_y;
    double h = fluid_sim->h;
    double h2 = 0.5 * h;

    for (uint32_t i = 1; i < fluid_sim->num_x; i++) {
        for (uint32_t j = 1; j < fluid_sim->num_y; j++) {
            if (buf_get_double(&fluid_sim->s, i, j) != 0.0 && buf_get_double(&fluid_sim->s, i - 1, j) != 0.0 && j < fluid_sim->num_y - 1) {
                double x = i * h;
                double y = j * h + h2;
                double u = buf_get_double(&fluid_sim->u, i, j);
                double v = avg_v(fluid_sim, i, j);
                x = x - dt * u;
                y = y - dt * v;
                u = sample_field(fluid_sim, x, y, &fluid_sim->u);
                buf_set_double(&fluid_sim->new_u, i, j, u);
            }
            if (buf_get_double(&fluid_sim->s, i, j) != 0.0 && buf_get_double(&fluid_sim->s, i, j - 1) != 0.0 && i < fluid_sim->num_x - 1) {
                double x = i * h + h2;
                double y = j * h;
                double u = avg_u(fluid_sim, i, j);
                double v = buf_get_double(&fluid_sim->v, i, j);
                x = x - dt * u;
                y = y - dt * v;
                v = sample_field(fluid_sim, x, y, &fluid_sim->v);
                buf_set_double(&fluid_sim->new_v, i, j, v);
            }
        }
    }
    buf_t tmp = fluid_sim->u;
    fluid_sim->u = fluid_sim->new_u;
    fluid_sim->new_u = tmp;
    tmp = fluid_sim->v;
    fluid_sim->v = fluid_sim->new_v;
    fluid_sim->new_v = tmp;
}

static void advect_smoke(fluid_sim_t* fluid_sim, double dt)
{
    uint32_t n = fluid_sim->num_y;
    double h = fluid_sim->h;
    double h2 = 0.5 * h;

    for (uint32_t i = 1; i < fluid_sim->num_x - 1; i++) {
        for (uint32_t j = 1; j < fluid_sim->num_y - 1; j++) {
            if (buf_get_double(&fluid_sim->s, i, j) != 0.0) {
                double u = (buf_get_double(&fluid_sim->u, i, j) + buf_get_double(&fluid_sim->u, i + 1, j)) * 0.5;
                double v = (buf_get_double(&fluid_sim->v, i, j) + buf_get_double(&fluid_sim->v, i, j + 1)) * 0.5;
                double x = i * h + h2 - dt * u;
                double y = j * h + h2 - dt * v;

                buf_set_double(&fluid_sim->new_m, i, j, sample_field(fluid_sim, x, y, &fluid_sim->s));
            }
        }
    }
    buf_t tmp = fluid_sim->m;
    fluid_sim->m = fluid_sim->new_m;
    fluid_sim->new_m = tmp;
}

static void simulate(fluid_sim_t* fluid_sim, double dt, double gravity, uint32_t num_iters)
{
    integrate(fluid_sim, dt, gravity);
    buf_fill_double(&fluid_sim->p, 0.0);
    solve_incompressibility(fluid_sim, num_iters, dt);
    extrapolate(fluid_sim);
    advect(fluid_sim, dt);
    advect_smoke(fluid_sim, dt);
}

static void canvas_update_timer(lv_timer_t* timer)
{
    lv_obj_t* canvas = lv_timer_get_user_data(timer);

    uint32_t w = lv_obj_get_width(canvas);
    uint32_t h = lv_obj_get_height(canvas);

    double dt = 0.1;
    double gravity = -9.8;
    uint32_t num_iters = 10;

    simulate(&g_fluid_sim, dt, gravity, num_iters);

    for (uint32_t i = 0; i < w; ++i) {
        for (uint32_t j = 0; j < h; ++j) {
            double m = buf_get_double(&g_fluid_sim.p, i, j);
            lv_canvas_set_px(canvas, i, j, lv_color_make(128, 0, 0), 255-m * 255);
        }
    }

    lv_obj_invalidate(canvas);
}

#endif
