/**
 * @file lv_circle_buff.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_circle_buff.h"
#include "lv_array.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

struct _lv_circle_buff_t {
    lv_array_t array;
    uint32_t head;
    uint32_t tail;    /**< The next write position */

    bool inner_alloc; /**< true: the array is allocated by the buffer, false: the array is created from an external buffer */
};

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void circle_buff_prepare_empty(lv_circle_buff_t * circle_buff);

/**********************
 *  GLOBAL VARIABLES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_circle_buff_t * lv_circle_buff_create(const uint32_t capacity, const uint32_t element_size)
{
    lv_circle_buff_t * circle_buff = lv_malloc(sizeof(lv_circle_buff_t));
    LV_ASSERT_MALLOC(circle_buff);

    if(circle_buff == NULL) {
        return NULL;
    }

    lv_array_init(&circle_buff->array, capacity, element_size);
    circle_buff->head = 0;
    circle_buff->tail = 0;
    circle_buff->inner_alloc = true;

    circle_buff_prepare_empty(circle_buff);

    return circle_buff;
}

lv_circle_buff_t * lv_circle_buff_create_from_buff(void * buff, const uint32_t capacity, const uint32_t element_size)
{
    LV_ASSERT_NULL(buff);

    lv_circle_buff_t * circle_buff = lv_malloc(sizeof(lv_circle_buff_t));
    LV_ASSERT_MALLOC(circle_buff);

    if(circle_buff == NULL) {
        return NULL;
    }

    lv_array_init_from_buff(&circle_buff->array, buff, capacity, element_size);
    circle_buff->head = 0;
    circle_buff->tail = 0;
    circle_buff->inner_alloc = false;

    circle_buff_prepare_empty(circle_buff);

    return circle_buff;
}

lv_circle_buff_t * lv_circle_buff_create_from_array(const lv_array_t * array)
{
    LV_ASSERT_NULL(array);
    if(array == NULL) {
        return NULL;
    }

    lv_circle_buff_t * circle_buff = lv_malloc(sizeof(lv_circle_buff_t));
    LV_ASSERT_MALLOC(circle_buff);

    if(circle_buff == NULL) {
        return NULL;
    }

    circle_buff->array = *array;
    circle_buff->head = 0;
    circle_buff->tail = 0;
    circle_buff->inner_alloc = false;

    circle_buff_prepare_empty(circle_buff);

    return circle_buff;
}
lv_result_t lv_circle_buff_resize(lv_circle_buff_t * circle_buff, const uint32_t capacity)
{
    if(lv_array_resize(&circle_buff->array, capacity) == false) {
        return LV_RESULT_INVALID;
    }

    circle_buff->head = 0;
    circle_buff->tail = 0;

    circle_buff_prepare_empty(circle_buff);

    return LV_RESULT_OK;
}

void lv_circle_buff_destroy(lv_circle_buff_t * circle_buff)
{
    lv_array_deinit(&circle_buff->array);

    lv_free(circle_buff);
}

uint32_t lv_circle_buff_size(const lv_circle_buff_t * circle_buff)
{
    return circle_buff->tail - circle_buff->head;
}

uint32_t lv_circle_buff_capacity(const lv_circle_buff_t * circle_buff)
{
    return lv_array_capacity(&circle_buff->array);
}

uint32_t lv_circle_buff_remain(const lv_circle_buff_t * circle_buff)
{
    return lv_circle_buff_capacity(circle_buff) - lv_circle_buff_size(circle_buff);
}

bool lv_circle_buff_is_empty(const lv_circle_buff_t * circle_buff)
{
    return !lv_circle_buff_size(circle_buff);
}

bool lv_circle_buff_is_full(const lv_circle_buff_t * circle_buff)
{
    return !lv_circle_buff_remain(circle_buff);
}

void lv_circle_buff_reset(lv_circle_buff_t * circle_buff)
{
    lv_array_clear(&circle_buff->array);
    circle_buff->head = 0;
    circle_buff->tail = 0;
}

void * lv_circle_buff_head(const lv_circle_buff_t * circle_buff)
{
    return lv_array_at(&circle_buff->array,
                       circle_buff->head % lv_circle_buff_capacity(circle_buff));
}

void * lv_circle_buff_tail(const lv_circle_buff_t * circle_buff)
{
    return lv_array_at(&circle_buff->array,
                       circle_buff->tail % lv_circle_buff_capacity(circle_buff));
}

lv_result_t lv_circle_buff_read(lv_circle_buff_t * circle_buff, void * data)
{
    if(lv_circle_buff_is_empty(circle_buff)) {
        circle_buff->head = 0;
        circle_buff->tail = 0;
        return LV_RESULT_INVALID;
    }

    lv_circle_buff_peek_at(circle_buff, 0, data);
    circle_buff->head++;

    return LV_RESULT_OK;
}

lv_result_t lv_circle_buff_write(lv_circle_buff_t * circle_buff, const void * data)
{
    if(lv_circle_buff_is_full(circle_buff)) {
        return LV_RESULT_INVALID;
    }

    lv_array_assign(&circle_buff->array, circle_buff->tail % lv_circle_buff_capacity(circle_buff), data);
    circle_buff->tail++;

    return LV_RESULT_OK;
}

uint32_t lv_circle_buff_fill(lv_circle_buff_t * circle_buff, uint32_t count, lv_circle_buff_fill_cb_t fill_cb,
                             void * user_data)
{
    uint32_t filled = 0;
    while(count > 0 && !lv_circle_buff_is_full(circle_buff)) {
        void * data = lv_circle_buff_tail(circle_buff);
        if(fill_cb(data, circle_buff->array.element_size, (int32_t)filled, user_data) == LV_RESULT_OK) {
            circle_buff->tail++;
            filled++;
        }
        else break;

        count--;
    }

    return filled;
}

lv_result_t lv_circle_buff_skip(lv_circle_buff_t * circle_buff)
{
    if(lv_circle_buff_is_empty(circle_buff)) {
        circle_buff->head = 0;
        circle_buff->tail = 0;
        return LV_RESULT_INVALID;
    }

    circle_buff->head++;

    return LV_RESULT_OK;
}

lv_result_t lv_circle_buff_peek(const lv_circle_buff_t * circle_buff, void * data)
{
    return lv_circle_buff_peek_at(circle_buff, 0, data);
}

lv_result_t lv_circle_buff_peek_at(const lv_circle_buff_t * circle_buff, const uint32_t index, void * data)
{
    const uint32_t real_index = (index % lv_circle_buff_size(circle_buff) + circle_buff->head) % lv_circle_buff_capacity(
                                    circle_buff);
    lv_memcpy(data, lv_array_at(&circle_buff->array, real_index), circle_buff->array.element_size);

    return LV_RESULT_OK;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void circle_buff_prepare_empty(lv_circle_buff_t * circle_buff)
{
    const uint32_t required = lv_array_capacity(&circle_buff->array) - lv_array_size(&circle_buff->array);
    for(uint32_t i = 0; i < required; i++) lv_array_push_back(&circle_buff->array, NULL);
}
