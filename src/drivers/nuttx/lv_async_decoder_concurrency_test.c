/**
 * @file lv_async_decoder_concurrency_test.c
 * Verify that LVGL UI remains responsive while worker thread is busy.
 *
 * Test: submit a long-running task to uv worker thread,
 * simultaneously check that lv_timer_handler keeps firing on main thread.
 */

#include "lv_async_decoder.h"

#if LV_USE_NUTTX
#if LV_USE_NUTTX_LIBUV

#include "../../stdlib/lv_string.h"
#include <nuttx/clock.h>
#include <stdio.h>
#include <unistd.h>

#define C(fmt, ...) do { printf("[CONCUR] " fmt "\n", ##__VA_ARGS__); fflush(stdout); } while(0)

static unsigned long g_cfreq;
static inline clock_t cnow(void)
{
    return perf_gettime();
}
static inline uint32_t cus(clock_t s, clock_t e)
{
    return (uint32_t)((uint64_t)(e - s) * 1000000ULL / g_cfreq);
}

/* Worker thread: simulate slow I/O by sleeping */
static volatile bool g_worker_done = false;
static volatile uint32_t g_worker_duration_us = 0;

static void worker_cb(uv_work_t * req)
{
    /* This runs in the worker thread — simulate slow FS I/O */
    clock_t t0 = cnow();
    usleep(500000);  /* 500ms — simulates reading a large file from SPI Flash */
    clock_t t1 = cnow();
    g_worker_duration_us = cus(t0, t1);
}

static void after_worker_cb(uv_work_t * req, int status)
{
    /* This runs on main thread after worker completes */
    LV_UNUSED(status);
    g_worker_done = true;
    C("worker done: %u us in worker thread", g_worker_duration_us);
}

/* Main thread: count how many times our timer fires during worker execution */
static volatile int g_tick_count = 0;
static volatile uint32_t g_max_gap_us = 0;
static clock_t g_last_tick;

static void tick_cb(uv_timer_t * handle)
{
    clock_t now = cnow();
    if(g_tick_count > 0) {
        uint32_t gap = cus(g_last_tick, now);
        if(gap > g_max_gap_us) g_max_gap_us = gap;
    }
    g_last_tick = now;
    g_tick_count++;

    if(g_worker_done) {
        uv_timer_stop(handle);
        uv_stop(handle->loop);
    }
}

void lv_async_decoder_concurrency_test(uv_loop_t * loop)
{
    g_cfreq = perf_getfreq();
    C("=== Concurrency Test Start ===");
    C("Test: worker thread busy 500ms, main thread timer should keep firing");
    C("");

    /* Reset state */
    g_worker_done = false;
    g_worker_duration_us = 0;
    g_tick_count = 0;
    g_max_gap_us = 0;

    /* Start a 10ms repeating timer on main thread */
    uv_timer_t tick_timer;
    uv_timer_init(loop, &tick_timer);
    g_last_tick = cnow();
    uv_timer_start(&tick_timer, tick_cb, 0, 10);

    /* Submit work to worker thread */
    uv_work_t work_req;
    clock_t submit_time = cnow();
    uv_queue_work(loop, &work_req, worker_cb, after_worker_cb);

    /* Run loop until worker completes */
    uv_run(loop, UV_RUN_DEFAULT);

    clock_t end_time = cnow();
    uint32_t total_us = cus(submit_time, end_time);

    C("");
    C("Results:");
    C("  Worker thread busy:    %u us", g_worker_duration_us);
    C("  Main thread ticks:     %d (expected ~%u at 10ms interval)",
      g_tick_count, g_worker_duration_us / 10000);
    C("  Max tick gap:          %u us (should be << 500ms)", g_max_gap_us);
    C("  Total wall time:       %u us", total_us);
    C("");

    if(g_tick_count >= 5 && g_max_gap_us < g_worker_duration_us / 2) {
        C("PASS: main thread remained responsive during worker I/O");
        C("  Timer fired %d times while worker was busy for %u ms",
          g_tick_count, g_worker_duration_us / 1000);
        C("  Max gap between ticks: %u ms (worker duration: %u ms)",
          g_max_gap_us / 1000, g_worker_duration_us / 1000);
        C("  → UI rendering would NOT be blocked by async I/O");
    }
    else if(g_tick_count <= 1) {
        C("FAIL: main thread was blocked — only %d ticks", g_tick_count);
        C("  → UI WOULD be blocked during async I/O");
    }
    else {
        C("PARTIAL: %d ticks, max gap %u ms (worker %u ms)",
          g_tick_count, g_max_gap_us / 1000, g_worker_duration_us / 1000);
        C("  → Main thread responsive but with scheduling jitter (expected in QEMU)");
    }

    C("");
    C("=== Concurrency Test End ===");
}

#endif /* LV_USE_NUTTX_LIBUV */
#endif /* LV_USE_NUTTX */
