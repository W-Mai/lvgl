/**
 * @file lv_arc_label.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_arc_label_private.h"

#if LV_USE_ARC_LABEL != 0

#include "../../core/lv_obj_class_private.h"
#include "../../core/lv_obj_event_private.h"
#include "../../core/lv_obj_private.h"
#include "../../misc/lv_area_private.h"
#include "../../misc/lv_assert.h"
#include "../../misc/lv_text_private.h"

/*********************
 *      DEFINES
 *********************/

#define MY_CLASS (&lv_arc_label_class)

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void lv_arc_label_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void arc_label_draw_main(lv_event_t * e);
static void lv_arc_label_event(const lv_obj_class_t * class_p, lv_event_t * e);
static void inv_arc_area(lv_obj_t * arc, lv_value_precise_t start_angle, lv_value_precise_t end_angle, lv_part_t part);
static void inv_knob_area(lv_obj_t * obj);
static void get_center(const lv_obj_t * obj, lv_point_t * center, int32_t * arc_r);
static lv_value_precise_t get_angle(const lv_obj_t * obj);

/**********************
 *  STATIC VARIABLES
 **********************/
const lv_obj_class_t lv_arc_label_class  = {
    .constructor_cb = lv_arc_label_constructor,
    .event_cb = lv_arc_label_event,
    .instance_size = sizeof(lv_arc_label_t),
    .editable = LV_OBJ_CLASS_EDITABLE_TRUE,
    .base_class = &lv_obj_class,
    .name = "arc_label",
};

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * lv_arc_label_create(lv_obj_t * parent)
{
    LV_LOG_INFO("begin");
    lv_obj_t * obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

/*======================
 * Add/remove functions
 *=====================*/

/*
 * New object specific "add" or "remove" functions come here
 */

/*=====================
 * Setter functions
 *====================*/

void lv_arc_label_set_text(lv_obj_t * obj, const char * text)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_arc_label_t * arc_label = (lv_arc_label_t *)obj;

    /*If text is NULL then just refresh with the current text*/
    if(text == NULL) text = arc_label->text;

    const size_t text_len = lv_strlen(text) + 1;

    /*If set its own text then reallocate it (maybe its size changed)*/
    if(arc_label->text == text && arc_label->static_txt == 0) {
        arc_label->text = lv_realloc(arc_label->text, text_len);
        LV_ASSERT_MALLOC(label->text);
        if(arc_label->text == NULL) return;
    }
    else {
        /*Free the old text*/
        if(arc_label->text != NULL && arc_label->static_txt == 0) {
            lv_free(arc_label->text);
            arc_label->text = NULL;
        }

        arc_label->text = lv_malloc(text_len);
        LV_ASSERT_MALLOC(label->text);
        if(arc_label->text == NULL) return;

        lv_strcpy(arc_label->text, text);

        /*Now the text is dynamically allocated*/
        arc_label->static_txt = 0;
    }

    // lv_arc_label_refr_text(obj);
}

void lv_arc_label_set_text_fmt(lv_obj_t * obj, const char * fmt, ...)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    LV_ASSERT_NULL(fmt);

    lv_obj_invalidate(obj);
    lv_arc_label_t * arc_label = (lv_arc_label_t *)obj;

    /*If text is NULL then refresh*/
    if(fmt == NULL) {
        // lv_arc_label_refr_text(obj);
        return;
    }

    if(arc_label->text != NULL && arc_label->static_txt == 0) {
        lv_free(arc_label->text);
        arc_label->text = NULL;
    }

    va_list args;
    va_start(args, fmt);
    arc_label->text = lv_text_set_text_vfmt(fmt, args);
    va_end(args);
    arc_label->static_txt = 0; /*Now the text is dynamically allocated*/

    // lv_arc_label_refr_text(obj);
}

