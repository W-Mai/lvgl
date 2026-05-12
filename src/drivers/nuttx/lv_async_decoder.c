/**
 * @file lv_async_decoder.c
 * Async image decoder — Scheme B: uv_fs worker threads.
 * File I/O in libuv worker pool, decode on main thread via prefetch pool.
 */

#include "lv_async_decoder.h"

#if LV_USE_NUTTX
#if LV_USE_NUTTX_LIBUV

#include "../../misc/lv_fs_prefetch.h"
#include "../../draw/lv_image_decoder.h"
#include "../../draw/lv_image_decoder_private.h"
#include "../../stdlib/lv_string.h"
#include "../../stdlib/lv_mem.h"

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void timer_cb(uv_timer_t * handle);
static void dispatch_task(lv_async_decoder_t * decoder, lv_async_decode_task_t * task);
static void task_complete(lv_async_decode_task_t * task);
static void advance_queue(lv_async_decoder_t * decoder);

static void on_open(uv_fs_t * req);
static void on_stat(uv_fs_t * req);
static void on_read(uv_fs_t * req);
static void on_close(uv_fs_t * req);
static void start_next_read(lv_async_decode_task_t * task);

static const char * default_path_map(lv_fs_drv_t * drv, const char * lvgl_path,
                                     char * buf, uint32_t buf_size);
static bool is_in_image_cache(const char * path);
static bool path_in_queue(lv_async_decoder_t * decoder, const char * path);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_async_decoder_t * lv_async_decoder_create(uv_loop_t * loop)
{
    lv_async_decoder_t * decoder = lv_malloc_zeroed(sizeof(lv_async_decoder_t));
    if(decoder == NULL) return NULL;

    decoder->loop = loop;
    uv_timer_init(loop, &decoder->timer);
    decoder->timer.data = decoder;
    decoder->path_map_cb = default_path_map;
    return decoder;
}

void lv_async_decoder_destroy(lv_async_decoder_t * decoder)
{
    if(decoder == NULL) return;
    uv_timer_stop(&decoder->timer);

    for(uint32_t i = 0; i < decoder->count; i++) {
        uint32_t idx = (decoder->head + i) % LV_ASYNC_DECODE_QUEUE_SIZE;
        decoder->queue[idx].cancelled = true;
    }
    /* READING tasks will self-cleanup via on_close.
     * PENDING tasks can be freed now. */
    decoder->count = 0;
    uv_close((uv_handle_t *)&decoder->timer, NULL);
    lv_free(decoder);
}

void lv_async_decoder_set_path_map(lv_async_decoder_t * decoder,
                                   lv_async_decode_path_map_cb_t cb)
{
    decoder->path_map_cb = cb ? cb : default_path_map;
}

lv_result_t lv_async_decoder_submit(lv_async_decoder_t * decoder,
                                    const char * path,
                                    lv_async_decode_cb_t cb,
                                    void * user_data)
{
    if(is_in_image_cache(path)) {
        if(cb) cb(path, LV_RESULT_OK, user_data);
        return LV_RESULT_OK;
    }

    if(path_in_queue(decoder, path)) return LV_RESULT_OK;
    if(decoder->count >= LV_ASYNC_DECODE_QUEUE_SIZE) return LV_RESULT_INVALID;

    uint32_t tail = (decoder->head + decoder->count) % LV_ASYNC_DECODE_QUEUE_SIZE;
    lv_async_decode_task_t * task = &decoder->queue[tail];
    lv_memzero(task, sizeof(*task));

    lv_strlcpy(task->lvgl_path, path, sizeof(task->lvgl_path));

    char drive = path[0];
    lv_fs_drv_t * drv = lv_fs_get_drv(drive);
    if(decoder->path_map_cb(drv, path, task->posix_path, sizeof(task->posix_path)) == NULL) {
        return LV_RESULT_INVALID;
    }

    task->cb = cb;
    task->user_data = user_data;
    task->state = LV_ASYNC_DECODE_STATE_PENDING;
    task->decoder = decoder;
    task->fd = -1;
    decoder->count++;

    uv_timer_start(&decoder->timer, timer_cb, 0, 0);
    return LV_RESULT_OK;
}

