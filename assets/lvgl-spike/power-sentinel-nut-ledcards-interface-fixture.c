#include "lvgl.h"
#include "ps_font_ddin_condensed_bold_60.c"
#include "ps_font_ddin_condensed_bold_40.c"
#include "ps_icon_chart_32.c"

#ifndef PS_NUT_HOME_STATE
#define PS_NUT_HOME_STATE 0
#endif

#define PS_NUT_STATE_NOMINAL 0
#define PS_NUT_STATE_ON_BATTERY 1
#define PS_NUT_STATE_LOW_BATTERY 2
#define PS_NUT_STATE_STALE 3
#define PS_NUT_STATE_HIGH_LOAD 4
#define PS_NUT_STATE_INPUT_LOW 5

static lv_color_t C(uint32_t hex) { return lv_color_hex(hex); }

static void set_no_border(lv_obj_t *obj) {
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_outline_width(obj, 0, 0);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, int radius, uint32_t fill, uint32_t border) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_style_bg_color(o, C(fill), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, border ? 1 : 0, 0);
    if (border) lv_obj_set_style_border_color(o, C(border), 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_OFF);
    return o;
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y, int w, const lv_font_t *font, uint32_t color) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_width(l, w);
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, C(color), 0);
    lv_obj_set_style_bg_opa(l, LV_OPA_TRANSP, 0);
    return l;
}

static void top_status(lv_obj_t *screen, uint32_t batt_color, int batt_w) {
    // Small iconographic row: approximate Wi-Fi glyph + time, carousel dots, local/module battery.
    // Keep this visually quiet; it should not become a product header.
    box(screen, 6, 8, 2, 1, 0, 0x919792, 0);
    box(screen, 8, 7, 2, 1, 0, 0x919792, 0);
    box(screen, 10, 6, 2, 1, 0, 0x919792, 0);
    box(screen, 12, 7, 2, 1, 0, 0x919792, 0);
    box(screen, 14, 8, 2, 1, 0, 0x919792, 0);
    box(screen, 8, 11, 2, 1, 0, 0x919792, 0);
    box(screen, 10, 10, 2, 1, 0, 0x919792, 0);
    box(screen, 12, 11, 2, 1, 0, 0x919792, 0);
    box(screen, 10, 14, 3, 3, 2, 0x919792, 0);
    label(screen, "10:23", 24, 2, 46, &lv_font_montserrat_10, 0x919792);

    box(screen, 148, 8, 5, 5, 3, 0xf5f6f2, 0);
    box(screen, 160, 8, 4, 4, 2, 0x4d5450, 0);
    box(screen, 172, 8, 4, 4, 2, 0x4d5450, 0);

    lv_obj_t *bat = box(screen, 294, 5, 20, 9, 2, 0x040607, 0xb1b6b2);
    box(bat, 2, 2, batt_w, 5, 1, batt_color, 0);
    box(screen, 315, 8, 3, 3, 1, 0xb1b6b2, 0);
}

static void chart_button(lv_obj_t *screen) {
    lv_obj_t *hit = lv_obj_create(screen);
    lv_obj_remove_style_all(hit);
    lv_obj_set_pos(hit, 263, 56);
    lv_obj_set_size(hit, 34, 34);
    lv_obj_set_scrollbar_mode(hit, LV_SCROLLBAR_MODE_OFF);
    lv_obj_t *icon = lv_label_create(hit);
    lv_label_set_text(icon, "\xF3\xB1\x95\x8D");
    lv_obj_set_style_text_font(icon, &ps_icon_chart_32, 0);
    lv_obj_set_style_text_color(icon, C(0xd6dad6), 0);
    lv_obj_set_style_bg_opa(icon, LV_OPA_TRANSP, 0);
    lv_obj_center(icon);
}

