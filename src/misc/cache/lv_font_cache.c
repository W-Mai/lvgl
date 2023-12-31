/**
* @file lv_font_cache.c
*
 */

/*********************
 *      INCLUDES
 *********************/
#include "../lv_assert.h"
#include "lv_font_cache.h"
#include "../../core/lv_global.h"
/*********************
 *      DEFINES
 *********************/
#define font_cache_p (LV_GLOBAL_DEFAULT()->font_cache)
/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

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
bool lv_font_cache_init()
{
    font_cache_p = lv_cache_create(&lv_cache_class_lru_rb_count
                                   , sizeof(lv_font_cache_data_t),
                                   (lv_cache_create_cb_t)lv_font_cache_create_cb,
                                   (lv_cache_free_cb_t)lv_font_cache_free_cb,
                                   (lv_cache_compare_cb_t)lv_font_cache_compare_cb);
}
void lv_font_cache_deinit()
{

}

void lv_font_cache_drop(const lv_font_t * font, uint32_t unicode_letter, uint32_t size)
{

}
/**********************
 *   STATIC FUNCTIONS
 **********************/
