/**
* @file lv_circle_buff.h
*
 */


#ifndef LV_CIRCLE_BUFF_H
#define LV_CIRCLE_BUFF_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "lv_types.h"
#include "lv_assert.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

typedef bool (*lv_circle_buff_fill_cb_t)(void * buff, uint32_t buff_len, int32_t index, void * user_data);

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Create a circle buffer
 * @param capacity the maximum number of elements in the buffer
 * @param element_size the size of an element in bytes
 * @return pointer to the created buffer
 */
lv_circle_buff_t * lv_circle_buff_create(uint32_t capacity, uint32_t element_size);

/**
 * Create a circle buffer from an existing buffer
 * @param buff pointer to a buffer
 * @param capacity the maximum number of elements in the buffer
 * @param element_size the size of an element in bytes
 * @return pointer to the created buffer
 */
lv_circle_buff_t * lv_circle_buff_create_from_buff(void * buff, uint32_t capacity, uint32_t element_size);

/**
 * Create a circle buffer from an existing array
 * @param array pointer to an array
 * @return pointer to the created buffer
 */
lv_circle_buff_t * lv_circle_buff_create_from_array(const lv_array_t * array);

/**
 * Resize the buffer
 * @param circle_buff pointer to a buffer
 * @param capacity the new capacity of the buffer
 * @return LV_RESULT_OK: the buffer is resized; LV_RESULT_INVALID: the buffer is not resized
 */
lv_result_t lv_circle_buff_resize(lv_circle_buff_t * circle_buff, uint32_t capacity);

/**
 * Destroy a circle buffer
 * @param circle_buff pointer to buffer
 */
void lv_circle_buff_destroy(lv_circle_buff_t * circle_buff);

/**
 * Get the size of the buffer
 * @param circle_buff pointer to buffer
 * @return the number of elements in the buffer
 */
uint32_t lv_circle_buff_size(const lv_circle_buff_t * circle_buff);

/**
 * Get the capacity of the buffer
 * @param circle_buff pointer to buffer
 * @return the maximum number of elements in the buffer
 */
uint32_t lv_circle_buff_capacity(const lv_circle_buff_t * circle_buff);

/**
 * Get the remaining space in the buffer
 * @param circle_buff pointer to buffer
 * @return the number of elements that can be written to the buffer
 */
uint32_t lv_circle_buff_remain(const lv_circle_buff_t * circle_buff);

/**
 * Check if the buffer is empty
 * @param circle_buff pointer to buffer
 * @return true: the buffer is empty; false: the buffer is not empty
 */
bool lv_circle_buff_is_empty(const lv_circle_buff_t * circle_buff);

/**
 * Check if the buffer is full
 * @param circle_buff pointer to buffer
 * @return true: the buffer is full; false: the buffer is not full
 */
bool lv_circle_buff_is_full(const lv_circle_buff_t * circle_buff);

/**
 * Reset the buffer
 * @param circle_buff pointer to buffer
 * @return LV_RESULT_OK: the buffer is reset; LV_RESULT_INVALID: the buffer is not reset
 */
void lv_circle_buff_reset(lv_circle_buff_t * circle_buff);

/**
 * Get the head of the buffer
 * @param circle_buff pointer to buffer
 * @return pointer to the head of the buffer
 */
void * lv_circle_buff_head(const lv_circle_buff_t * circle_buff);

/**
 * Get the tail of the buffer
 * @param circle_buff pointer to buffer
 * @return pointer to the tail of the buffer
 */
void * lv_circle_buff_tail(const lv_circle_buff_t * circle_buff);

/**
 * Read a value
 * @param circle_buff pointer to buffer
 * @param data pointer to a variable to store the read value
 * @return LV_RESULT_OK: the value is read; LV_RESULT_INVALID: the value is not read
 */
lv_result_t lv_circle_buff_read(lv_circle_buff_t * circle_buff, void * data);

/**
 * Write a value
 * @param circle_buff pointer to buffer
 * @param data pointer to the value to write
 * @return LV_RESULT_OK: the value is written; LV_RESULT_INVALID: the value is not written
 */
lv_result_t lv_circle_buff_write(lv_circle_buff_t * circle_buff, const void * data);

/**
 * Fill the buffer with values
 * @param circle_buff pointer to buffer
 * @param count the number of values to fill
 * @param fill_cb the callback function to fill the buffer
 * @param user_data
 * @return the number of values filled
 */
uint32_t lv_circle_buff_fill(lv_circle_buff_t * circle_buff, uint32_t count, lv_circle_buff_fill_cb_t fill_cb,
                             void * user_data);

/**
 * Skip a value
 * @param circle_buff pointer to buffer
 * @return LV_RESULT_OK: the value is skipped; LV_RESULT_INVALID: the value is not skipped
 */
lv_result_t lv_circle_buff_skip(lv_circle_buff_t * circle_buff);

/**
 * Peek a value
 * @param circle_buff pointer to buffer
 * @param data pointer to a variable to store the peeked value
 * @return LV_RESULT_OK: the value is peeked; LV_RESULT_INVALID: the value is not peeked
 */
lv_result_t lv_circle_buff_peek(const lv_circle_buff_t * circle_buff, void * data);

/**
 * Peek a value at an index
 * @param circle_buff pointer to buffer
 * @param index the index of the value to peek， if the index is greater than the size of the buffer, it will return looply.
 * @param data pointer to a variable to store the peeked value
 * @return LV_RESULT_OK: the value is peeked; LV_RESULT_INVALID: the value is not peeked
 */
lv_result_t lv_circle_buff_peek_at(const lv_circle_buff_t * circle_buff, uint32_t index, void * data);

/*************************
 *    GLOBAL VARIABLES
 *************************/

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_CIRCLE_BUFF_H*/