void lv_arc_label_set_text_static(lv_obj_t * obj, const char * text)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_arc_label_t * arc_label = (lv_arc_label_t *)obj;

    if(arc_label->static_txt == 0 && arc_label->text != NULL) {
        lv_free(arc_label->text);
        arc_label->text = NULL;
    }

    if(text != NULL) {
        arc_label->static_txt = 1;
        arc_label->text       = (char *)text;
    }

    // lv_arc_label_refr_text(obj);
}

void lv_arc_label_set_angle_start(lv_obj_t * obj, lv_value_precise_t start)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_arc_label_t * arc = (lv_arc_label_t *)obj;

    arc->angle_start = start;
}

void lv_arc_label_set_angle_size(lv_obj_t * obj, lv_value_precise_t size)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_arc_label_t * arc = (lv_arc_label_t *)obj;

    arc->angle_size = size;
}

void lv_arc_label_set_offset(lv_obj_t * obj, int32_t offset)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_arc_label_t * arc = (lv_arc_label_t *)obj;

    arc->offset = offset;
}

void lv_arc_label_set_dir(lv_obj_t * obj, lv_arc_label_dir_t dir)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_arc_label_t * arc = (lv_arc_label_t *)obj;

    arc->dir = dir;
}

void lv_arc_label_set_recolor(lv_obj_t * obj, bool en)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_arc_label_t * arc = (lv_arc_label_t *)obj;
    arc->recolor = en;
}

void lv_arc_label_set_radius(lv_obj_t * obj, uint32_t radius)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_arc_label_t * arc = (lv_arc_label_t *)obj;

    arc->radius = radius;
}

/*=====================
 * Getter functions
 *====================*/

lv_value_precise_t lv_arc_label_get_angle_start(lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    return ((lv_arc_label_t *) obj)->angle_start;
}

lv_value_precise_t lv_arc_label_get_angle_size(lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_arc_label_t * arc_label = (lv_arc_label_t *)obj;
    return arc_label->angle_size;
}

lv_arc_label_dir_t lv_arc_label_get_dir(const lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    return ((lv_arc_label_t *) obj)->dir;
}

/*=====================
 * Other functions
 *====================*/

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void lv_arc_label_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    LV_TRACE_OBJ_CREATE("begin");

    lv_arc_label_t * arc = (lv_arc_label_t *)obj;

    /*Initialize the allocated 'ext'*/
    arc->angle_start = 0;
    arc->angle_size  = 360;
    arc->dir = LV_ARC_LABEL_DIR_CLOCKWISE;
    arc->recolor = false;

    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_label_set_text(obj, LV_ARC_LABEL_DEFAULT_TEXT);

    // lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    // lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    // lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN | LV_OBJ_FLAG_SCROLLABLE);
    // lv_obj_set_ext_click_area(obj, LV_DPI_DEF / 10);

    LV_TRACE_OBJ_CREATE("finished");
}

