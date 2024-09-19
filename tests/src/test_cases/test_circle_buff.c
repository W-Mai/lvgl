#if LV_BUILD_TEST
#include "../lvgl.h"
#include "../../lvgl_private.h"

#include "unity/unity.h"

static lv_circle_buff_t * circle_buff;

#define CIRCLE_BUFF_CAPACITY 4

void setUp(void)
{
    circle_buff = lv_circle_buff_create(CIRCLE_BUFF_CAPACITY, sizeof(int32_t));

    TEST_ASSERT_EQUAL_UINT32(lv_circle_buff_capacity(circle_buff), CIRCLE_BUFF_CAPACITY);
    TEST_ASSERT_EQUAL_UINT32(0, lv_circle_buff_size(circle_buff));

    /**
     * Write values to the circle buffer. The max size of the buffer is CIRCLE_BUFF_CAPACITY.
     * When the buffer is full, the write operation should return LV_RESULT_INVALID.
     */
    for(int32_t i = 0; i < CIRCLE_BUFF_CAPACITY * 2; i++) {
        const lv_result_t res = lv_circle_buff_write(circle_buff, &i);

        if(i < CIRCLE_BUFF_CAPACITY) TEST_ASSERT_EQUAL(LV_RESULT_OK, res);
        else TEST_ASSERT_EQUAL(LV_RESULT_INVALID, res);
    }

    /**
     * After writing values to the buffer, the size of the buffer should be equal to the capacity.
     */
    TEST_ASSERT_EQUAL_UINT32(lv_circle_buff_size(circle_buff), CIRCLE_BUFF_CAPACITY);
}

void tearDown(void)
{
    lv_circle_buff_destroy(circle_buff);
    circle_buff = NULL;
}

void test_circle_buff_read_write_peek_values(void)
{
    /**
     * Read 1 value from the buffer.
     */
    {
        int32_t value;
        const lv_result_t res = lv_circle_buff_read(circle_buff, &value);

        TEST_ASSERT_EQUAL(LV_RESULT_OK, res);
        TEST_ASSERT_EQUAL_INT32(0, value);
    }

    /**
     * Peek values will not advance the read and write cursors.
     * If the peek index is greater than the size of the buffer, it will returns looply.
     */
    for(int32_t i = 0, j = 1; i < CIRCLE_BUFF_CAPACITY * 10; i++, j++) {
        int32_t value;
        const lv_result_t res = lv_circle_buff_peek_at(circle_buff, i, &value);

        TEST_ASSERT_EQUAL(LV_RESULT_OK, res);
        TEST_ASSERT_EQUAL_INT32(j, value);

        if(j == 3) j = 0;
    }

    /**
     * Read values from the circle buffer. The max size of the buffer is CIRCLE_BUFF_CAPACITY.
     * When the buffer is empty, the read operation should return LV_RESULT_INVALID.
     */
    for(int32_t i = 1; i < CIRCLE_BUFF_CAPACITY * 2; i++) {
        int32_t value;
        const lv_result_t res = lv_circle_buff_read(circle_buff, &value);

        if(i < CIRCLE_BUFF_CAPACITY) {
            TEST_ASSERT_EQUAL(LV_RESULT_OK, res);
            TEST_ASSERT_EQUAL_INT32(i, value);
        }
        else {
            TEST_ASSERT_EQUAL(LV_RESULT_INVALID, res);
        }
    }

    /**
     * After reading values from the buffer, the size of the buffer should be equal to 0.
     */
    TEST_ASSERT_EQUAL_INT32(0, lv_circle_buff_size(circle_buff));
}

void test_circle_buff_skip_values(void)
{
    /**
     * Skip 1 value from the buffer.
     */
    {
        const lv_result_t res = lv_circle_buff_skip(circle_buff);

        TEST_ASSERT_EQUAL(LV_RESULT_OK, res);
    }

    /**
     * Skip values from the circle buffer. The max size of the buffer is CIRCLE_BUFF_CAPACITY.
     * When the buffer is empty, the skip operation should return LV_RESULT_INVALID.
     */
    for(int32_t i = 1; i < CIRCLE_BUFF_CAPACITY * 2; i++) {
        const lv_result_t res = lv_circle_buff_skip(circle_buff);

        if(i < CIRCLE_BUFF_CAPACITY) {
            TEST_ASSERT_EQUAL(LV_RESULT_OK, res);
        }
        else {
            TEST_ASSERT_EQUAL(LV_RESULT_INVALID, res);
        }
    }

    /**
     * After skipping values from the buffer, the size of the buffer should be equal to 0.
     */
    TEST_ASSERT_EQUAL_INT32(0, lv_circle_buff_size(circle_buff));
}

void test_circle_buff_read_after_read_and_write(void)
{
    /**
     * Read 1 value from the buffer.
     */
    {
        int32_t value;
        const lv_result_t res = lv_circle_buff_read(circle_buff, &value);

        TEST_ASSERT_EQUAL(LV_RESULT_OK, res);
        TEST_ASSERT_EQUAL_INT32(0, value);
    }

    /**
     * Write 1 value to the buffer.
     */
    {
        const int32_t value = 4;
        const lv_result_t res = lv_circle_buff_write(circle_buff, &value);

        TEST_ASSERT_EQUAL(LV_RESULT_OK, res);
    }

    const int32_t expected[] = {4, 1, 2, 3};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, ((lv_array_t *)circle_buff)->data, 4);

    /**
     * Read values from the circle buffer. The max size of the buffer is CIRCLE_BUFF_CAPACITY.
     * When the buffer is empty, the read operation should return LV_RESULT_INVALID.
     */
    for(int32_t i = 1; i < CIRCLE_BUFF_CAPACITY * 2; i++) {
        int32_t value;
        const lv_result_t res = lv_circle_buff_read(circle_buff, &value);

        if(i <= CIRCLE_BUFF_CAPACITY) {
            TEST_ASSERT_EQUAL(LV_RESULT_OK, res);
            TEST_ASSERT_EQUAL_INT32(i, value);
        }
        else {
            TEST_ASSERT_EQUAL(LV_RESULT_INVALID, res);
        }
    }
}

#endif
