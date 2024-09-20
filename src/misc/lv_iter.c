/**
 * @file lv_iter.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_iter.h"

#include "lv_array.h"
#include "lv_circle_buff.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

struct _lv_iter_t {
    /* Iterator state */
    void  *  instance;        /**< Pointer to the object to iterate over */
    uint32_t elem_size;       /**< Size of one element in bytes */
    void  *  context;         /**< Custom context for the iteration */
    uint32_t context_size;    /**< Size of the custom context in bytes */

    /* Peeking */
    lv_circle_buff_t * peek_buff; /**< Circular buffer for peeking */
    uint32_t peek_offset;         /**< Offset in the peek buffer */

    /* Callbacks */
    lv_iter_next_cb next_cb;  /**< Callback to get the next element */
};

/**********************
 *  STATIC PROTOTYPES
 **********************/

static bool peek_fill_cb(void * buff, uint32_t buff_len, int32_t index, void * user_data);

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

lv_iter_t * lv_iter_create(void * instance, uint32_t elem_size, uint32_t context_size, lv_iter_next_cb next_cb)
{
    lv_iter_t * iter = lv_malloc_zeroed(sizeof(lv_iter_t));
    LV_ASSERT_MALLOC(iter);

    if(iter == NULL) {
        LV_LOG_ERROR("Could not allocate memory for iterator");
        return NULL;
    }

    iter->instance = instance;
    iter->elem_size = elem_size;
    iter->context_size = context_size;
    iter->next_cb = next_cb;

    if(context_size > 0) {
        iter->context = lv_malloc_zeroed(context_size);
        LV_ASSERT_MALLOC(iter->context);
    }

    return iter;
}

void * lv_iter_get_context(lv_iter_t * iter)
{
    return iter->context;
}

void lv_iter_destroy(lv_iter_t * iter)
{
    if(iter->context_size > 0) lv_free(iter->context);
    if(iter->peek_buff != NULL) lv_circle_buff_destroy(iter->peek_buff);

    lv_free(iter);
}

void lv_iter_make_peekable(lv_iter_t * iter, uint32_t capacity)
{
    if(capacity == 0 || iter->peek_buff != NULL) return;
    iter->peek_buff = lv_circle_buff_create(capacity, iter->elem_size);
    LV_ASSERT_NULL(iter->peek_buff);
}

lv_result_t lv_iter_next(lv_iter_t * iter, void * elem)
{
    lv_circle_buff_t * cbuff = iter->peek_buff;
    if(cbuff != NULL && !lv_circle_buff_is_empty(cbuff)) {
        if(elem) lv_circle_buff_read(cbuff, elem);
        else lv_circle_buff_skip(cbuff);
        iter->peek_offset = 0;
        return LV_RESULT_OK;
    }

    const lv_result_t iter_res = iter->next_cb(iter->instance, iter->context, elem);
    if(iter_res == LV_RESULT_INVALID) return LV_RESULT_INVALID;

    if(cbuff != NULL) iter->peek_offset = 0;

    return iter_res;
}

lv_result_t lv_iter_peek(lv_iter_t * iter, void * elem)
{
    lv_circle_buff_t * cbuff = iter->peek_buff;
    if(cbuff == NULL) return LV_RESULT_INVALID;

    const uint32_t peek_count = lv_circle_buff_size(cbuff);
    if(iter->peek_offset >= peek_count) {
        const uint32_t required = iter->peek_offset + 1 - peek_count;
        const uint32_t filled = lv_circle_buff_fill(cbuff, required, peek_fill_cb, iter);
        if(filled != required) return LV_RESULT_INVALID;
    }

    lv_circle_buff_peek_at(cbuff, iter->peek_offset, elem);

    return LV_RESULT_OK;
}

lv_result_t lv_iter_peek_advance(lv_iter_t * iter)
{
    if(iter->peek_buff == NULL || iter->peek_offset + 1 >= lv_circle_buff_capacity(iter->peek_buff))
        return LV_RESULT_INVALID;
    iter->peek_offset++;
    return LV_RESULT_OK;
}

lv_result_t lv_iter_peek_reset(lv_iter_t * iter)
{
    if(iter->peek_buff == NULL) return LV_RESULT_INVALID;

    iter->peek_offset = 0;
    return LV_RESULT_OK;
}

void lv_iter_inspect(lv_iter_t * iter, lv_iter_inspect_cb inspect_cb)
{
    void * elem = lv_malloc_zeroed(iter->elem_size);
    LV_ASSERT_MALLOC(elem);

    if(elem == NULL) {
        LV_LOG_ERROR("Could not allocate memory for element");
        return;
    }

    while(lv_iter_next(iter, elem) == LV_RESULT_OK) {
        inspect_cb(elem);
    }

    lv_free(elem);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static bool peek_fill_cb(void * buff, uint32_t buff_len, int32_t index, void * user_data)
{
    LV_UNUSED(buff_len);
    LV_UNUSED(index);

    const lv_iter_t * iter = user_data;
    const lv_result_t iter_res = iter->next_cb(iter->instance, iter->context, buff);
    if(iter_res == LV_RESULT_INVALID) return false;

    return true;
}
