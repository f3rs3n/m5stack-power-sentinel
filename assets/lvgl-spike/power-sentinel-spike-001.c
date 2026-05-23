#include "lvgl.h"

static lv_color_t C(uint32_t hex) { return lv_color_hex(hex); }

static void panel(lv_obj_t *obj, uint32_t bg, uint32_t border) {
    lv_obj_set_style_bg_color(obj, C(bg), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, C(border), 0);
    lv_obj_set_style_radius(obj, 12, 0);
    lv_obj_set_style_pad_all(obj, 7, 0);
    lv_obj_set_style_pad_gap(obj, 5, 0);
}

static lv_obj_t *label(lv_obj_t *parent, const char *txt, uint32_t color, const lv_font_t *font) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, C(color), 0);
    if (font) lv_obj_set_style_text_font(l, font, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
    return l;
}

static lv_obj_t *badge(lv_obj_t *parent, const char *txt, uint32_t bg) {
    lv_obj_t *b = lv_label_create(parent);
    lv_label_set_text(b, txt);
    lv_obj_set_style_text_color(b, C(0xffffff), 0);
    lv_obj_set_style_text_font(b, &lv_font_montserrat_12, 0);
    lv_obj_set_style_bg_color(b, C(bg), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(b, 10, 0);
    lv_obj_set_style_pad_hor(b, 8, 0);
    lv_obj_set_style_pad_ver(b, 4, 0);
    return b;
}

static void metric(lv_obj_t *parent, const char *name, const char *value) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *left = label(row, name, 0x93a4bd, &lv_font_montserrat_12);
    lv_obj_set_width(left, 108);
    lv_obj_t *right = label(row, value, 0xe8eefc, &lv_font_montserrat_14);
    lv_obj_set_style_text_align(right, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_width(right, 170);
}

static void bar(lv_obj_t *parent, int value, uint32_t color) {
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_set_width(bar, lv_pct(100));
    lv_obj_set_height(bar, 9);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, value, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, C(0x273142), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, C(color), LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 5, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 5, LV_PART_INDICATOR);
}

void create_ui(void) {
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, C(0x0b0f17), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 6, 0);
    lv_obj_set_style_pad_gap(screen, 5, 0);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *top = lv_obj_create(screen);
    lv_obj_set_width(top, lv_pct(100));
    lv_obj_set_height(top, 158);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_COLUMN);
    panel(top, 0x111827, 0x22c55e);

    lv_obj_t *header = lv_obj_create(top);
    lv_obj_remove_style_all(header);
    lv_obj_set_width(header, lv_pct(100));
    lv_obj_set_height(header, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *title = label(header, "POWER SENTINEL", 0xf8fbff, &lv_font_montserrat_20);
    lv_obj_set_width(title, 190);
    badge(header, "OK", 0x16a34a);

    label(top, "GRID ONLINE", 0xc8d0df, &lv_font_montserrat_14);
    metric(top, "battery / runtime", "100%   57m");
    bar(top, 100, 0x22c55e);
    metric(top, "load / input", "19%   232V");
    bar(top, 19, 0x3b82f6);

    lv_obj_t *pills = lv_obj_create(top);
    lv_obj_remove_style_all(pills);
    lv_obj_set_width(pills, lv_pct(100));
    lv_obj_set_height(pills, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(pills, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(pills, 5, 0);
    badge(pills, "NUT OK", 0x16a34a);
    badge(pills, "PVE OK", 0x16a34a);
    badge(pills, "HA OK", 0x16a34a);

    lv_obj_t *bottom = lv_obj_create(screen);
    lv_obj_set_width(bottom, lv_pct(100));
    lv_obj_set_flex_grow(bottom, 1);
    lv_obj_set_flex_flow(bottom, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(bottom, 6, 0);
    lv_obj_set_style_bg_opa(bottom, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bottom, 0, 0);
    lv_obj_set_style_pad_all(bottom, 0, 0);

    lv_obj_t *left = lv_obj_create(bottom);
    lv_obj_set_width(left, 150);
    lv_obj_set_height(left, lv_pct(100));
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
    panel(left, 0x171b24, 0x394152);
    label(left, "local", 0x93a4bd, &lv_font_montserrat_12);
    label(left, "NET OK   M5S OK", 0xe8eefc, &lv_font_montserrat_14);

    lv_obj_t *right = lv_obj_create(bottom);
    lv_obj_set_flex_grow(right, 1);
    lv_obj_set_height(right, lv_pct(100));
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
    panel(right, 0x171b24, 0x394152);
    label(right, "problems", 0x93a4bd, &lv_font_montserrat_12);
    label(right, "none", 0xe8eefc, &lv_font_montserrat_14);
}
