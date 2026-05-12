#if LV_BUILD_TEST
#include "../lvgl.h"
#include "../../src/misc/lv_fs_prefetch.h"

#include "unity/unity.h"
#include <string.h>

/* 'T' drive is registered unconditionally by lv_test_fs_init() in every test
 * configuration (full / minimal / etc). Prefetch fast path bypasses drv
 * callbacks entirely, so we only need any registered drive letter here. */
#define PF_DRIVE "T:"

static uint8_t sample[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    30, 31
};

static uint8_t other[] = { 0xAA, 0xBB, 0xCC, 0xDD };

void setUp(void)
{
    /* Make sure no leftover entries from previous test pollute the pool.
     * A sweep over all paths any test in this file ever sets is enough
     * because no other test touches the prefetch pool. */
    char path[32];

    lv_fs_prefetch_clear(PF_DRIVE "/prefetch_main");
    lv_fs_prefetch_clear(PF_DRIVE "/prefetch_empty");
    for(uint32_t i = 0; i <= LV_FS_PREFETCH_POOL_SIZE; i++) {
        lv_snprintf(path, sizeof(path), PF_DRIVE "/pf_slot_%u", (unsigned)i);
        lv_fs_prefetch_clear(path);
    }
}

void tearDown(void)
{
}

void test_prefetch_set_and_find(void)
{
    lv_fs_prefetch_entry_t * e = lv_fs_prefetch_find(PF_DRIVE "/prefetch_main");
    TEST_ASSERT_NULL(e);

    lv_fs_prefetch_set(PF_DRIVE "/prefetch_main", sample, sizeof(sample));
    e = lv_fs_prefetch_find(PF_DRIVE "/prefetch_main");
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_PTR(sample, e->buffer);
    TEST_ASSERT_EQUAL_UINT32(sizeof(sample), e->size);

    lv_fs_prefetch_clear(PF_DRIVE "/prefetch_main");
    TEST_ASSERT_NULL(lv_fs_prefetch_find(PF_DRIVE "/prefetch_main"));
}

void test_prefetch_reset_same_path(void)
{
    lv_fs_prefetch_set(PF_DRIVE "/prefetch_main", sample, sizeof(sample));
    lv_fs_prefetch_set(PF_DRIVE "/prefetch_main", other, sizeof(other));

    lv_fs_prefetch_entry_t * e = lv_fs_prefetch_find(PF_DRIVE "/prefetch_main");
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_PTR(other, e->buffer);
    TEST_ASSERT_EQUAL_UINT32(sizeof(other), e->size);

    lv_fs_prefetch_clear(PF_DRIVE "/prefetch_main");
}

void test_prefetch_fifo_eviction(void)
{
    /* Fill pool exactly to LV_FS_PREFETCH_POOL_SIZE, then push one extra: the
     * oldest entry must be evicted, the rest must stay findable. */
    char path[32];
    const uint32_t pool = LV_FS_PREFETCH_POOL_SIZE;

    for(uint32_t i = 0; i < pool; i++) {
        lv_snprintf(path, sizeof(path), PF_DRIVE "/pf_slot_%u", (unsigned)i);
        lv_fs_prefetch_set(path, sample, 4);
    }

    /* One more push -> slot 0 is evicted */
    lv_snprintf(path, sizeof(path), PF_DRIVE "/pf_slot_%u", (unsigned)pool);
    lv_fs_prefetch_set(path, sample, 4);

    lv_snprintf(path, sizeof(path), PF_DRIVE "/pf_slot_0");
    TEST_ASSERT_NULL(lv_fs_prefetch_find(path));

    for(uint32_t i = 1; i <= pool; i++) {
        lv_snprintf(path, sizeof(path), PF_DRIVE "/pf_slot_%u", (unsigned)i);
        TEST_ASSERT_NOT_NULL_MESSAGE(lv_fs_prefetch_find(path), path);
    }

    for(uint32_t i = 0; i <= pool; i++) {
        lv_snprintf(path, sizeof(path), PF_DRIVE "/pf_slot_%u", (unsigned)i);
        lv_fs_prefetch_clear(path);
    }
}

void test_prefetch_open_read_sequential(void)
{
    lv_fs_prefetch_set(PF_DRIVE "/prefetch_main", sample, sizeof(sample));

    lv_fs_file_t f;
    TEST_ASSERT_EQUAL(LV_FS_RES_OK,
                      lv_fs_open(&f, PF_DRIVE "/prefetch_main", LV_FS_MODE_RD));

    uint8_t buf[sizeof(sample)];
    uint32_t br = 0;
    TEST_ASSERT_EQUAL(LV_FS_RES_OK, lv_fs_read(&f, buf, sizeof(buf), &br));
    TEST_ASSERT_EQUAL_UINT32(sizeof(sample), br);
    TEST_ASSERT_EQUAL_MEMORY(sample, buf, sizeof(sample));

    TEST_ASSERT_EQUAL(LV_FS_RES_OK, lv_fs_close(&f));
    lv_fs_prefetch_clear(PF_DRIVE "/prefetch_main");
}

void test_prefetch_seek_tell(void)
{
    lv_fs_prefetch_set(PF_DRIVE "/prefetch_main", sample, sizeof(sample));

    lv_fs_file_t f;
    TEST_ASSERT_EQUAL(LV_FS_RES_OK,
                      lv_fs_open(&f, PF_DRIVE "/prefetch_main", LV_FS_MODE_RD));

    /* Seek to middle, tell should report it */
    TEST_ASSERT_EQUAL(LV_FS_RES_OK, lv_fs_seek(&f, 10, LV_FS_SEEK_SET));
    uint32_t pos = 0;
    TEST_ASSERT_EQUAL(LV_FS_RES_OK, lv_fs_tell(&f, &pos));
    TEST_ASSERT_EQUAL_UINT32(10, pos);

    /* Read forward from pos 10 */
    uint8_t buf[4];
    uint32_t br = 0;
    TEST_ASSERT_EQUAL(LV_FS_RES_OK, lv_fs_read(&f, buf, sizeof(buf), &br));
    TEST_ASSERT_EQUAL_UINT32(4, br);
    TEST_ASSERT_EQUAL_MEMORY(&sample[10], buf, 4);

    /* Seek from end */
    TEST_ASSERT_EQUAL(LV_FS_RES_OK, lv_fs_seek(&f, 5, LV_FS_SEEK_END));
    TEST_ASSERT_EQUAL(LV_FS_RES_OK, lv_fs_tell(&f, &pos));
    TEST_ASSERT_EQUAL_UINT32(sizeof(sample) - 5, pos);

    TEST_ASSERT_EQUAL(LV_FS_RES_OK, lv_fs_close(&f));
    lv_fs_prefetch_clear(PF_DRIVE "/prefetch_main");
}

void test_prefetch_empty_buffer(void)
{
    /* Zero-size entry: find succeeds, open+read returns 0 bytes. */
    lv_fs_prefetch_set(PF_DRIVE "/prefetch_empty", sample, 0);

    lv_fs_file_t f;
    TEST_ASSERT_EQUAL(LV_FS_RES_OK,
                      lv_fs_open(&f, PF_DRIVE "/prefetch_empty", LV_FS_MODE_RD));

    uint8_t buf[8];
    uint32_t br = 42;
    lv_fs_read(&f, buf, sizeof(buf), &br);
    TEST_ASSERT_EQUAL_UINT32(0, br);

    TEST_ASSERT_EQUAL(LV_FS_RES_OK, lv_fs_close(&f));
    lv_fs_prefetch_clear(PF_DRIVE "/prefetch_empty");
}

#endif
