/**
 * @file lv_demo_scroll.c
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_demo_fluid_sim.h"
#include "math.h"

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
int init();
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

    //    fluid_sim_init(&g_fluid_sim, 322, 242, 1000);
    init();
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

#define CONSOLE_WIDTH 160
#define CONSOLE_HEIGHT 160

int xSandboxAreaScan = 0, ySandboxAreaScan = 0;

struct Particle {
    double xPos;
    double yPos;
    double density;
    int wallflag;
    double xForce;
    double yForce;
    double xVelocity;
    double yVelocity;
} particles[CONSOLE_WIDTH * CONSOLE_HEIGHT * 2];

double xParticleDistance, yParticleDistance;
double particlesInteraction;
double particlesDistance;

int x, y, screenBufferIndex, totalOfParticles;
int gravity = 1, pressure = 4, viscosity = 1;

char screenBuffer[CONSOLE_WIDTH * CONSOLE_HEIGHT + 1];

const char* buffer_map = "\n"
                         "\n"
                         "\n"
                         "            ###.......................###\n"
                         "            ###.......................###\n"
                         "            ###.......................###\n"
                         "            ###.......................###\n"
                         "            ###.......................###\n"
                         "            ###.......................###\n"
                         "            ###.......................###\n"
                         "            ###.......................###\n"
                         "            ###                       ###\n"
                         "            ###                       ###\n"
                         "            ###                       ###\n"
                         "            ###                       ###\n"
                         "            ###                       ###\n"
                         "            ###                       ###\n"
                         "            ###                       ###\n"
                         "            ###                       ###\n"
                         "            ###                       ###\n"
                         "            #############################\n"
                         "            #############################";

int init()
{

    // read the input file to initialise the particles.
    // # stands for "wall", i.e. unmovable particles (very high density)
    // any other non-space character represents normal particles.
//    int particlesCounter = 0;
//    int buf_i = 0;
//    while ((x = buffer_map[buf_i++]) != '\0') {
//
//        switch (x) {
//        case '\n':
//            // next row
//            // rewind the x to -1 cause it's gonna be incremented at the
//            // end of the while body
//            ySandboxAreaScan += 2;
//            xSandboxAreaScan = -1;
//            break;
//        case ' ':
//            break;
//        case '#':
//            // The character # represents “wall particle” (a particle with fixed position),
//            // and any other non-space characters represent free particles.
//            // A wall sets the flag on 2 particles side by side.
//            particles[particlesCounter].wallflag = particles[particlesCounter + 1].wallflag = 1;
//        default:
//
//            particles[particlesCounter].xPos = xSandboxAreaScan;
//            particles[particlesCounter].yPos = ySandboxAreaScan;
//
//            particles[particlesCounter + 1].xPos = xSandboxAreaScan;
//            particles[particlesCounter + 1].yPos = ySandboxAreaScan + 1;
//
//            totalOfParticles = particlesCounter += 2;
//        }
//        xSandboxAreaScan += 1;
//    }
    for (int i = 0; i < CONSOLE_WIDTH; ++i) {
        for (int j = 0; j < CONSOLE_HEIGHT; ++j) {
            if ( 10 < i && i < CONSOLE_WIDTH - 10 && 10 < j && j < 20) {
                particles[totalOfParticles].xPos = i;
                particles[totalOfParticles].yPos = j;
                particles[totalOfParticles].wallflag = 0;
                totalOfParticles++;
            } else if ( 0 <= i && i <= CONSOLE_HEIGHT && CONSOLE_HEIGHT - 10 <= j && j < CONSOLE_HEIGHT) {
                particles[totalOfParticles].xPos = i;
                particles[totalOfParticles].yPos = j;
                particles[totalOfParticles].wallflag = 1;
                totalOfParticles++;
            }


        }
    }

    return 0;
}

void simulate()
{

    int particlesCursor, particlesCursor2;

    // Iterate over every pair of particles to calculate the densities
    for (particlesCursor = 0; particlesCursor < totalOfParticles; particlesCursor++) {
        // density of "wall" particles is high, other particles will bounce off them.
        particles[particlesCursor].density = particles[particlesCursor].wallflag * 9;

        for (particlesCursor2 = 0; particlesCursor2 < totalOfParticles; particlesCursor2++) {

            xParticleDistance = particles[particlesCursor].xPos - particles[particlesCursor2].xPos;
            yParticleDistance = particles[particlesCursor].yPos - particles[particlesCursor2].yPos;
            particlesDistance = sqrt(pow(xParticleDistance, 2.0) + pow(yParticleDistance, 2.0));
            particlesInteraction = particlesDistance / 2.0 - 1.0;

            if (floor(1.0 - particlesInteraction) > 0) {
                particles[particlesCursor].density += particlesInteraction * particlesInteraction;
            }
        }
    }

    // Iterate over every pair of particles to calculate the forces
    for (particlesCursor = 0; particlesCursor < totalOfParticles; particlesCursor++) {
        particles[particlesCursor].yForce = gravity;
        particles[particlesCursor].xForce = 0;

        for (particlesCursor2 = 0; particlesCursor2 < totalOfParticles; particlesCursor2++) {

            xParticleDistance = particles[particlesCursor].xPos - particles[particlesCursor2].xPos;
            yParticleDistance = particles[particlesCursor].yPos - particles[particlesCursor2].yPos;
            particlesDistance = sqrt(pow(xParticleDistance, 2.0) + pow(yParticleDistance, 2.0));
            particlesInteraction = particlesDistance / 2.0 - 1.0;

            // force is updated only if particles are close enough
            if (floor(1.0 - particlesInteraction) > 0) {
                particles[particlesCursor].xForce += particlesInteraction * (xParticleDistance * (3 - particles[particlesCursor].density - particles[particlesCursor2].density) * pressure + particles[particlesCursor].xVelocity * viscosity - particles[particlesCursor2].xVelocity * viscosity) / particles[particlesCursor].density;
                particles[particlesCursor].yForce += particlesInteraction * (yParticleDistance * (3 - particles[particlesCursor].density - particles[particlesCursor2].density) * pressure + particles[particlesCursor].yVelocity * viscosity - particles[particlesCursor2].yVelocity * viscosity) / particles[particlesCursor].density;
            }
        }
    }

    // empty the buffer
    for (screenBufferIndex = 0; screenBufferIndex < CONSOLE_WIDTH * CONSOLE_HEIGHT; screenBufferIndex++) {
        screenBuffer[screenBufferIndex] = 0;
    }

    for (particlesCursor = 0; particlesCursor < totalOfParticles; particlesCursor++) {
        if (!particles[particlesCursor].wallflag) {
            if (sqrt(pow(particles[particlesCursor].xForce, 2.0) + pow(particles[particlesCursor].yForce, 2.0)) < 4.2) {
                particles[particlesCursor].xVelocity += particles[particlesCursor].xForce / 10;
                particles[particlesCursor].yVelocity += particles[particlesCursor].yForce / 10;
            } else {
                particles[particlesCursor].xVelocity += particles[particlesCursor].xForce / 11;
                particles[particlesCursor].yVelocity += particles[particlesCursor].yForce / 11;
            }

            particles[particlesCursor].xPos += particles[particlesCursor].xVelocity;
            particles[particlesCursor].yPos += particles[particlesCursor].yVelocity;
        }
        x = particles[particlesCursor].xPos;
        y = particles[particlesCursor].yPos / 2;
        screenBufferIndex = x + CONSOLE_WIDTH * y;

        if (y >= 0 && y < CONSOLE_HEIGHT - 1 && x >= 0 && x < CONSOLE_WIDTH - 1) {
            screenBuffer[screenBufferIndex] |= 8; // set 4th bit to 1
            screenBuffer[screenBufferIndex + 1] |= 4; // set 3rd bit to 1
            // now the cell in row below
            screenBuffer[screenBufferIndex + CONSOLE_WIDTH] |= 2; // set 2nd bit to 1
            screenBuffer[screenBufferIndex + CONSOLE_WIDTH + 1] |= 1; // set 1st bit to 1
        }
    }

    for (screenBufferIndex = 0; screenBufferIndex < CONSOLE_WIDTH * CONSOLE_HEIGHT; screenBufferIndex++) {
        if (screenBufferIndex % CONSOLE_WIDTH == CONSOLE_WIDTH - 1)
            screenBuffer[screenBufferIndex] = '\n';
        else {

            //            screenBuffer[screenBufferIndex] = " '`-.|//,\\|\\_\\/#"[screenBuffer[screenBufferIndex]];
        }
    }
}

static void canvas_update_timer(lv_timer_t* timer)
{
    lv_obj_t* canvas = lv_timer_get_user_data(timer);

    uint32_t w = CONSOLE_WIDTH;
    uint32_t h = CONSOLE_HEIGHT;

    simulate();

    for (uint32_t i = 0; i < h; ++i) {
        for (uint32_t j = 0; j < w; ++j) {
            char m = screenBuffer[i * w + j];
            if (m == 15) {
                lv_canvas_set_px(canvas, j, i, lv_color_make(0, 0, 0), LV_OPA_100);
            } else {
                lv_canvas_set_px(canvas, j, i, lv_color_make(0, 0, 0), m);
            }
        }
    }

    lv_obj_invalidate(canvas);
}

#endif
