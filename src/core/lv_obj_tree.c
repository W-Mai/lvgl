/**
 * @file lv_obj_tree.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include <stdlib.h>

#include "lv_obj.h"
#include "lv_indev.h"
#include "lv_indev_private.h"
#include "lv_disp.h"
#include "lv_disp_private.h"
#include "../misc/lv_anim.h"
#include "../misc/lv_gc.h"
#include "../misc/lv_async.h"

/*********************
 *      DEFINES
 *********************/
#define MY_CLASS &lv_obj_class

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lv_obj_del_async_cb(void * obj);
static void obj_del_core(lv_obj_t * obj);
static lv_obj_tree_walk_res_t walk_core(lv_obj_t * obj, lv_obj_tree_walk_cbs_t * cbs, lv_coord_t depth,
                                        void * user_data);
static lv_obj_tree_walk_res_t lv_obj_dump_tree_cb(lv_obj_t * obj, lv_coord_t depth, void * user_data);
static lv_obj_tree_walk_res_t lv_obj_dump_tree_children_pre_cb(lv_obj_t * obj, lv_coord_t depth, void * user_data);
static lv_obj_tree_walk_res_t lv_obj_dump_tree_children_post_cb(lv_obj_t * obj, lv_coord_t depth, void * user_data);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_obj_del(lv_obj_t * obj)
{
    LV_LOG_TRACE("begin (delete %p)", (void *)obj);
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_obj_invalidate(obj);

    lv_obj_t * par = lv_obj_get_parent(obj);
    if(par) {
        lv_obj_scrollbar_invalidate(par);
    }

    lv_disp_t * disp = NULL;
    bool act_scr_del = false;
    if(par == NULL) {
        disp = lv_obj_get_disp(obj);
        if(!disp) return;   /*Shouldn't happen*/
        if(disp->act_scr == obj) act_scr_del = true;
    }

    obj_del_core(obj);

    /*Call the ancestor's event handler to the parent to notify it about the child delete*/
    if(par) {
        lv_obj_update_layout(par);
        lv_obj_readjust_scroll(par, LV_ANIM_OFF);
        lv_obj_scrollbar_invalidate(par);
        lv_obj_send_event(par, LV_EVENT_CHILD_CHANGED, NULL);
        lv_obj_send_event(par, LV_EVENT_CHILD_DELETED, NULL);
    }

    /*Handle if the active screen was deleted*/
    if(act_scr_del) {
        LV_LOG_WARN("the active screen was deleted");
        disp->act_scr = NULL;
    }

    LV_ASSERT_MEM_INTEGRITY();
    LV_LOG_TRACE("finished (delete %p)", (void *)obj);
}

void lv_obj_clean(lv_obj_t * obj)
{
    LV_LOG_TRACE("begin (delete %p)", (void *)obj);
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_obj_invalidate(obj);

    lv_obj_t * child = lv_obj_get_child(obj, 0);
    while(child) {
        obj_del_core(child);
        child = lv_obj_get_child(obj, 0);
    }
    /*Just to remove scroll animations if any*/
    lv_obj_scroll_to(obj, 0, 0, LV_ANIM_OFF);
    if(obj->spec_attr) {
        obj->spec_attr->scroll.x = 0;
        obj->spec_attr->scroll.y = 0;
    }

    LV_ASSERT_MEM_INTEGRITY();

    LV_LOG_TRACE("finished (delete %p)", (void *)obj);
}

void lv_obj_del_delayed(lv_obj_t * obj, uint32_t delay_ms)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, NULL);
    lv_anim_set_time(&a, 1);
    lv_anim_set_delay(&a, delay_ms);
    lv_anim_set_ready_cb(&a, lv_obj_del_anim_ready_cb);
    lv_anim_start(&a);
}

void lv_obj_del_anim_ready_cb(lv_anim_t * a)
{
    lv_obj_del(a->var);
}

void lv_obj_del_async(lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_async_call(lv_obj_del_async_cb, obj);
}

