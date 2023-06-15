/**
* @file lv_pack.c
*/

/*********************
 *      INCLUDES
 *********************/

#include "lv_pack.h"
#include "stdarg.h"
#include "lv_log.h"
#include "lv_types.h"
#include "stddef.h"

//#if LV_USE_PACK != 0

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

void lv_pack_yaml_write_pack_begin(lv_pack_t * pack);
void lv_pack_yaml_write_pack_end(lv_pack_t * pack);
void lv_pack_yaml_write_dict_begin(lv_pack_t * pack, const char * key);
void lv_pack_yaml_write_dict_end(lv_pack_t * pack);
void lv_pack_yaml_write_key_pair_begin(lv_pack_t * pack, const char * key);
void lv_pack_yaml_write_key_pair_end(lv_pack_t * pack);
void lv_pack_yaml_write_array_begin(lv_pack_t * pack);
void lv_pack_yaml_write_array_end(lv_pack_t * pack);
void lv_pack_yaml_write_array_num_inline(lv_pack_t * pack, uint32_t num_cnt, ...);
void lv_pack_yaml_write_str(lv_pack_t * pack, const char * str);
void lv_pack_yaml_write_num(lv_pack_t * pack, int32_t num);
void lv_pack_yaml_write_ptr(lv_pack_t * pack, const void * ptr);
void lv_pack_yaml_write_bool(lv_pack_t * pack, bool b);
void lv_pack_yaml_write_null(lv_pack_t * pack);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_pack_t lv_pack_yaml = {
    .depth = 0,
    .write = lv_log,
    .write_pack_begin = lv_pack_yaml_write_pack_begin,
    .write_pack_end = lv_pack_yaml_write_pack_end,
    .write_dict_begin = lv_pack_yaml_write_dict_begin,
    .write_dict_end = lv_pack_yaml_write_dict_end,
    .write_key_pair_begin = lv_pack_yaml_write_key_pair_begin,
    .write_key_pair_end = lv_pack_yaml_write_key_pair_end,
    .write_array_begin = lv_pack_yaml_write_array_begin,
    .write_array_end = lv_pack_yaml_write_array_end,
    .write_array_num_inline = lv_pack_yaml_write_array_num_inline,
    .write_str = lv_pack_yaml_write_str,
    .write_num = lv_pack_yaml_write_num,
    .write_ptr = lv_pack_yaml_write_ptr,
    .write_bool = lv_pack_yaml_write_bool,
    .write_null = lv_pack_yaml_write_null,
};

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
lv_pack_t  * lv_pack_get(lv_pack_type_t type)
{
    switch(type) {
        case LV_PACK_YAML:
            return &lv_pack_yaml;
        default:
            LV_LOG_WARN("lv_pack_get: unknown type");
            return NULL;
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void lv_obj_dump_tree_print_intend(lv_pack_t * pack, const char * splitter)
{
    /*print depth splitter*/
    for(int i = 0; i < pack->depth; ++i) {
        pack->write("%s", splitter);
    }
}

void lv_pack_yaml_write_pack_begin(lv_pack_t * pack)
{
    pack->depth = 0;
    pack->write("---\n");
}

void lv_pack_yaml_write_pack_end(lv_pack_t * pack)
{
    pack->depth = 0;
    pack->write("...\n");
}

void lv_pack_yaml_write_dict_begin(lv_pack_t * pack, const char * key)
{
    lv_obj_dump_tree_print_intend(pack, "  ");
    pack->write("%s: \n", key);
    pack->depth++;
}
void lv_pack_yaml_write_dict_end(lv_pack_t * pack)
{
    pack->depth--;
}
void lv_pack_yaml_write_key_pair_begin(lv_pack_t * pack, const char * key)
{
    lv_obj_dump_tree_print_intend(pack, "  ");
    pack->write("%s: ", key);
}
void lv_pack_yaml_write_key_pair_end(lv_pack_t * pack)
{
    pack->write("\n");
}
void lv_pack_yaml_write_array_begin(lv_pack_t * pack)
{
    lv_obj_dump_tree_print_intend(pack, "  ");
    pack->write("- \n");
    pack->depth++;
}
void lv_pack_yaml_write_array_end(lv_pack_t * pack)
{
    pack->depth--;
}
void lv_pack_yaml_write_array_num_inline(lv_pack_t * pack, uint32_t num_cnt, ...)
{
    va_list args;
    va_start(args, num_cnt);
    pack->write("[");
    for(uint32_t i = 0; i < num_cnt; ++i) {
        int32_t num = va_arg(args, int32_t);
        pack->write("%d", num);
        if(i != num_cnt - 1) {
            pack->write(", ");
        }
    }
    pack->write("]");
    va_end(args);
}
void lv_pack_yaml_write_str(lv_pack_t * pack, const char * str)
{
    char * p = (char *)str;
    // transform all escape characters
    while(*p != '\0') {
        if(*p == '\n') {
            pack->write("\\n");
        }
        else if(*p == '\r') {
            pack->write("\\r");
        }
        else if(*p == '\t') {
            pack->write("\\t");
        }
        else if(*p == '\\') {
            pack->write("\\\\");
        }
        else if(*p == '\"') {
            pack->write("\\\"");
        }
        else if(*p == '\'') {
            pack->write("''");
        }
        else {
            pack->write("%c", *p);
        }
        p++;
    }
}
void lv_pack_yaml_write_num(lv_pack_t * pack, int32_t num)
{
    pack->write("%d", num);
}
void lv_pack_yaml_write_ptr(lv_pack_t * pack, const void * ptr)
{
    pack->write("%p", ptr);
}
void lv_pack_yaml_write_bool(lv_pack_t * pack, bool b)
{
    pack->write("%s", b ? "true" : "false");
}
void lv_pack_yaml_write_null(lv_pack_t * pack)
{
    pack->write("null");
}

//#endif /*LV_USE_PACK != 0*/
