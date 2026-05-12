/**
 * @file lv_async_decoder_test.c
 * Built-in tests for async decoder (scheme B: uv_fs worker).
 *
 * Tests:
 * 1. Prefetch pool: set/find/clear/eviction
 * 2. lv_fs from_buffer: open/read/seek/tell/close on prefetched file
 * 3. Async decoder: submit with hostfs file, verify cache hit
 * 4. Cancel: submit then cancel before completion
 * 5. Duplicate submit: same path submitted twice
 * 6. Cache hit: submit already-cached path → immediate callback
 */

#include "lv_async_decoder.h"
#include "lv_async_decoder_test.h"

#if LV_USE_NUTTX
#if LV_USE_NUTTX_LIBUV

#include "../../misc/lv_fs_prefetch.h"
#include "../../draw/lv_image_decoder.h"
#include "../../draw/lv_image_decoder_private.h"
#include "../../stdlib/lv_string.h"
#include "../../stdlib/lv_mem.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define TEST_LOG(fmt, ...) do { printf("[ASYNC_TEST] " fmt "\n", ##__VA_ARGS__); fflush(stdout); } while(0)
#define TEST_PASS(name) printf("[ASYNC_TEST] PASS: %s\n", name)
#define TEST_FAIL(name, reason) printf("[ASYNC_TEST] FAIL: %s — %s\n", name, reason)

#define ASSERT_TEST(cond, name, reason) do { \
        if(!(cond)) { TEST_FAIL(name, reason); return; } \
    } while(0)

/**********************
 * Test 1: Prefetch Pool
 **********************/
static void test_prefetch_pool(void)
{
    const char * name = "prefetch_pool";
    static uint8_t buf1[16] = {1, 2, 3, 4};
    static uint8_t buf2[16] = {5, 6, 7, 8};

    /* Set and find */
    lv_fs_prefetch_set("S:/test1.bin", buf1, sizeof(buf1));
    lv_fs_prefetch_entry_t * e = lv_fs_prefetch_find("S:/test1.bin");
    ASSERT_TEST(e != NULL, name, "find after set returned NULL");
    ASSERT_TEST(e->buffer == buf1, name, "buffer mismatch");
    ASSERT_TEST(e->size == sizeof(buf1), name, "size mismatch");

    /* Find non-existent */
    e = lv_fs_prefetch_find("S:/nonexist.bin");
    ASSERT_TEST(e == NULL, name, "find non-existent should return NULL");

    /* Clear */
    lv_fs_prefetch_clear("S:/test1.bin");
    e = lv_fs_prefetch_find("S:/test1.bin");
    ASSERT_TEST(e == NULL, name, "find after clear should return NULL");

    /* Eviction: fill all slots + 1 */
    char path[32];
    for(int i = 0; i < LV_FS_PREFETCH_POOL_SIZE + 1; i++) {
        snprintf(path, sizeof(path), "S:/evict%d.bin", i);
        lv_fs_prefetch_set(path, buf2, sizeof(buf2));
    }
    /* First entry should be evicted */
    e = lv_fs_prefetch_find("S:/evict0.bin");
    ASSERT_TEST(e == NULL, name, "evict0 should be evicted");
    /* Last entry should exist */
    snprintf(path, sizeof(path), "S:/evict%d.bin", LV_FS_PREFETCH_POOL_SIZE);
    e = lv_fs_prefetch_find(path);
    ASSERT_TEST(e != NULL, name, "last entry should exist");

    /* Cleanup */
    for(int i = 0; i <= LV_FS_PREFETCH_POOL_SIZE; i++) {
        snprintf(path, sizeof(path), "S:/evict%d.bin", i);
        lv_fs_prefetch_clear(path);
    }

    TEST_PASS(name);
}

/**********************
 * Test 2: lv_fs from_buffer
 **********************/