void lv_obj_set_parent(lv_obj_t * obj, lv_obj_t * parent)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    LV_ASSERT_OBJ(parent, MY_CLASS);

    if(obj->parent == NULL) {
        LV_LOG_WARN("Can't set the parent of a screen");
        return;
    }

    if(parent == NULL) {
        LV_LOG_WARN("Can't set parent == NULL to an object");
        return;
    }

    lv_obj_invalidate(obj);

    lv_obj_allocate_spec_attr(parent);

    lv_obj_t * old_parent = obj->parent;
    /*Remove the object from the old parent's child list*/
    int32_t i;
    for(i = lv_obj_get_index(obj); i <= (int32_t)lv_obj_get_child_cnt(old_parent) - 2; i++) {
        old_parent->spec_attr->children[i] = old_parent->spec_attr->children[i + 1];
    }
    old_parent->spec_attr->child_cnt--;
    if(old_parent->spec_attr->child_cnt) {
        old_parent->spec_attr->children = lv_realloc(old_parent->spec_attr->children,
                                                     old_parent->spec_attr->child_cnt * (sizeof(lv_obj_t *)));
    }
    else {
        lv_free(old_parent->spec_attr->children);
        old_parent->spec_attr->children = NULL;
    }

    /*Add the child to the new parent as the last (newest child)*/
    parent->spec_attr->child_cnt++;
    parent->spec_attr->children = lv_realloc(parent->spec_attr->children,
                                             parent->spec_attr->child_cnt * (sizeof(lv_obj_t *)));
    parent->spec_attr->children[lv_obj_get_child_cnt(parent) - 1] = obj;

    obj->parent = parent;

    /*Notify the original parent because one of its children is lost*/
    lv_obj_readjust_scroll(old_parent, LV_ANIM_OFF);
    lv_obj_scrollbar_invalidate(old_parent);
    lv_obj_send_event(old_parent, LV_EVENT_CHILD_CHANGED, obj);
    lv_obj_send_event(old_parent, LV_EVENT_CHILD_DELETED, NULL);

    /*Notify the new parent about the child*/
    lv_obj_send_event(parent, LV_EVENT_CHILD_CHANGED, obj);
    lv_obj_send_event(parent, LV_EVENT_CHILD_CREATED, NULL);

    lv_obj_mark_layout_as_dirty(obj);

    lv_obj_invalidate(obj);
}

void lv_obj_move_to_index(lv_obj_t * obj, int32_t index)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_obj_t * parent = lv_obj_get_parent(obj);

    if(!parent) {
        LV_LOG_WARN("parent is NULL");
        return;
    }

    if(index < 0) {
        index = lv_obj_get_child_cnt(parent) + index;
    }

    const int32_t old_index = lv_obj_get_index(obj);

    if(index < 0) return;
    if(index >= (int32_t) lv_obj_get_child_cnt(parent)) return;
    if(index == old_index) return;

    int32_t i = old_index;
    if(index < old_index) {
        while(i > index)  {
            parent->spec_attr->children[i] = parent->spec_attr->children[i - 1];
            i--;
        }
    }
    else {
        while(i < index) {
            parent->spec_attr->children[i] = parent->spec_attr->children[i + 1];
            i++;
        }
    }

    parent->spec_attr->children[index] = obj;
    lv_obj_send_event(parent, LV_EVENT_CHILD_CHANGED, NULL);
    lv_obj_invalidate(parent);
}

void lv_obj_swap(lv_obj_t * obj1, lv_obj_t * obj2)
{
    LV_ASSERT_OBJ(obj1, MY_CLASS);
    LV_ASSERT_OBJ(obj2, MY_CLASS);

    lv_obj_t * parent = lv_obj_get_parent(obj1);
    lv_obj_t * parent2 = lv_obj_get_parent(obj2);

    uint_fast32_t index1 = lv_obj_get_index(obj1);
    uint_fast32_t index2 = lv_obj_get_index(obj2);

    lv_obj_send_event(parent2, LV_EVENT_CHILD_DELETED, obj2);
    lv_obj_send_event(parent, LV_EVENT_CHILD_DELETED, obj1);

    parent->spec_attr->children[index1] = obj2;
    obj2->parent = parent;

    parent2->spec_attr->children[index2] = obj1;
    obj1->parent = parent2;

    lv_obj_send_event(parent, LV_EVENT_CHILD_CHANGED, obj2);
    lv_obj_send_event(parent, LV_EVENT_CHILD_CREATED, obj2);
    lv_obj_send_event(parent2, LV_EVENT_CHILD_CHANGED, obj1);
    lv_obj_send_event(parent2, LV_EVENT_CHILD_CREATED, obj1);

    lv_obj_invalidate(parent);

    if(parent != parent2) {
        lv_obj_invalidate(parent2);
    }
    lv_group_swap_obj(obj1, obj2);
}