static void lv_arc_label_event(const lv_obj_class_t * class_p, lv_event_t * e)
{
    LV_UNUSED(class_p);

    /*Call the ancestor's event handler*/
    const lv_result_t res = lv_obj_event_base(MY_CLASS, e);
    if(res != LV_RESULT_OK) return;

    const lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_current_target(e);

    if((code == LV_EVENT_STYLE_CHANGED) || (code == LV_EVENT_SIZE_CHANGED)) {
        // lv_label_refr_text(obj);
    }
    else if(code == LV_EVENT_REFR_EXT_DRAW_SIZE) {
        /* Italic or other non-typical letters can be drawn of out of the object.
         * It happens if box_w + ofs_x > adw_w in the glyph.
         * To avoid this add some extra draw area.
         * font_h / 4 is an empirical value. */
        const lv_font_t * font = lv_obj_get_style_text_font(obj, LV_PART_MAIN);
        const int32_t font_h = lv_font_get_line_height(font);
        lv_event_set_ext_draw_size(e, font_h / 4);
    }
    else if(code == LV_EVENT_GET_SELF_SIZE) {
        lv_arc_label_t * arc_label = (lv_arc_label_t *)obj;
        // if(label->invalid_size_cache) {
        //     const lv_font_t * font = lv_obj_get_style_text_font(obj, LV_PART_MAIN);
        //     int32_t letter_space = lv_obj_get_style_text_letter_space(obj, LV_PART_MAIN);
        //     int32_t line_space = lv_obj_get_style_text_line_space(obj, LV_PART_MAIN);
        //     lv_text_flag_t flag = LV_TEXT_FLAG_NONE;
        //     if(label->recolor != 0) flag |= LV_TEXT_FLAG_RECOLOR;
        //     if(label->expand != 0) flag |= LV_TEXT_FLAG_EXPAND;
        //
        //     int32_t w;
        //     if(lv_obj_get_style_width(obj, LV_PART_MAIN) == LV_SIZE_CONTENT && !obj->w_layout) w = LV_COORD_MAX;
        //     else w = lv_obj_get_content_width(obj);
        //     w = LV_MIN(w, lv_obj_get_style_max_width(obj, 0));
        //
        //     uint32_t dot_begin = label->dot_begin;
        //     lv_label_revert_dots(obj);
        //     lv_text_get_size(&label->size_cache, label->text, font, letter_space, line_space, w, flag);
        //     lv_label_set_dots(obj, dot_begin);
        //
        //     label->invalid_size_cache = false;
        // }

        lv_point_t * self_size = lv_event_get_param(e);
        // self_size->x = LV_MAX(self_size->x, label->size_cache.x);
        // self_size->y = LV_MAX(self_size->y, label->size_cache.y);
    }
    else if(code == LV_EVENT_DRAW_MAIN) {
        arc_label_draw_main(e);
    }
}

static void arc_label_draw_main(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_current_target(e);
    lv_arc_label_t * arc_label = (lv_arc_label_t *)obj;

    lv_area_t coords;
    lv_obj_get_content_coords(obj, &coords);

    int32_t w = lv_obj_get_width(obj);
    int32_t h = lv_obj_get_height(obj);
    int32_t ls = lv_obj_get_style_space_left(obj, LV_PART_MAIN);
    int32_t rs = lv_obj_get_style_space_right(obj, LV_PART_MAIN);
    int32_t ts = lv_obj_get_style_pad_top(obj, LV_PART_MAIN);
    int32_t bs = lv_obj_get_style_space_bottom(obj, LV_PART_MAIN);
    int32_t scroll_top = lv_obj_get_scroll_top(obj);
    int32_t scroll_left = lv_obj_get_scroll_left(obj);

    lv_layer_t * layer = lv_event_get_layer(e);

    int32_t arc_r = arc_label->radius;

    for(lv_value_precise_t angle_start = 0; angle_start < arc_label->angle_size; angle_start += 20) {
        lv_value_precise_t curr_angle = arc_label->angle_start + (arc_label->dir == LV_ARC_LABEL_DIR_CLOCKWISE ? angle_start :
                                                                  -angle_start);

        float sin_value = lv_trigo_sin(curr_angle) / 32767.0;
        float cos_value = lv_trigo_cos(curr_angle) / 32767.0;

        float x = cos_value * arc_r;
        float y = sin_value * arc_r;

        // lv_point_t point = {
        //     (int32_t)(x + (ls + w) / 2),
        //     (int32_t)(y + (ts + h) / 2)
        // };

        lv_point_t point = {
            (int32_t)(x + (ls + w) / 2 + coords.x1),
            (int32_t)(y + (ts + h) / 2 + coords.y1),
        };

        lv_draw_label_dsc_t dsc;
        lv_draw_label_dsc_init(&dsc);
        dsc.font = lv_obj_get_style_text_font(obj, LV_PART_MAIN);

        dsc.color = lv_color_make(0x11, 0x45, 0x14);
        dsc.rotation = curr_angle * 10 + 900;

        lv_area_t area = {
            .x1 = point.x - 10,
            .y1 = point.y - 10,
            .x2 = point.x,
            .y2 = point.y
        };

        lv_draw_character(layer, &dsc, &point, '6');
    }

}