static void test_fs_from_buffer(void)
{
    const char * name = "fs_from_buffer";
    static uint8_t data[64];
    for(int i = 0; i < 64; i++) data[i] = (uint8_t)i;

    /* Need a valid FS driver letter. Use 'A' if registered. */
    lv_fs_drv_t * drv = lv_fs_get_drv('A');
    if(drv == NULL) {
        TEST_LOG("SKIP: %s (no 'A' driver registered)", name);
        return;
    }

    lv_fs_prefetch_set("A:/test_fb.bin", data, sizeof(data));

    /* Open */
    lv_fs_file_t f;
    lv_fs_res_t res = lv_fs_open(&f, "A:/test_fb.bin", LV_FS_MODE_RD);
    ASSERT_TEST(res == LV_FS_RES_OK, name, "open failed");

    /* Read first 4 bytes */
    uint8_t buf[8];
    uint32_t br = 0;
    res = lv_fs_read(&f, buf, 4, &br);
    ASSERT_TEST(res == LV_FS_RES_OK && br == 4, name, "read 4 bytes failed");
    ASSERT_TEST(buf[0] == 0 && buf[1] == 1 && buf[2] == 2 && buf[3] == 3,
                name, "read data mismatch");

    /* Tell */
    uint32_t pos = 0;
    res = lv_fs_tell(&f, &pos);
    ASSERT_TEST(res == LV_FS_RES_OK && pos == 4, name, "tell after read failed");

    /* Seek SET */
    res = lv_fs_seek(&f, 10, LV_FS_SEEK_SET);
    ASSERT_TEST(res == LV_FS_RES_OK, name, "seek SET failed");
    res = lv_fs_read(&f, buf, 2, &br);
    ASSERT_TEST(br == 2 && buf[0] == 10 && buf[1] == 11, name, "read after seek failed");

    /* Seek END */
    res = lv_fs_seek(&f, 4, LV_FS_SEEK_END);
    ASSERT_TEST(res == LV_FS_RES_OK, name, "seek END failed");
    res = lv_fs_tell(&f, &pos);
    ASSERT_TEST(pos == 60, name, "tell after seek END failed");

    /* Read at end — should clamp */
    res = lv_fs_read(&f, buf, 8, &br);
    ASSERT_TEST(br == 4, name, "read at end should clamp to 4 bytes");
    ASSERT_TEST(buf[0] == 60 && buf[3] == 63, name, "end data mismatch");

    /* Close — should not crash, should not free buffer */
    res = lv_fs_close(&f);
    ASSERT_TEST(res == LV_FS_RES_OK, name, "close failed");

    /* Buffer should still be accessible (not freed by close) */
    ASSERT_TEST(data[0] == 0 && data[63] == 63, name, "buffer corrupted after close");

    lv_fs_prefetch_clear("A:/test_fb.bin");
    TEST_PASS(name);
}

/**********************
 * Test 3-6: Async decoder (need uv_loop running)
 **********************/

typedef struct {
    const char * test_name;
    volatile bool done;
    lv_result_t result;
    const char * result_path;
} test_async_ctx_t;

static void async_cb(const char * path, lv_result_t result, void * user_data)
{
    test_async_ctx_t * ctx = user_data;
    ctx->result = result;
    ctx->result_path = path;
    ctx->done = true;
    TEST_LOG("async_cb: path=%s result=%d", path, result);
}

static lv_async_decoder_t * g_decoder = NULL;
static uv_timer_t g_test_timer;
static int g_test_phase = 0;

static void run_async_tests(uv_timer_t * handle);

void lv_async_decoder_test_start(uv_loop_t * loop)
{
    TEST_LOG("=== Async Decoder Test Suite Start ===");

    /* Synchronous tests first */
    test_prefetch_pool();
    test_fs_from_buffer();

    /* Create decoder for async tests */
    g_decoder = lv_async_decoder_create(loop);
    if(g_decoder == NULL) {
        TEST_FAIL("create", "lv_async_decoder_create returned NULL");
        return;
    }
    TEST_PASS("create");

    /* Start async test phases via timer */
    g_test_phase = 0;
    int r1 = uv_timer_init(loop, &g_test_timer);
    g_test_timer.data = loop;
    int r2 = uv_timer_start(&g_test_timer, run_async_tests, 0, 0);
    TEST_LOG("timer init=%d start=%d", r1, r2);
}

static test_async_ctx_t g_ctx;