lv_obj_t * lv_obj_get_screen(const lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    const lv_obj_t * par = obj;
    const lv_obj_t * act_par;

    do {
        act_par = par;
        par = lv_obj_get_parent(act_par);
    } while(par != NULL);

    return (lv_obj_t *)act_par;
}

lv_disp_t * lv_obj_get_disp(const lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    const lv_obj_t * scr;

    if(obj->parent == NULL) scr = obj;  /*`obj` is a screen*/
    else scr = lv_obj_get_screen(obj);  /*get the screen of `obj`*/

    lv_disp_t * d;
    _LV_LL_READ(&LV_GC_ROOT(_lv_disp_ll), d) {
        uint32_t i;
        for(i = 0; i < d->screen_cnt; i++) {
            if(d->screens[i] == scr) return d;
        }
    }

    LV_LOG_WARN("No screen found");
    return NULL;
}

lv_obj_t * lv_obj_get_parent(const lv_obj_t * obj)
{
    if(obj == NULL) return NULL;
    LV_ASSERT_OBJ(obj, MY_CLASS);

    return obj->parent;
}

lv_obj_t * lv_obj_get_child(const lv_obj_t * obj, int32_t id)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    if(obj->spec_attr == NULL) return NULL;

    uint32_t idu;
    if(id < 0) {
        id = obj->spec_attr->child_cnt + id;
        if(id < 0) return NULL;
        idu = (uint32_t) id;
    }
    else {
        idu = id;
    }

    if(idu >= obj->spec_attr->child_cnt) return NULL;
    else return obj->spec_attr->children[id];
}

uint32_t lv_obj_get_child_cnt(const lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    if(obj->spec_attr == NULL) return 0;
    return obj->spec_attr->child_cnt;
}

uint32_t lv_obj_get_index(const lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_obj_t * parent = lv_obj_get_parent(obj);
    if(parent == NULL) return 0xFFFFFFFF;

    uint32_t i = 0;
    for(i = 0; i < lv_obj_get_child_cnt(parent); i++) {
        if(lv_obj_get_child(parent, i) == obj) return i;
    }

    return 0xFFFFFFFF; /*Shouldn't happen*/
}

void lv_obj_tree_walk(lv_obj_t * start_obj, lv_obj_tree_walk_cbs_t * cbs, void * user_data)
{
    lv_coord_t start_depth = start_obj == NULL ? 0 : 0;

    walk_core(start_obj, cbs, start_depth, user_data);
}