static void tile(lv_obj_t *screen, int x, int y, const char *value, const char *name, const char *unit, uint32_t accent, uint32_t fill) {
    lv_obj_t *t = box(screen, x, y, 142, 46, 7, fill, 0x141e1b);
    box(t, 7, 8, 5, 28, 3, accent, 0);
    lv_obj_t *name_l = label(t, name, 76, 6, 55, &lv_font_montserrat_12, 0xc9d0c9);
    lv_obj_set_style_text_align(name_l, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_t *unit_l = label(t, unit, 78, 23, 53, &lv_font_montserrat_12, 0x87918c);
    lv_obj_set_style_text_align(unit_l, LV_TEXT_ALIGN_RIGHT, 0);
    // Keep the metric value visually on top of the right-side label/unit objects.
    // On the physical CoreS3, the transparent label objects can still obscure the
    // right edge of tight values such as TTE "06:24" more than the MCP render shows.
    label(t, value, 20, 8, 58, &ps_font_ddin_condensed_bold_40, 0xf5f6f2);
}

static void render_nut_home(lv_obj_t *screen) {
    lv_obj_set_style_bg_color(screen, C(0x040607), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    set_no_border(screen);

#if PS_NUT_HOME_STATE == PS_NUT_STATE_ON_BATTERY
    top_status(screen, 0xfcca3d, 10);
    const char *hero_value = "06:24";
    const char *hero_label = "TTE";
    const char *hero_unit = "mm:ss";
    const char *hero_state = "ON BATTERY";
    uint32_t hero_accent = 0xfcca3d;
    uint32_t state_color = 0xfcca3d;
    int label_x = 160;
#elif PS_NUT_HOME_STATE == PS_NUT_STATE_LOW_BATTERY
    top_status(screen, 0xff4e3e, 4);
    const char *hero_value = "18";
    const char *hero_label = "Battery";
    const char *hero_unit = "%";
    const char *hero_state = "LOW BATTERY";
    uint32_t hero_accent = 0xff4e3e;
    uint32_t state_color = 0xff6a57;
    int label_x = 96;
#elif PS_NUT_HOME_STATE == PS_NUT_STATE_STALE
    top_status(screen, 0xaab0ac, 8);
    const char *hero_value = "--";
    const char *hero_label = "NUT";
    const char *hero_unit = "stale";
    const char *hero_state = "STALE 42s";
    uint32_t hero_accent = 0x6c7470;
    uint32_t state_color = 0xc1c5c1;
    int label_x = 92;
#elif PS_NUT_HOME_STATE == PS_NUT_STATE_HIGH_LOAD
    top_status(screen, 0xff8a2a, 12);
    const char *hero_value = "86";
    const char *hero_label = "Load";
    const char *hero_unit = "%";
    const char *hero_state = "HIGH";
    uint32_t hero_accent = 0xff8a2a;
    uint32_t state_color = 0xffb064;
    int label_x = 100;
#elif PS_NUT_HOME_STATE == PS_NUT_STATE_INPUT_LOW
    top_status(screen, 0xfcca3d, 10);
    const char *hero_value = "185";
    const char *hero_label = "Input";
    const char *hero_unit = "V";
    const char *hero_state = "MARGINAL INPUT";
    uint32_t hero_accent = 0xfcca3d;
    uint32_t state_color = 0xfcca3d;
    int label_x = 124;
#else
    top_status(screen, 0xaab0ac, 14);
    const char *hero_value = "100";
    const char *hero_label = "Battery";
    const char *hero_unit = "%";
    const char *hero_state = "FULL";
    uint32_t hero_accent = 0x14dc78;
    uint32_t state_color = 0x9bb2a0;
    int label_x = 124;
#endif

    box(screen, 19, 33, 7, 79, 4, hero_accent, 0);
    label(screen, hero_value, 43, 33, 120, &ps_font_ddin_condensed_bold_60, 0xf5f6f2);
    label(screen, hero_label, label_x, 33, 72, &lv_font_montserrat_12, 0xbeb8a0);
    label(screen, hero_unit, label_x, 63, 72, &lv_font_montserrat_12, 0x968f78);
    label(screen, hero_state, 45, 94, 142, &lv_font_montserrat_14, state_color);
    chart_button(screen);

#if PS_NUT_HOME_STATE == PS_NUT_STATE_ON_BATTERY
    tile(screen, 12, 124, "72", "Battery", "%", 0xfcca3d, 0x221c08);
    tile(screen, 166, 124, "38", "Load", "%", 0x14dc78, 0x071c12);
    tile(screen, 12, 182, "0", "Input", "V", 0xff4e3e, 0x200d0c);
    tile(screen, 166, 182, "1", "NUT", "client", 0x1cb5f0, 0x07161d);
#elif PS_NUT_HOME_STATE == PS_NUT_STATE_LOW_BATTERY
    tile(screen, 12, 124, "06:24", "TTE", "mm:ss", 0xfcca3d, 0x221c08);
    tile(screen, 166, 124, "42", "Load", "%", 0x14dc78, 0x071c12);
    tile(screen, 12, 182, "0", "Input", "V", 0xff4e3e, 0x200d0c);
    tile(screen, 166, 182, "1", "NUT", "client", 0x1cb5f0, 0x07161d);
#elif PS_NUT_HOME_STATE == PS_NUT_STATE_STALE
    tile(screen, 12, 124, "--", "Battery", "%", 0x6c7470, 0x101514);
    tile(screen, 166, 124, "--", "TTE", "mm:ss", 0x6c7470, 0x101514);
    tile(screen, 12, 182, "--", "Load", "%", 0x6c7470, 0x101514);
    tile(screen, 166, 182, "--", "Input", "V", 0x6c7470, 0x101514);
#elif PS_NUT_HOME_STATE == PS_NUT_STATE_HIGH_LOAD
    tile(screen, 12, 124, "92", "Battery", "%", 0x14dc78, 0x071c12);
    tile(screen, 166, 124, "42", "TTE", "m", 0x1cb5f0, 0x07161d);
    tile(screen, 12, 182, "226", "Input", "V", 0x14dc78, 0x071e14);
    tile(screen, 166, 182, "1", "NUT", "client", 0x1cb5f0, 0x07161d);
#elif PS_NUT_HOME_STATE == PS_NUT_STATE_INPUT_LOW
    tile(screen, 12, 124, "88", "Battery", "%", 0x14dc78, 0x071c12);
    tile(screen, 166, 124, "51", "TTE", "m", 0x1cb5f0, 0x07161d);
    tile(screen, 12, 182, "24", "Load", "%", 0x14dc78, 0x071c12);
    tile(screen, 166, 182, "1", "NUT", "client", 0x1cb5f0, 0x07161d);
#else
    tile(screen, 12, 124, "57", "TTE", "m", 0x1cb5f0, 0x07161d);
    tile(screen, 166, 124, "18", "Load", "%", 0x14dc78, 0x071c12);
    tile(screen, 12, 182, "226", "Input", "V", 0x14dc78, 0x071e14);
    tile(screen, 166, 182, "1", "NUT", "client", 0x1cb5f0, 0x07161d);
#endif
}

void create_ui(void) {
    render_nut_home(lv_screen_active());
}
