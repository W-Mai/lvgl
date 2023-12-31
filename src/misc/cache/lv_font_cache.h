/**
* @file lv_font_cache.h
*
 */

#ifndef LV_FONT_CACHE_H
#define LV_FONT_CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "lv_cache_private.h"
#include "../../font/lv_font.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/
bool lv_font_cache_init();
void lv_font_cache_deinit();

lv_font_t * lv_font_cache_acquire(const void * src, uint32_t size);
lv_font_t * lv_font_cache_add(const void * src, uint32_t size);
void lv_font_cache_release(lv_font_t * font);
void lv_font_cache_drop(const void * src);
/*************************
 *    GLOBAL VARIABLES
 *************************/

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_FONT_CACHE_H*/