void lv_obj_dump_tree(lv_obj_t * start_obj)
{
    lv_obj_tree_walk_cbs_t cbs = {
        .walking = lv_obj_dump_tree_cb,
        .walk_child_pre = lv_obj_dump_tree_children_pre_cb,
        .walk_child_post = lv_obj_dump_tree_children_post_cb,
    };

    lv_pack_t pack = {
        .depth = 0,
        .write = lv_log,

        .write_dict_begin = lv_pack_write_dict_begin,
        .write_dict_end =  lv_pack_write_dict_end,
        .write_key_pair_begin =  lv_pack_write_key_pair_begin,
        .write_key_pair_end =  lv_pack_write_key_pair_end,
        .write_array_begin = lv_pack_write_array_begin,
        .write_array_end =    lv_pack_write_array_end,
        .write_array_num_inline =  lv_pack_write_array_num_inline,
        .write_str =    lv_pack_write_str,
        .write_num =    lv_pack_write_num,
        .write_ptr =    lv_pack_write_ptr,
        .write_bool =   lv_pack_write_bool,
        .write_null =   lv_pack_write_null,
    };

    lv_log("---\n");
    lv_obj_tree_walk(start_obj, &cbs, &pack);
    lv_log("...\n");
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void lv_obj_del_async_cb(void * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_obj_del(obj);
}

static void obj_del_core(lv_obj_t * obj)
{
    /*Let the user free the resources used in `LV_EVENT_DELETE`*/
    lv_res_t res = lv_obj_send_event(obj, LV_EVENT_DELETE, NULL);
    if(res == LV_RES_INV) return;

    /*Recursively delete the children*/
    lv_obj_t * child = lv_obj_get_child(obj, 0);
    while(child) {
        obj_del_core(child);
        child = lv_obj_get_child(obj, 0);
    }

    lv_group_t * group = lv_obj_get_group(obj);

    /*Reset all input devices if the object to delete is used*/
    lv_indev_t * indev = lv_indev_get_next(NULL);
    while(indev) {
        lv_indev_type_t indev_type = lv_indev_get_type(indev);
        if(indev_type == LV_INDEV_TYPE_POINTER || indev_type == LV_INDEV_TYPE_BUTTON) {
            if(indev->pointer.act_obj == obj || indev->pointer.last_obj == obj) {
                lv_indev_reset(indev, obj);
            }
            if(indev->pointer.last_pressed == obj) {
                indev->pointer.last_pressed = NULL;
            }
        }

        if(indev->group == group && obj == lv_indev_get_obj_act()) {
            lv_indev_reset(indev, obj);
        }
        indev = lv_indev_get_next(indev);
    }

    /*Delete all pending async del-s*/
    lv_res_t async_cancel_res = LV_RES_OK;
    while(async_cancel_res == LV_RES_OK) {
        async_cancel_res = lv_async_call_cancel(lv_obj_del_async_cb, obj);
    }

    /*All children deleted. Now clean up the object specific data*/
    _lv_obj_destruct(obj);

    /*Remove the screen for the screen list*/
    if(obj->parent == NULL) {
        lv_disp_t * disp = lv_obj_get_disp(obj);
        uint32_t i;
        /*Find the screen in the list*/
        for(i = 0; i < disp->screen_cnt; i++) {
            if(disp->screens[i] == obj) break;
        }

        uint32_t id = i;
        for(i = id; i < disp->screen_cnt - 1; i++) {
            disp->screens[i] = disp->screens[i + 1];
        }
        disp->screen_cnt--;
        disp->screens = lv_realloc(disp->screens, disp->screen_cnt * sizeof(lv_obj_t *));
    }
    /*Remove the object from the child list of its parent*/
    else {
        uint32_t id = lv_obj_get_index(obj);
        uint32_t i;
        for(i = id; i < obj->parent->spec_attr->child_cnt - 1; i++) {
            obj->parent->spec_attr->children[i] = obj->parent->spec_attr->children[i + 1];
        }
        obj->parent->spec_attr->child_cnt--;
        obj->parent->spec_attr->children = lv_realloc(obj->parent->spec_attr->children,
                                                      obj->parent->spec_attr->child_cnt * sizeof(lv_obj_t *));
    }

    /*Free the object itself*/
    lv_free(obj);
}


static lv_obj_tree_walk_res_t walk_core(lv_obj_t * obj, lv_obj_tree_walk_cbs_t * cbs, lv_coord_t depth,
                                        void * user_data)
{
    lv_obj_tree_walk_res_t res = LV_OBJ_TREE_WALK_NEXT;

    if(obj == NULL) {
        lv_disp_t * disp = lv_disp_get_next(NULL);
        while(disp) {
            uint32_t i;
            if(cbs->walk_child_pre) cbs->walk_child_pre(obj, depth, user_data);
            for(i = 0; i < disp->screen_cnt; i++) {
                walk_core(disp->screens[i], cbs, depth + 1, user_data);
            }
            if(cbs->walk_child_post) cbs->walk_child_post(obj, depth, user_data);
            disp = lv_disp_get_next(disp);
        }
        return LV_OBJ_TREE_WALK_END;    /*The value doesn't matter as it wasn't called recursively*/
    }

    res = cbs->walking(obj, depth, user_data);

    if(res == LV_OBJ_TREE_WALK_END) return LV_OBJ_TREE_WALK_END;

    if(res != LV_OBJ_TREE_WALK_SKIP_CHILDREN) {
        uint32_t i;
        if(cbs->walk_child_pre) cbs->walk_child_pre(obj, depth, user_data);
        for(i = 0; i < lv_obj_get_child_cnt(obj); i++) {
            res = walk_core(lv_obj_get_child(obj, i), cbs, depth + 1, user_data);
            if(res == LV_OBJ_TREE_WALK_END) return LV_OBJ_TREE_WALK_END;
        }
        if(cbs->walk_child_post) cbs->walk_child_post(obj, depth, user_data);
    }
    return LV_OBJ_TREE_WALK_NEXT;
}

static void lv_obj_dump_tree_print_intend(lv_coord_t depth, const char * splitter)
{
    /*print depth splitter*/
    for(int i = 0; i < depth; ++i) {
        lv_log("%s", splitter);
    }
}

void lv_pack_write_dict_begin(lv_pack_t * pack, const char * key)
{
    lv_obj_dump_tree_print_intend(pack->depth, "  ");
    pack->write("%s: \n", key);
    pack->depth++;
}
void lv_pack_write_dict_end(lv_pack_t * pack)
{
    pack->depth--;
}
void lv_pack_write_key_pair_begin(lv_pack_t * pack, const char * key)
{
    lv_obj_dump_tree_print_intend(pack->depth, "  ");
    pack->write("%s: ", key);
}
void lv_pack_write_key_pair_end(lv_pack_t * pack)
{
    pack->write("\n");
}
void lv_pack_write_array_begin(lv_pack_t * pack)
{
    lv_obj_dump_tree_print_intend(pack->depth, "  ");
    pack->write("- \n");
    pack->depth++;
}
void lv_pack_write_array_end(lv_pack_t * pack)
{
    pack->depth--;
}
void lv_pack_write_array_num_inline(lv_pack_t * pack, uint32_t num_cnt, ...)
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
void lv_pack_write_str(lv_pack_t * pack, const char * str)
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
void lv_pack_write_num(lv_pack_t * pack, int32_t num)
{
    pack->write("%d", num);
}
void lv_pack_write_ptr(lv_pack_t * pack, const void * ptr)
{
    pack->write("%p", ptr);
}
void lv_pack_write_bool(lv_pack_t * pack, bool b)
{
    pack->write("%s", b ? "true" : "false");
}
void lv_pack_write_null(lv_pack_t * pack)
{
    pack->write("null");
}

static lv_obj_tree_walk_res_t lv_obj_dump_tree_cb(lv_obj_t * obj, lv_coord_t depth, void * user_data)
{
    lv_pack_t * pack = (lv_pack_t *)user_data;

    pack->write_key_pair_begin(pack, "ptr");
    pack->write_ptr(pack, obj);
    pack->write_key_pair_end(pack);

    if(obj->class_p->class_name) {
        pack->write_key_pair_begin(pack, "type");
        pack->write_str(pack, obj->class_p->class_name);
        pack->write_key_pair_end(pack);
    }
    else {
        pack->write_key_pair_begin(pack, "type");
        pack->write_str(pack, "unknown");
        pack->write_key_pair_end(pack);
    }

    pack->write_key_pair_begin(pack, "area");
    pack->write_array_num_inline(pack, 4, obj->coords.x1, obj->coords.y1, obj->coords.x2, obj->coords.y2);
    pack->write_key_pair_end(pack);

    pack->write_key_pair_begin(pack, "size");
    pack->write_array_num_inline(pack, 2, lv_obj_get_width(obj), lv_obj_get_height(obj));
    pack->write_key_pair_end(pack);

    pack->write_key_pair_begin(pack, "opa");
    pack->write_num(pack, lv_obj_get_style_opa(obj, LV_PART_MAIN));
    pack->write_key_pair_end(pack);

    pack->write_key_pair_begin(pack, "bg_color");
    lv_color_t bg_color = lv_obj_get_style_bg_color(obj, LV_PART_MAIN);
    pack->write_array_num_inline(pack, 3, bg_color.red, bg_color.green, bg_color.blue);
    pack->write_key_pair_end(pack);


    pack->write_key_pair_begin(pack, "bg_opa");
    pack->write_num(pack, lv_obj_get_style_bg_opa(obj, LV_PART_MAIN));
    pack->write_key_pair_end(pack);

    pack->write_key_pair_begin(pack, "border_color");
    lv_color_t border_color = lv_obj_get_style_border_color(obj, LV_PART_MAIN);
    pack->write_array_num_inline(pack, 3, border_color.red, border_color.green, border_color.blue);
    pack->write_key_pair_end(pack);

    pack->write_key_pair_begin(pack, "border_opa");
    pack->write_num(pack, lv_obj_get_style_border_opa(obj, LV_PART_MAIN));
    pack->write_key_pair_end(pack);

    pack->write_key_pair_begin(pack, "border_width");
    pack->write_num(pack, lv_obj_get_style_border_width(obj, LV_PART_MAIN));
    pack->write_key_pair_end(pack);

    pack->write_key_pair_begin(pack, "border_side");
    pack->write_num(pack, lv_obj_get_style_border_side(obj, LV_PART_MAIN));
    pack->write_key_pair_end(pack);

    pack->write_key_pair_begin(pack, "radius");
    pack->write_num(pack, lv_obj_get_style_radius(obj, LV_PART_MAIN));
    pack->write_key_pair_end(pack);


    pack->write_key_pair_begin(pack, "padding");
    lv_area_t padding = {
        .x1 = lv_obj_get_style_pad_left(obj, LV_PART_MAIN),
        .y1 = lv_obj_get_style_pad_top(obj, LV_PART_MAIN),
        .x2 = lv_obj_get_style_pad_right(obj, LV_PART_MAIN),
        .y2 = lv_obj_get_style_pad_bottom(obj, LV_PART_MAIN),
    };
    pack->write_array_num_inline(pack, 4, padding.x1, padding.y1, padding.x2, padding.y2);
    pack->write_key_pair_end(pack);


    pack->write_key_pair_begin(pack, "margin");
    lv_area_t margin = {
        .x1 = lv_obj_get_style_margin_left(obj, LV_PART_MAIN),
        .y1 = lv_obj_get_style_margin_top(obj, LV_PART_MAIN),
        .x2 = lv_obj_get_style_margin_right(obj, LV_PART_MAIN),
        .y2 = lv_obj_get_style_margin_bottom(obj, LV_PART_MAIN),
    };
    pack->write_array_num_inline(pack, 4, margin.x1, margin.y1, margin.x2, margin.y2);
    pack->write_key_pair_end(pack);

    pack->write_key_pair_begin(pack, "align");
    pack->write_num(pack, lv_obj_get_style_align(obj, LV_PART_MAIN));
    pack->write_key_pair_end(pack);

    pack->write_key_pair_begin(pack, "offset");
    lv_point_t offset = {
        .x = lv_obj_get_style_translate_x(obj, LV_PART_MAIN),
        .y = lv_obj_get_style_translate_y(obj, LV_PART_MAIN),
    };
    pack->write_array_num_inline(pack, 2, offset.x, offset.y);
    pack->write_key_pair_end(pack);


    lv_obj_send_event(obj, LV_EVENT_DUMP_OBJ_CUSTOM, user_data);

    pack->write_dict_begin(pack, "children");
    return LV_OBJ_TREE_WALK_NEXT;
}

static lv_obj_tree_walk_res_t lv_obj_dump_tree_children_pre_cb(lv_obj_t * obj, lv_coord_t depth, void * user_data)
{
    lv_pack_t * pack = (lv_pack_t *)user_data;

    if(obj == NULL) {
        pack->write_array_begin(pack);

        pack->write_key_pair_begin(pack, "ptr");
        pack->write_ptr(pack, NULL);
        pack->write_key_pair_end(pack);

        pack->write_key_pair_begin(pack, "type");
        pack->write_str(pack, "lv_disp");
        pack->write_key_pair_end(pack);

        pack->write_dict_begin(pack, "children");
    }

    pack->write_array_begin(pack);
    return LV_OBJ_TREE_WALK_NEXT;
}

static lv_obj_tree_walk_res_t lv_obj_dump_tree_children_post_cb(lv_obj_t * obj, lv_coord_t depth, void * user_data)
{
    lv_pack_t * pack = (lv_pack_t *)user_data;
    pack->write_dict_end(pack);

    if(obj == NULL) {
        pack->write_array_end(pack);
    }
    return LV_OBJ_TREE_WALK_NEXT;
}