static void inv_arc_area(lv_obj_t * obj, lv_value_precise_t start_angle, lv_value_precise_t end_angle, lv_part_t part)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    /*Skip this complicated invalidation if the arc is not visible*/
    if(lv_obj_is_visible(obj) == false) return;

    lv_arc_label_t * arc = (lv_arc_label_t *)obj;

    if(start_angle == end_angle) return;

    if(start_angle > 360) start_angle -= 360;
    if(end_angle > 360) end_angle -= 360;

    // start_angle += arc->rotation;
    // end_angle += arc->rotation;

    if(start_angle > 360) start_angle -= 360;
    if(end_angle > 360) end_angle -= 360;

    int32_t r;
    lv_point_t c;
    get_center(obj, &c, &r);

    int32_t w = lv_obj_get_style_arc_width(obj, part);
    int32_t rounded = lv_obj_get_style_arc_rounded(obj, part);

    lv_area_t inv_area;
    lv_draw_arc_get_area(c.x, c.y, r, start_angle, end_angle, w, rounded, &inv_area);

    lv_obj_invalidate_area(obj, &inv_area);
}

static void inv_knob_area(lv_obj_t * obj)
{
    lv_point_t c;
    int32_t r;
    get_center(obj, &c, &r);

    lv_area_t a;
    // get_knob_area(obj, &c, r, &a);

    // int32_t knob_extra_size = knob_get_extra_size(obj);

    // if(knob_extra_size > 0) {
    // lv_area_increase(&a, knob_extra_size, knob_extra_size);
    // }

    lv_obj_invalidate_area(obj, &a);
}

static void get_center(const lv_obj_t * obj, lv_point_t * center, int32_t * arc_r)
{
    int32_t left_bg = lv_obj_get_style_pad_left(obj, LV_PART_MAIN);
    int32_t right_bg = lv_obj_get_style_pad_right(obj, LV_PART_MAIN);
    int32_t top_bg = lv_obj_get_style_pad_top(obj, LV_PART_MAIN);
    int32_t bottom_bg = lv_obj_get_style_pad_bottom(obj, LV_PART_MAIN);

    int32_t r = (LV_MIN(lv_obj_get_width(obj) - left_bg - right_bg,
                        lv_obj_get_height(obj) - top_bg - bottom_bg)) / 2;

    center->x = obj->coords.x1 + r + left_bg;
    center->y = obj->coords.y1 + r + top_bg;

    if(arc_r) *arc_r = r;
}

static lv_value_precise_t get_angle(const lv_obj_t * obj)
{
    lv_arc_label_t * arc = (lv_arc_label_t *)obj;
    // lv_value_precise_t angle = arc->rotation;
    // if(arc->type == LV_ARC_LABEL_DIR_CLOCKWISE) {
    //     angle += arc->angle_size;
    // }
    // else if(arc->type == LV_ARC_LABEL_DIR_COUNTER_CLOCKWISE) {
    //     angle += arc->angle_start;
    // }
    // else if(arc->type == LV_ARC_LABEL_MODE_SYMMETRICAL) {
    //     lv_value_precise_t bg_end = arc->bg_angle_end;
    //     if(arc->bg_angle_end < arc->bg_angle_start) bg_end = arc->bg_angle_end + 360;
    //     lv_value_precise_t indic_end = arc->angle_size;
    //     if(arc->angle_size < arc->angle_start) indic_end = arc->angle_size + 360;
    //
    //     lv_value_precise_t angle_midpoint = (int32_t)(arc->bg_angle_start + bg_end) / 2;
    //     if(arc->angle_start < angle_midpoint) angle += arc->angle_start;
    //     else if(indic_end > angle_midpoint) angle += arc->angle_size;
    //     else angle += angle_midpoint;
    // }

    // return angle;
}