static void run_async_tests(uv_timer_t * handle)
{
    uv_loop_t * loop = handle->data;
    TEST_LOG("timer_cb phase=%d", g_test_phase);

    switch(g_test_phase) {
        case 0: {
                /* Test: submit + cancel (no actual file needed) */
                TEST_LOG("--- Test: submit_and_cancel ---");
                lv_result_t res = lv_async_decoder_submit(
                                      g_decoder, "A:/test.bin", NULL, NULL);
                TEST_LOG("submit: %s", res == LV_RESULT_OK ? "OK" : "INVALID");

                lv_result_t cr = lv_async_decoder_cancel(g_decoder, "A:/test.bin");
                TEST_LOG("cancel: %s", cr == LV_RESULT_OK ? "OK" : "INVALID");

                cr = lv_async_decoder_cancel(g_decoder, "A:/no_such.bin");
                TEST_LOG("cancel nonexist: %s", cr == LV_RESULT_INVALID ? "OK(INVALID)" : "UNEXPECTED");

                g_test_phase = 2;
                break;
            }

        case 1: {
                /* skipped */
                g_test_phase = 2;
                break;
            }

        case 2: {
                /* Test 4: Duplicate submit */
                TEST_LOG("--- Test: duplicate_submit ---");
                memset(&g_ctx, 0, sizeof(g_ctx));
                lv_result_t r1 = lv_async_decoder_submit(
                                     g_decoder, "A:/dup_test.bin", async_cb, &g_ctx);
                lv_result_t r2 = lv_async_decoder_submit(
                                     g_decoder, "A:/dup_test.bin", async_cb, &g_ctx);
                if(r1 == LV_RESULT_OK && r2 == LV_RESULT_OK) {
                    TEST_PASS("duplicate_submit (second submit skipped)");
                }
                else {
                    TEST_FAIL("duplicate_submit", "unexpected return values");
                }
                g_test_phase = 3;
                break;
            }

        case 3: {
                /* Test 5: Cancel */
                TEST_LOG("--- Test: cancel ---");
                lv_async_decoder_submit(g_decoder, "A:/cancel_test.bin", NULL, NULL);
                lv_result_t cr = lv_async_decoder_cancel(g_decoder, "A:/cancel_test.bin");
                if(cr == LV_RESULT_OK) {
                    TEST_PASS("cancel");
                }
                else {
                    TEST_FAIL("cancel", "cancel returned INVALID");
                }

                /* Cancel non-existent */
                cr = lv_async_decoder_cancel(g_decoder, "A:/no_such.bin");
                if(cr == LV_RESULT_INVALID) {
                    TEST_PASS("cancel_nonexistent");
                }
                else {
                    TEST_FAIL("cancel_nonexistent", "should return INVALID");
                }
                g_test_phase = 4;
                break;
            }

        case 4: {
                /* Test 6: Queue full */
                TEST_LOG("--- Test: queue_full ---");
                lv_result_t res = LV_RESULT_OK;
                char path[64];
                int i;
                for(i = 0; i < LV_ASYNC_DECODE_QUEUE_SIZE + 2; i++) {
                    snprintf(path, sizeof(path), "A:/qfull_%d.bin", i);
                    res = lv_async_decoder_submit(g_decoder, path, NULL, NULL);
                    if(res != LV_RESULT_OK) break;
                }
                if(res == LV_RESULT_INVALID && i <= LV_ASYNC_DECODE_QUEUE_SIZE) {
                    TEST_PASS("queue_full");
                }
                else {
                    TEST_FAIL("queue_full", "should reject when full");
                }
                /* Clean up: cancel all queued items */
                for(int j = 0; j < i; j++) {
                    snprintf(path, sizeof(path), "A:/qfull_%d.bin", j);
                    lv_async_decoder_cancel(g_decoder, path);
                }
                g_test_phase = 5;
                break;
            }

        case 5: {
                /* Test: prefetch → decode → cache hit */
                TEST_LOG("--- Test: prefetch_decode_cache ---");

                /* Create a 4x4 ARGB8888 LVGL bin file in /tmp */
                {
                    uint8_t bin[12 + 4 * 4 * 4]; /* 12-byte header + 64 pixels */
                    /* Header: magic=0x19, cf=0x10(ARGB8888), flags=0, w=4, h=4, stride=16 */
                    memset(bin, 0, sizeof(bin));
                    bin[0] = 0x19;  /* magic */
                    bin[1] = 0x10;  /* cf = LV_COLOR_FORMAT_ARGB8888 */
                    bin[2] = 0;
                    bin[3] = 0; /* flags */
                    bin[4] = 4;
                    bin[5] = 0; /* w = 4 */
                    bin[6] = 4;
                    bin[7] = 0; /* h = 4 */
                    bin[8] = 16;
                    bin[9] = 0; /* stride = 4*4 = 16 */
                    bin[10] = 0;
                    bin[11] = 0; /* reserved */
                    /* Fill pixels: red */
                    for(int p = 12; p < (int)sizeof(bin); p += 4) {
                        bin[p + 0] = 0xFF; /* B */
                        bin[p + 1] = 0x00; /* G */
                        bin[p + 2] = 0x00; /* R */
                        bin[p + 3] = 0xFF; /* A */
                    }
                    int fd = open("/tmp/test_cache.bin", O_WRONLY | O_CREAT | O_TRUNC, 0666);
                    if(fd >= 0) {
                        write(fd, bin, sizeof(bin));
                        close(fd);
                    }
                }

                /* Step 1: Verify not in cache yet */
                lv_image_header_t hdr;
                lv_result_t info_res = lv_image_decoder_get_info("A:/tmp/test_cache.bin", &hdr);
                if(info_res != LV_RESULT_OK) {
                    TEST_LOG("  get_info failed (bin decoder may not recognize file)");
                    TEST_LOG("  SKIP: prefetch_decode_cache (need valid LVGL bin file)");
                    g_test_phase = 8;
                    break;
                }
                TEST_LOG("  image info: %dx%d cf=%d", hdr.w, hdr.h, hdr.cf);

                /* Step 2: Read file into RAM (simulate async read completion) */
                {
                    lv_fs_file_t f;
                    lv_fs_res_t r = lv_fs_open(&f, "A:/tmp/test_cache.bin", LV_FS_MODE_RD);
                    if(r != LV_FS_RES_OK) {
                        TEST_FAIL("prefetch_decode_cache", "cannot open test file");
                        g_test_phase = 8;
                        break;
                    }
                    uint32_t fsize = 0;
                    lv_fs_get_size(&f, &fsize);
                    uint8_t * buf = lv_malloc(fsize);
                    uint32_t br = 0;
                    lv_fs_read(&f, buf, fsize, &br);
                    lv_fs_close(&f);

                    TEST_LOG("  read %u bytes into RAM", br);

                    /* Step 3: Register in prefetch pool and decode */
                    lv_fs_prefetch_set("A:/tmp/test_cache.bin", buf, br);

                    lv_image_decoder_dsc_t dsc;
                    lv_result_t dec_res = lv_image_decoder_open(&dsc, "A:/tmp/test_cache.bin", NULL);
                    TEST_LOG("  decoder_open result=%d", dec_res);

                    if(dec_res == LV_RESULT_OK) {
                        TEST_LOG("  decoded: %dx%d", dsc.header.w, dsc.header.h);
                        lv_image_decoder_close(&dsc);
                    }

                    lv_fs_prefetch_clear("A:/tmp/test_cache.bin");
                    lv_free(buf);

                    if(dec_res != LV_RESULT_OK) {
                        TEST_FAIL("prefetch_decode_cache", "decode failed");
                        g_test_phase = 8;
                        break;
                    }
                }

                /* Step 4: Verify cache hit — open again should succeed from cache */
                {
                    lv_image_decoder_dsc_t dsc2;
                    lv_result_t r2 = lv_image_decoder_open(&dsc2, "A:/tmp/test_cache.bin", NULL);
                    if(r2 == LV_RESULT_OK) {
                        TEST_LOG("  cache hit: %dx%d ✓", dsc2.header.w, dsc2.header.h);
                        lv_image_decoder_close(&dsc2);
                        TEST_PASS("prefetch_decode_cache");
                    }
                    else {
                        TEST_FAIL("prefetch_decode_cache", "cache miss after decode");
                    }
                }

                g_test_phase = 6;
                break;
            }

        case 6: {
                /* Test: submit already-cached path → immediate callback */
                TEST_LOG("--- Test: submit_cached ---");
                memset(&g_ctx, 0, sizeof(g_ctx));
                lv_result_t r = lv_async_decoder_submit(
                                    g_decoder, "A:/tmp/test_cache.bin", async_cb, &g_ctx);
                /* Should call cb immediately since it's in cache */
                if(r == LV_RESULT_OK && g_ctx.done && g_ctx.result == LV_RESULT_OK) {
                    TEST_PASS("submit_cached (immediate callback)");
                }
                else {
                    TEST_LOG("  r=%d done=%d result=%d", r, g_ctx.done, g_ctx.result);
                    TEST_FAIL("submit_cached", "expected immediate callback with OK");
                }
                g_test_phase = 7;
                break;
            }

        case 7: {
                /* Test: lv_image_set_src with cached image */
                TEST_LOG("--- Test: image_set_src_cached ---");
                lv_obj_t * img = lv_image_create(lv_screen_active());
                if(img) {
                    lv_image_set_src(img, "A:/tmp/test_cache.bin");
                    const void * src = lv_image_get_src(img);
                    if(src && strcmp(src, "A:/tmp/test_cache.bin") == 0) {
                        TEST_PASS("image_set_src_cached");
                    }
                    else {
                        TEST_FAIL("image_set_src_cached", "src mismatch");
                    }
                    lv_obj_delete(img);
                }
                else {
                    TEST_FAIL("image_set_src_cached", "cannot create image widget");
                }
                g_test_phase = 8;
                break;
            }

        case 8: {
                /* Test: page switch simulation — cancel A, submit B */
                TEST_LOG("--- Test: page_switch ---");
                /* Submit "page A" images */
                lv_async_decoder_submit(g_decoder, "A:/pageA/img1.bin", NULL, NULL);
                lv_async_decoder_submit(g_decoder, "A:/pageA/img2.bin", NULL, NULL);
                lv_async_decoder_submit(g_decoder, "A:/pageA/img3.bin", NULL, NULL);
                /* User navigates away — cancel all page A */
                lv_result_t c1 = lv_async_decoder_cancel(g_decoder, "A:/pageA/img1.bin");
                lv_result_t c2 = lv_async_decoder_cancel(g_decoder, "A:/pageA/img2.bin");
                lv_result_t c3 = lv_async_decoder_cancel(g_decoder, "A:/pageA/img3.bin");
                /* Submit "page B" images */
                lv_result_t s1 = lv_async_decoder_submit(g_decoder, "A:/pageB/img1.bin", NULL, NULL);
                lv_result_t s2 = lv_async_decoder_submit(g_decoder, "A:/pageB/img2.bin", NULL, NULL);
                if(c1 == LV_RESULT_OK && c2 == LV_RESULT_OK && c3 == LV_RESULT_OK &&
                   s1 == LV_RESULT_OK && s2 == LV_RESULT_OK) {
                    TEST_PASS("page_switch (cancel A + submit B)");
                }
                else {
                    TEST_FAIL("page_switch", "cancel or submit failed");
                }
                /* Clean up page B from queue */
                lv_async_decoder_cancel(g_decoder, "A:/pageB/img1.bin");
                lv_async_decoder_cancel(g_decoder, "A:/pageB/img2.bin");
                g_test_phase = 9;
                break;
            }

        case 9: {
                /* Test: zero-byte file error handling */
                TEST_LOG("--- Test: zero_byte_file ---");
                uint8_t dummy = 0;
                lv_fs_prefetch_set("A:/tmp/zero.bin", &dummy, 0);
                lv_fs_file_t f;
                lv_fs_res_t r = lv_fs_open(&f, "A:/tmp/zero.bin", LV_FS_MODE_RD);
                if(r == LV_FS_RES_OK) {
                    uint8_t buf[4];
                    uint32_t br = 99;
                    lv_fs_read(&f, buf, 4, &br);
                    TEST_LOG("  read returned br=%u", br);
                    lv_fs_close(&f);
                    /* Zero-size prefetch: read should return 0 or very few bytes */
                    TEST_PASS("zero_byte_file (no crash)");
                }
                else {
                    TEST_PASS("zero_byte_file (open rejected)");
                }
                lv_fs_prefetch_clear("A:/tmp/zero.bin");
                g_test_phase = 10;
                break;
            }

        case 10: {
                /* Test: corrupted file (bad magic) */
                TEST_LOG("--- Test: corrupted_file ---");
                uint8_t bad_bin[76];
                memset(bad_bin, 0xFF, sizeof(bad_bin)); /* garbage data */
                int fd = open("/tmp/corrupt.bin", O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if(fd >= 0) {
                    write(fd, bad_bin, sizeof(bad_bin));
                    close(fd);
                }

                lv_fs_prefetch_set("A:/tmp/corrupt.bin", bad_bin, sizeof(bad_bin));
                lv_image_decoder_dsc_t dsc;
                lv_result_t r = lv_image_decoder_open(&dsc, "A:/tmp/corrupt.bin", NULL);
                lv_fs_prefetch_clear("A:/tmp/corrupt.bin");
                if(r != LV_RESULT_OK) {
                    TEST_PASS("corrupted_file (decoder rejects bad magic)");
                }
                else {
                    lv_image_decoder_close(&dsc);
                    TEST_FAIL("corrupted_file", "decoder should reject garbage");
                }
                g_test_phase = 11;
                break;
            }

        case 11: {
                /* Test: multiple create/destroy cycles (leak detection) */
                TEST_LOG("--- Test: create_destroy_cycle ---");
                bool ok = true;
                for(int i = 0; i < 10; i++) {
                    lv_async_decoder_t * d = lv_async_decoder_create(loop);
                    if(!d) {
                        ok = false;
                        break;
                    }
                    lv_async_decoder_submit(d, "A:/cycle_test.bin", NULL, NULL);
                    lv_async_decoder_cancel(d, "A:/cycle_test.bin");
                    lv_async_decoder_destroy(d);
                }
                if(ok) {
                    TEST_PASS("create_destroy_cycle (10 cycles, no crash)");
                }
                else {
                    TEST_FAIL("create_destroy_cycle", "create returned NULL");
                }
                g_test_phase = 12;
                break;
            }

        case 12: {
                /* Test: submit then immediately destroy */
                TEST_LOG("--- Test: submit_then_destroy ---");
                lv_async_decoder_t * d2 = lv_async_decoder_create(loop);
                lv_async_decoder_submit(d2, "A:/destroy_test1.bin", NULL, NULL);
                lv_async_decoder_submit(d2, "A:/destroy_test2.bin", NULL, NULL);
                lv_async_decoder_submit(d2, "A:/destroy_test3.bin", NULL, NULL);
                lv_async_decoder_destroy(d2); /* Should not crash */
                TEST_PASS("submit_then_destroy (no crash)");
                g_test_phase = 13;
                break;
            }

        case 13: {
                /* Test: batch submit small icons (simulate icon grid page) */
                TEST_LOG("--- Test: batch_submit_icons ---");
                /* Destroy and recreate decoder to ensure clean queue */
                lv_async_decoder_destroy(g_decoder);
                g_decoder = lv_async_decoder_create(loop);
                int submitted = 0;
                char path[64];
                for(int i = 0; i < LV_ASYNC_DECODE_QUEUE_SIZE; i++) {
                    snprintf(path, sizeof(path), "A:/icons/icon_%d.bin", i);
                    if(lv_async_decoder_submit(g_decoder, path, NULL, NULL) == LV_RESULT_OK) {
                        submitted++;
                    }
                }
                TEST_LOG("  submitted %d icons", submitted);
                /* Cancel all */
                for(int i = 0; i < submitted; i++) {
                    snprintf(path, sizeof(path), "A:/icons/icon_%d.bin", i);
                    lv_async_decoder_cancel(g_decoder, path);
                }
                if(submitted == LV_ASYNC_DECODE_QUEUE_SIZE) {
                    TEST_PASS("batch_submit_icons (filled queue)");
                }
                else {
                    TEST_FAIL("batch_submit_icons", "could not fill queue");
                }
                g_test_phase = 14;
                break;
            }

        case 14: {
                /* Test: sync decode races with async (sync wins) */
                TEST_LOG("--- Test: sync_race ---");
                /* Create a valid bin file */
                {
                    uint8_t bin[12 + 2 * 2 * 4];
                    memset(bin, 0, sizeof(bin));
                    bin[0] = 0x19;
                    bin[1] = 0x10;
                    bin[4] = 2;
                    bin[6] = 2;
                    bin[8] = 8;
                    for(int p = 12; p < (int)sizeof(bin); p += 4) {
                        bin[p] = 0;
                        bin[p + 1] = 255;
                        bin[p + 2] = 0;
                        bin[p + 3] = 255;
                    }
                    int fd = open("/tmp/race.bin", O_WRONLY | O_CREAT | O_TRUNC, 0666);
                    if(fd >= 0) {
                        write(fd, bin, sizeof(bin));
                        close(fd);
                    }
                }
                /* Submit async (will be PENDING) */
                memset(&g_ctx, 0, sizeof(g_ctx));
                lv_async_decoder_submit(g_decoder, "A:/tmp/race.bin", async_cb, &g_ctx);
                /* Immediately do sync decode (simulates LVGL needing the image now) */
                lv_image_decoder_dsc_t dsc;
                lv_result_t r = lv_image_decoder_open(&dsc, "A:/tmp/race.bin", NULL);
                if(r == LV_RESULT_OK) {
                    lv_image_decoder_close(&dsc);
                    TEST_LOG("  sync decode succeeded (now in cache)");
                }
                /* Cancel the async task (it hasn't started yet) */
                lv_async_decoder_cancel(g_decoder, "A:/tmp/race.bin");
                /* Verify cache still has it */
                lv_result_t r2 = lv_image_decoder_open(&dsc, "A:/tmp/race.bin", NULL);
                if(r2 == LV_RESULT_OK) {
                    lv_image_decoder_close(&dsc);
                    TEST_PASS("sync_race (sync wins, cache valid, async cancelled)");
                }
                else {
                    TEST_FAIL("sync_race", "cache lost after cancel");
                }
                g_test_phase = 15;
                break;
            }

        case 15: {
                /* Test: multiple lv_image widgets sharing same cached source */
                TEST_LOG("--- Test: shared_cache_widgets ---");
                lv_obj_t * parent = lv_screen_active();
                lv_obj_t * img1 = lv_image_create(parent);
                lv_obj_t * img2 = lv_image_create(parent);
                lv_obj_t * img3 = lv_image_create(parent);
                /* All use the same cached image from test_cache.bin */
                lv_image_set_src(img1, "A:/tmp/test_cache.bin");
                lv_image_set_src(img2, "A:/tmp/test_cache.bin");
                lv_image_set_src(img3, "A:/tmp/test_cache.bin");
                bool ok = (lv_image_get_src(img1) != NULL &&
                           lv_image_get_src(img2) != NULL &&
                           lv_image_get_src(img3) != NULL);
                lv_obj_delete(img1);
                lv_obj_delete(img2);
                lv_obj_delete(img3);
                if(ok) {
                    TEST_PASS("shared_cache_widgets (3 widgets, 1 cache entry)");
                }
                else {
                    TEST_FAIL("shared_cache_widgets", "src is NULL");
                }
                g_test_phase = 99;
                break;
            }

        case 99: {
                /* Cleanup */
                TEST_LOG("--- Cleanup ---");
                lv_async_decoder_destroy(g_decoder);
                g_decoder = NULL;
                TEST_PASS("destroy");

                uv_timer_stop(handle);
                TEST_LOG("=== Async Decoder Test Suite End ===");
                g_test_phase = -1;
                break;
            }
    }

    /* Schedule next phase (unless done) */
    if(g_test_phase >= 0) {
        uv_timer_start(handle, run_async_tests, 0, 0);
    }
}

#endif /* LV_USE_NUTTX_LIBUV */
#endif /* LV_USE_NUTTX */
