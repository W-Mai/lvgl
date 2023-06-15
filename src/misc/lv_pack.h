/**
* @file lv_pack.h
*
*/

#ifndef LV_PACK_H
#define LV_PACK_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
*      INCLUDES
*********************/
#include "lv_area.h"

/*********************
*      DEFINES
*********************/

/**********************
*      TYPEDEFS
**********************/
typedef enum {
    LV_PACK_YAML,
} lv_pack_type_t;

typedef struct _lv_pack_t {
    lv_coord_t depth;

    void (*write)(const char * format, ...);

    void (*write_pack_begin)(struct _lv_pack_t * pack);
    void (*write_pack_end)(struct _lv_pack_t * pack);

    void (*write_dict_begin)(struct _lv_pack_t * pack, const char * key);
    void (*write_dict_end)(struct _lv_pack_t * pack);
    void (*write_key_pair_begin)(struct _lv_pack_t * pack, const char * key);
    void (*write_key_pair_end)(struct _lv_pack_t * pack);
    void (*write_array_begin)(struct _lv_pack_t * pack);
    void (*write_array_end)(struct _lv_pack_t * pack);
    void (*write_array_num_inline)(struct _lv_pack_t * pack, uint32_t num_cnt, ...);
    void (*write_str)(struct _lv_pack_t * pack, const char * str);
    void (*write_num)(struct _lv_pack_t * pack, int32_t num);
    void (*write_ptr)(struct _lv_pack_t * pack, const void * ptr);
    void (*write_bool)(struct _lv_pack_t * pack, bool b);
    void (*write_null)(struct _lv_pack_t * pack);
} lv_pack_t;
/**********************
* GLOBAL PROTOTYPES
**********************/

lv_pack_t  * lv_pack_get(lv_pack_type_t type);

/**********************
*      MACROS
**********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_MEMCPY_BUILTIN_H*/
