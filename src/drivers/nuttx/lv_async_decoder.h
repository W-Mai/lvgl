/**
 * @file lv_async_decoder.h
 * Async image decoder using uv_fs worker threads.
 * File I/O runs in libuv worker pool (zero main-thread blocking),
 * then decodes via prefetch pool on main thread.
 */

#ifndef LV_ASYNC_DECODER_H
#define LV_ASYNC_DECODER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lvgl.h"
#include <uv.h>

#ifndef LV_ASYNC_DECODE_QUEUE_SIZE
#define LV_ASYNC_DECODE_QUEUE_SIZE 8
#endif

#ifndef LV_ASYNC_DECODE_CHUNK_SIZE
#define LV_ASYNC_DECODE_CHUNK_SIZE 65536
#endif

typedef enum {
    LV_ASYNC_DECODE_STATE_PENDING,   /**< queued, waiting for dispatch */
    LV_ASYNC_DECODE_STATE_READING,   /**< uv_fs I/O in progress (worker thread) */
    LV_ASYNC_DECODE_STATE_DECODING,  /**< decoding on main thread */
    LV_ASYNC_DECODE_STATE_DONE,      /**< completed */
} lv_async_decode_state_t;

typedef void (*lv_async_decode_cb_t)(const char * path, lv_result_t result, void * user_data);

/**
 * Path mapping callback: converts LVGL path to POSIX path for uv_fs.
 * @param drv        the FS driver
 * @param lvgl_path  LVGL path (e.g. "S:/img/bg.bin")
 * @param buf        output buffer for POSIX path
 * @param buf_size   size of output buffer
 * @return           pointer to buf, or NULL on failure
 */
typedef const char * (*lv_async_decode_path_map_cb_t)(lv_fs_drv_t * drv,
                                                      const char * lvgl_path,
                                                      char * buf, uint32_t buf_size);

typedef struct {
    char lvgl_path[LV_FS_MAX_PATH_LENGTH];   /**< LVGL path (cache key) */
    char posix_path[LV_FS_MAX_PATH_LENGTH];  /**< POSIX path (uv_fs) */
    lv_async_decode_cb_t cb;
    void * user_data;
    lv_async_decode_state_t state;
    bool cancelled;
    void * decoder;  /**< back-pointer to lv_async_decoder_t */

    /* uv_fs handles */
    uv_fs_t open_req;
    uv_fs_t stat_req;
    uv_fs_t read_req;
    uv_fs_t close_req;
    uv_file fd;
    uv_buf_t uv_buf;

    /* read buffer */
    uint8_t * buffer;
    uint32_t file_size;
    uint32_t offset;
    lv_result_t result;
} lv_async_decode_task_t;

typedef struct {
    uv_loop_t * loop;
    uv_timer_t timer;
    lv_async_decode_task_t queue[LV_ASYNC_DECODE_QUEUE_SIZE];
    uint32_t head;
    uint32_t count;
    lv_async_decode_path_map_cb_t path_map_cb;
} lv_async_decoder_t;

/**
 * Create an async decoder instance.
 * @param loop  the uv_loop to attach to
 * @return      pointer to the instance, or NULL on failure
 */
lv_async_decoder_t * lv_async_decoder_create(uv_loop_t * loop);

/**
 * Destroy the async decoder and free all resources.
 */
void lv_async_decoder_destroy(lv_async_decoder_t * decoder);

/**
 * Set the path mapping callback for LVGL path → POSIX path conversion.
 * If not set, a default mapping is used (strip drive letter: "S:/x" → "/x").
 */
void lv_async_decoder_set_path_map(lv_async_decoder_t * decoder,
                                   lv_async_decode_path_map_cb_t cb);

/**
 * Submit an image for async decoding.
 * @param decoder   the async decoder instance
 * @param path      LVGL file path (same as lv_image_set_src)
 * @param cb        callback when decode completes (may be NULL)
 * @param user_data passed to callback
 * @return          LV_RESULT_OK or LV_RESULT_INVALID if queue is full
 */
lv_result_t lv_async_decoder_submit(lv_async_decoder_t * decoder,
                                    const char * path,
                                    lv_async_decode_cb_t cb,
                                    void * user_data);

/**
 * Cancel a pending decode request.
 * PENDING tasks are removed immediately.
 * READING tasks are marked cancelled and cleaned up at next read_cb.
 */
lv_result_t lv_async_decoder_cancel(lv_async_decoder_t * decoder,
                                    const char * path);

#ifdef __cplusplus
}
#endif

#endif /* LV_ASYNC_DECODER_H */
