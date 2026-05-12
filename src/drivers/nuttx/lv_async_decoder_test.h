/**
 * @file lv_async_decoder_test.h
 * Built-in test for async decoder. Enabled by LV_ASYNC_DECODER_TEST.
 * Call lv_async_decoder_test_start(loop) after uv_loop is initialized.
 * Test results are printed via LV_LOG.
 */

#ifndef LV_ASYNC_DECODER_TEST_H
#define LV_ASYNC_DECODER_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <uv.h>

/**
 * Start async decoder tests. Registers a uv_timer that runs tests
 * after a short delay (to let the system settle).
 * Results are logged via LV_LOG_USER/LV_LOG_ERROR.
 */
void lv_async_decoder_test_start(uv_loop_t * loop);

#ifdef __cplusplus
}
#endif

#endif