static void get_knob_area(lv_obj_t * obj, const lv_point_t * center, int32_t r, lv_area_t * knob_area)
{
    // int32_t indic_width = lv_obj_get_style_arc_width(obj, LV_PART_INDICATOR);
    // int32_t indic_width_half = indic_width / 2;
    // r -= indic_width_half;
    //
    // int32_t angle = (int32_t)get_angle(obj);
    // int32_t knob_offset = lv_arc_label_get_knob_offset(obj);
    // int32_t knob_x = (r * lv_trigo_sin(knob_offset + angle + 90)) >> LV_TRIGO_SHIFT;
    // int32_t knob_y = (r * lv_trigo_sin(knob_offset + angle)) >> LV_TRIGO_SHIFT;
    //
    // int32_t left_knob = lv_obj_get_style_pad_left(obj, LV_PART_KNOB);
    // int32_t right_knob = lv_obj_get_style_pad_right(obj, LV_PART_KNOB);
    // int32_t top_knob = lv_obj_get_style_pad_top(obj, LV_PART_KNOB);
    // int32_t bottom_knob = lv_obj_get_style_pad_bottom(obj, LV_PART_KNOB);
    //
    // knob_area->x1 = center->x + knob_x - left_knob - indic_width_half;
    // knob_area->x2 = center->x + knob_x + right_knob + indic_width_half;
    // knob_area->y1 = center->y + knob_y - top_knob - indic_width_half;
    // knob_area->y2 = center->y + knob_y + bottom_knob + indic_width_half;
}

/**
 * Used internally to update arc angles after a value change
 * @param arc pointer to an arc object
 */
static void value_update(lv_obj_t * obj)
{
    // LV_ASSERT_OBJ(obj, MY_CLASS);
    // lv_arc_label_t * arc = (lv_arc_label_t *)obj;
    //
    // /*If the value is still not set to any value do not update*/
    // if(arc->value == VALUE_UNSET) return;
    //
    // lv_value_precise_t bg_midpoint, bg_end = arc->bg_angle_end;
    // int32_t range_midpoint;
    // if(arc->bg_angle_end < arc->bg_angle_start) bg_end = arc->bg_angle_end + 360;
    //
    // int32_t angle;
    // switch(arc->type) {
    //     case LV_ARC_LABEL_MODE_SYMMETRICAL:
    //         bg_midpoint = (arc->bg_angle_start + bg_end) / 2;
    //         range_midpoint = (int32_t)(arc->min_value + arc->max_value) / 2;
    //
    //         if(arc->value < range_midpoint) {
    //             angle = lv_map(arc->value, arc->min_value, range_midpoint, (int32_t)arc->bg_angle_start, (int32_t)bg_midpoint);
    //             lv_arc_label_set_angle_start(obj, angle);
    //             lv_arc_label_set_angle_end(obj, bg_midpoint);
    //         }
    //         else {
    //             angle = lv_map(arc->value, range_midpoint, arc->max_value, (int32_t)bg_midpoint, (int32_t)bg_end);
    //             lv_arc_label_set_angle_start(obj, bg_midpoint);
    //             lv_arc_label_set_angle_end(obj, angle);
    //         }
    //         break;
    //     case LV_ARC_LABEL_DIR_COUNTER_CLOCKWISE:
    //         angle = lv_map(arc->value, arc->min_value, arc->max_value, (int32_t)bg_end, (int32_t)arc->bg_angle_start);
    //         lv_arc_label_set_angles(obj, angle, arc->bg_angle_end);
    //         break;
    //     case LV_ARC_LABEL_DIR_CLOCKWISE:
    //         angle = lv_map(arc->value, arc->min_value, arc->max_value, (int32_t)arc->bg_angle_start, (int32_t)bg_end);
    //         lv_arc_label_set_angles(obj, arc->bg_angle_start, angle);
    //
    //         break;
    //     default:
    //         LV_LOG_WARN("Invalid mode: %d", arc->type);
    //         return;
    // }
    // arc->last_angle = angle; /*Cache angle for slew rate limiting*/
}