lv_result_t lv_async_decoder_cancel(lv_async_decoder_t * decoder,
                                    const char * path)
{
    for(uint32_t i = 0; i < decoder->count; i++) {
        uint32_t idx = (decoder->head + i) % LV_ASYNC_DECODE_QUEUE_SIZE;
        lv_async_decode_task_t * task = &decoder->queue[idx];
        if(lv_strcmp(task->lvgl_path, path) != 0) continue;

        if(task->state == LV_ASYNC_DECODE_STATE_PENDING) {
            /* Remove immediately */
            for(uint32_t j = i; j < decoder->count - 1; j++) {
                uint32_t src = (decoder->head + j + 1) % LV_ASYNC_DECODE_QUEUE_SIZE;
                uint32_t dst = (decoder->head + j) % LV_ASYNC_DECODE_QUEUE_SIZE;
                decoder->queue[dst] = decoder->queue[src];
            }
            decoder->count--;
        }
        else {
            /* READING — mark cancelled, on_read/on_close will cleanup */
            task->cancelled = true;
        }
        return LV_RESULT_OK;
    }
    return LV_RESULT_INVALID;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void timer_cb(uv_timer_t * handle)
{
    lv_async_decoder_t * decoder = handle->data;

    /* Advance past any DONE tasks */
    while(decoder->count > 0) {
        lv_async_decode_task_t * head = &decoder->queue[decoder->head];
        if(head->state != LV_ASYNC_DECODE_STATE_DONE) break;
        advance_queue(decoder);
    }

    /* Dispatch next PENDING task */
    if(decoder->count > 0) {
        lv_async_decode_task_t * head = &decoder->queue[decoder->head];
        if(head->state == LV_ASYNC_DECODE_STATE_PENDING) {
            dispatch_task(decoder, head);
        }
    }
}

static void dispatch_task(lv_async_decoder_t * decoder, lv_async_decode_task_t * task)
{
    if(is_in_image_cache(task->lvgl_path)) {
        task->result = LV_RESULT_OK;
        task_complete(task);
        return;
    }

    task->state = LV_ASYNC_DECODE_STATE_READING;
    task->open_req.data = task;
    uv_fs_open(decoder->loop, &task->open_req, task->posix_path,
               UV_FS_O_RDONLY, 0, on_open);
}

static void on_open(uv_fs_t * req)
{
    lv_async_decode_task_t * task = req->data;
    lv_async_decoder_t * decoder = task->decoder;
    ssize_t fd = req->result;
    fflush(stdout);
    uv_fs_req_cleanup(req);

    if(task->cancelled || fd < 0) {
        task->result = LV_RESULT_INVALID;
        if(fd >= 0) {
            task->fd = fd;
            task->close_req.data = task;
            uv_fs_close(decoder->loop, &task->close_req, fd, on_close);
        }
        else {
            task_complete(task);
        }
        return;
    }

    task->fd = fd;
    task->stat_req.data = task;
    uv_fs_fstat(decoder->loop, &task->stat_req, fd, on_stat);
}

static void on_stat(uv_fs_t * req)
{
    lv_async_decode_task_t * task = req->data;
    lv_async_decoder_t * decoder = task->decoder;
    uint64_t size = req->statbuf.st_size;
    uv_fs_req_cleanup(req);

    if(task->cancelled || size == 0) {
        task->result = LV_RESULT_INVALID;
        task->close_req.data = task;
        uv_fs_close(decoder->loop, &task->close_req, task->fd, on_close);
        return;
    }

    task->file_size = (uint32_t)size;
    task->buffer = lv_malloc(task->file_size);
    if(task->buffer == NULL) {
        task->result = LV_RESULT_INVALID;
        task->close_req.data = task;
        uv_fs_close(decoder->loop, &task->close_req, task->fd, on_close);
        return;
    }

    task->offset = 0;
    start_next_read(task);
}

static void start_next_read(lv_async_decode_task_t * task)
{
    lv_async_decoder_t * decoder = task->decoder;
    uint32_t remaining = task->file_size - task->offset;
    uint32_t to_read = remaining > LV_ASYNC_DECODE_CHUNK_SIZE
                       ? LV_ASYNC_DECODE_CHUNK_SIZE : remaining;

    task->uv_buf = uv_buf_init((char *)task->buffer + task->offset, to_read);
    task->read_req.data = task;
    uv_fs_read(decoder->loop, &task->read_req, task->fd,
               &task->uv_buf, 1, task->offset, on_read);
}

static void on_read(uv_fs_t * req)
{
    lv_async_decode_task_t * task = req->data;
    lv_async_decoder_t * decoder = task->decoder;
    ssize_t nread = req->result;
    uv_fs_req_cleanup(req);

    /* Cancel check point */
    if(task->cancelled || nread <= 0) {
        task->result = (nread == 0 && task->offset > 0)
                       ? LV_RESULT_OK : LV_RESULT_INVALID;
        task->close_req.data = task;
        uv_fs_close(decoder->loop, &task->close_req, task->fd, on_close);
        return;
    }

    task->offset += (uint32_t)nread;

    if(task->offset < task->file_size) {
        /* More chunks to read */
        start_next_read(task);
        return;
    }

    /* All bytes read — close file, then decode */
    task->close_req.data = task;
    uv_fs_close(decoder->loop, &task->close_req, task->fd, on_close);
}

static void on_close(uv_fs_t * req)
{
    lv_async_decode_task_t * task = req->data;
    uv_fs_req_cleanup(req);
    task->fd = -1;

    if(task->cancelled || task->buffer == NULL || task->result == LV_RESULT_INVALID) {
        if(task->buffer) {
            lv_free(task->buffer);
            task->buffer = NULL;
        }
        task->result = LV_RESULT_INVALID;
        task_complete(task);
        return;
    }

    /* DECODE on main thread */
    task->state = LV_ASYNC_DECODE_STATE_DECODING;

    if(is_in_image_cache(task->lvgl_path)) {
        task->result = LV_RESULT_OK;
    }
    else {
        lv_fs_prefetch_set(task->lvgl_path, task->buffer, task->offset);

        lv_image_decoder_dsc_t dsc;
        lv_result_t res = lv_image_decoder_open(&dsc, task->lvgl_path, NULL);
        if(res == LV_RESULT_OK) {
            lv_image_decoder_close(&dsc);
            task->result = LV_RESULT_OK;
        }
        else {
            task->result = LV_RESULT_INVALID;
        }

        lv_fs_prefetch_clear(task->lvgl_path);
    }

    lv_free(task->buffer);
    task->buffer = NULL;
    task_complete(task);
}

/** Mark task done, notify user, schedule queue advancement. */
static void task_complete(lv_async_decode_task_t * task)
{
    lv_async_decoder_t * decoder = task->decoder;

    if(task->cb && !task->cancelled) {
        task->cb(task->lvgl_path, task->result, task->user_data);
    }

    task->state = LV_ASYNC_DECODE_STATE_DONE;

    /* Schedule timer to advance queue on next loop iteration */
    uv_timer_start(&decoder->timer, timer_cb, 0, 0);
}

static void advance_queue(lv_async_decoder_t * decoder)
{
    decoder->head = (decoder->head + 1) % LV_ASYNC_DECODE_QUEUE_SIZE;
    decoder->count--;
}

static const char * default_path_map(lv_fs_drv_t * drv, const char * lvgl_path,
                                     char * buf, uint32_t buf_size)
{
    LV_UNUSED(drv);
    const char * p = lvgl_path;
    if(p[0] >= 'A' && p[0] <= 'Z' && p[1] == ':') p += 2;
    lv_strlcpy(buf, p, buf_size);
    return buf;
}

static bool is_in_image_cache(const char * path)
{
    lv_image_header_t header;
    if(lv_image_decoder_get_info(path, &header) != LV_RESULT_OK) return false;

    lv_image_decoder_dsc_t dsc;
    if(lv_image_decoder_open(&dsc, path, NULL) == LV_RESULT_OK) {
        lv_image_decoder_close(&dsc);
        return true;
    }
    return false;
}

static bool path_in_queue(lv_async_decoder_t * decoder, const char * path)
{
    for(uint32_t i = 0; i < decoder->count; i++) {
        uint32_t idx = (decoder->head + i) % LV_ASYNC_DECODE_QUEUE_SIZE;
        if(lv_strcmp(decoder->queue[idx].lvgl_path, path) == 0) return true;
    }
    return false;
}

#endif /* LV_USE_NUTTX_LIBUV */
#endif /* LV_USE_NUTTX */
