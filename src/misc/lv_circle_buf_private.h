/**
 * @file lv_circle_buf_private.h
 *
 */

#ifndef LV_CIRCLE_BUF_PRIVATE_H
#define LV_CIRCLE_BUF_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "lv_circle_buf.h"
#include "lv_array.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

struct _lv_circle_buf_t {
    lv_array_t array;
    uint32_t head;
    uint32_t tail;    /**< The next write position */

    bool inner_alloc; /**< true: the array is allocated by the buffer, false: the array is created from an external buffer */
    bool is_static; /**< true: the circle buffer is static, false: the circle buffer is dynamic */
};

/**********************
* GLOBAL PROTOTYPES
**********************/

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_CIRCLE_BUF_PRIVATE_H*/