static int32_t knob_get_extra_size(lv_obj_t * obj)
{
    int32_t knob_shadow_size = 0;
    knob_shadow_size += lv_obj_get_style_shadow_width(obj, LV_PART_KNOB);
    knob_shadow_size += lv_obj_get_style_shadow_spread(obj, LV_PART_KNOB);
    knob_shadow_size += LV_ABS(lv_obj_get_style_shadow_offset_x(obj, LV_PART_KNOB));
    knob_shadow_size += LV_ABS(lv_obj_get_style_shadow_offset_y(obj, LV_PART_KNOB));

    int32_t knob_outline_size = 0;
    knob_outline_size += lv_obj_get_style_outline_width(obj, LV_PART_KNOB);
    knob_outline_size += lv_obj_get_style_outline_pad(obj, LV_PART_KNOB);

    return LV_MAX(knob_shadow_size, knob_outline_size);
}

/**
 * Check if angle is within arc background bounds
 *
 * In order to avoid unexpected value update of the arc value when the user clicks
 * outside of the arc background we need to check if the angle (of the clicked point)
 * is within the bounds of the background.
 *
 * A tolerance (extra room) also should be taken into consideration.
 *
 * E.g. Arc with start angle of 0° and end angle of 90°, the background is only visible in
 * that range, from 90° to 360° the background is invisible. Click in 150° should not update
 * the arc value, click within the arc angle range should.
 *
 * IMPORTANT NOTE: angle is always relative to bg_angle_start, e.g. if bg_angle_start is 30
 * and we click a bit to the left, angle is 10, not the expected 40.
 *
 * @param obj   Pointer to lv_arc_label
 * @param angle Angle to be checked. Is 0<=angle<=360 and relative to bg_angle_start
 * @param tolerance_deg Tolerance
 *
 * @return true if angle is within arc background bounds, false otherwise
 */
static bool lv_arc_label_angle_within_bg_bounds(lv_obj_t * obj, const lv_value_precise_t angle,
                                                const lv_value_precise_t tolerance_deg)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    // lv_arc_label_t * arc = (lv_arc_label_t *)obj;
    //
    // lv_value_precise_t bounds_angle = arc->bg_angle_end - arc->bg_angle_start;
    //
    // /* ensure the angle is in the range [0, 360) */
    // while(bounds_angle < 0) bounds_angle += 360;
    // while(bounds_angle >= 360) bounds_angle -= 360;
    //
    // /* Angle is in the bounds */
    // if(angle <= bounds_angle) {
    //     if(angle < (bounds_angle / 2)) {
    //         arc->min_close = CLICK_CLOSER_TO_MIN_END;
    //     }
    //     else {
    //         arc->min_close = CLICK_CLOSER_TO_MAX_END;
    //     }
    //     arc->in_out = CLICK_INSIDE_BG_ANGLES;
    //     return true;
    // }
    //
    // /* Distance between background start and end angles is less than tolerance,
    //  * consider the click inside the arc */
    // if(360 - bounds_angle <= tolerance_deg) {
    //     arc->min_close = CLICK_CLOSER_TO_MIN_END;
    //     arc->in_out = CLICK_INSIDE_BG_ANGLES;
    //     return true;
    // }
    //
    // /* angle is within the tolerance of the min end */
    // if(360 - angle <= tolerance_deg) {
    //     arc->min_close = CLICK_CLOSER_TO_MIN_END;
    //     arc->in_out = CLICK_OUTSIDE_BG_ANGLES;
    //     return true;
    // }
    //
    // /* angle is within the tolerance of the max end */
    // if(angle <= bounds_angle + tolerance_deg) {
    //     arc->min_close = CLICK_CLOSER_TO_MAX_END;
    //     arc->in_out = CLICK_OUTSIDE_BG_ANGLES;
    //     return true;
    // }
    //
    // return false;
}

#endif
