#include "lvgl.h"
#include "ps_font_ddin_condensed_bold_60.c"
#include "ps_font_ddin_condensed_bold_40.c"
#include "ps_icon_chart_32.c"
#include "ps_icon_status_14.c"

#include <string.h>

#ifndef PS_PROXMOX_STATE
#define PS_PROXMOX_STATE 0
#endif

#define PS_PROXMOX_STATE_NOMINAL 0
#define PS_PROXMOX_STATE_STORAGE_WARN 1
#define PS_PROXMOX_STATE_GUEST_CRITICAL 2
#define PS_PROXMOX_STATE_UNCONFIGURED 3

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
    lv_label_set_text(l, text ? text : "");
    lv_obj_set_pos(l, x, y);
    lv_obj_set_width(l, w);
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, C(color), 0);
    lv_obj_set_style_bg_opa(l, LV_OPA_TRANSP, 0);
    return l;
}

static lv_obj_t *icon_label(lv_obj_t *parent, const char *text, int x, int y, int w, uint32_t color) {
    return label(parent, text, x, y, w, &ps_icon_status_14, color);
}

static void top_status(lv_obj_t *screen, int lan, int wifi, int link, const char *time_text, const char *battery_icon) {
    const uint32_t on = 0xaab0ac;
    const uint32_t off = 0x66706b;
    icon_label(screen, lan ? "\xF3\xB0\x8C\x98" : "\xF3\xB0\x8C\x99", 6, 2, 16, lan ? on : off);
    icon_label(screen, wifi ? "\xF3\xB0\x96\xA9" : "\xF3\xB0\x96\xAA", 22, 2, 16, wifi ? on : off);
    icon_label(screen, link ? "\xF3\xB0\x8C\xB9" : "\xF3\xB0\x8C\xBA", 38, 2, 16, link ? on : off);
    box(screen, 153, 8, 4, 4, 3, 0x4d5450, 0);
    box(screen, 163, 8, 5, 5, 3, 0xf5f6f2, 0);
    lv_obj_t *time = label(screen, time_text, 234, 2, 48, &lv_font_montserrat_12, on);
    lv_obj_set_style_text_align(time, LV_TEXT_ALIGN_RIGHT, 0);
    icon_label(screen, battery_icon, 292, 2, 20, on);
}

static uint32_t state_fill(uint32_t accent) {
    if (accent == 0xff4e3e) return 0x200d0c;
    if (accent == 0xff8a2a) return 0x241307;
    if (accent == 0xfcca3d) return 0x221c08;
    if (accent == 0x1cb5f0) return 0x07161d;
    if (accent == 0x9b5cff) return 0x180f26;
    if (accent == 0x6c7470) return 0x101514;
    return 0x071c12;
}

static uint32_t state_text(uint32_t accent) {
    if (accent == 0xff4e3e) return 0xff6a57;
    if (accent == 0xff8a2a) return 0xffb064;
    if (accent == 0xfcca3d) return 0xfcca3d;
    if (accent == 0x1cb5f0) return 0x1cb5f0;
    if (accent == 0x9b5cff) return 0xc4b5fd;
    if (accent == 0x6c7470) return 0xc1c5c1;
    return 0x9bb2a0;
}

static void chart_icon(lv_obj_t *parent, int x, int y) {
    lv_obj_t *hit = lv_obj_create(parent);
    lv_obj_remove_style_all(hit);
    lv_obj_set_pos(hit, x, y);
    lv_obj_set_size(hit, 34, 34);
    lv_obj_set_scrollbar_mode(hit, LV_SCROLLBAR_MODE_OFF);
    lv_obj_t *icon = lv_label_create(hit);
    lv_label_set_text(icon, "\xF3\xB1\x95\x8D");
    lv_obj_set_style_text_font(icon, &ps_icon_chart_32, 0);
    lv_obj_set_style_text_color(icon, C(0xd6dad6), 0);
    lv_obj_set_style_bg_opa(icon, LV_OPA_TRANSP, 0);
    lv_obj_center(icon);
}

static void hero_card(lv_obj_t *parent, const char *value, const char *label_text, const char *unit, const char *state, uint32_t accent) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_pos(card, 12, 25);
    lv_obj_set_size(card, 296, 91);
    lv_obj_set_style_bg_opa(card, LV_OPA_TRANSP, 0);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);

    int label_x = 84;
    size_t len = value ? strlen(value) : 0;
    if (len >= 5) label_x = 148;
    else if (len >= 3) label_x = 112;

    box(card, 7, 8, 7, 76, 4, accent, 0);
    label(card, value, 31, 8, 120, &ps_font_ddin_condensed_bold_60, 0xf5f6f2);
    label(card, label_text, label_x, 8, 72, &lv_font_montserrat_12, 0xbeb8a0);
    label(card, unit, label_x, 38, 72, &lv_font_montserrat_12, 0x968f78);
    label(card, state, 33, 71, 142, &lv_font_montserrat_14, state_text(accent));
    chart_icon(card, 262, 8);
}

static void tile(lv_obj_t *screen, int x, int y, const char *value, const char *name, const char *unit, const char *state, uint32_t accent) {
    lv_obj_t *t = box(screen, x, y, 142, 46, 7, state_fill(accent), 0x141e1b);
    box(t, 7, 8, 5, 28, 3, accent, 0);
    lv_obj_t *name_l = label(t, name, 76, 6, 55, &lv_font_montserrat_12, 0xc9d0c9);
    lv_obj_set_style_text_align(name_l, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_t *unit_l = label(t, unit, 78, 23, 53, &lv_font_montserrat_12, 0x87918c);
    lv_obj_set_style_text_align(unit_l, LV_TEXT_ALIGN_RIGHT, 0);
    label(t, value, 20, 8, 58, &ps_font_ddin_condensed_bold_40, 0xf5f6f2);
}

static void render_proxmox_page(lv_obj_t *screen) {
    lv_obj_set_style_bg_color(screen, C(0x040607), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    set_no_border(screen);

#if PS_PROXMOX_STATE == PS_PROXMOX_STATE_STORAGE_WARN
    top_status(screen, 1, 1, 1, "10:23", "\xF3\xB0\x81\xB9");
    hero_card(screen, "89", "Storage", "%", "ALMOST FULL", 0xff8a2a);
    tile(screen, 166, 124, "23", "CPU", "%", "LOW", 0x1cb5f0);
    tile(screen, 166, 182, "42", "RAM", "%", "NORMAL", 0x14dc78);
    tile(screen, 12, 182, "7/8", "Guests", "", "ALL RUNNING", 0x1cb5f0);
    tile(screen, 12, 124, "12", "Network", "%", "QUIET", 0x1cb5f0);
#elif PS_PROXMOX_STATE == PS_PROXMOX_STATE_GUEST_CRITICAL
    top_status(screen, 1, 1, 1, "10:23", "\xF3\xB0\x81\xB9");
    hero_card(screen, "5/8", "Guests", "", "GUESTS STOPPED", 0xff4e3e);
    tile(screen, 166, 124, "23", "CPU", "%", "LOW", 0x1cb5f0);
    tile(screen, 166, 182, "42", "RAM", "%", "NORMAL", 0x14dc78);
    tile(screen, 12, 182, "68", "Storage", "%", "NORMAL", 0x14dc78);
    tile(screen, 12, 124, "12", "Network", "%", "QUIET", 0x1cb5f0);
#elif PS_PROXMOX_STATE == PS_PROXMOX_STATE_UNCONFIGURED
    top_status(screen, 1, 1, 1, "--:--", "\xF3\xB0\x82\x83");
    hero_card(screen, "--", "Proxmox", "setup", "UNCONFIGURED", 0x9b5cff);
    tile(screen, 166, 124, "--", "CPU", "%", "UNCONFIGURED", 0x9b5cff);
    tile(screen, 166, 182, "--", "RAM", "%", "UNCONFIGURED", 0x9b5cff);
    tile(screen, 12, 182, "--", "Storage", "%", "UNCONFIGURED", 0x9b5cff);
    tile(screen, 12, 124, "--", "Network", "%", "UNCONFIGURED", 0x9b5cff);
#else
    top_status(screen, 1, 1, 1, "10:23", "\xF3\xB0\x81\xB9");
    hero_card(screen, "23", "CPU", "%", "LOW", 0x1cb5f0);
    tile(screen, 166, 124, "42", "RAM", "%", "NORMAL", 0x14dc78);
    tile(screen, 166, 182, "7/8", "Guests", "", "ALL RUNNING", 0x1cb5f0);
    tile(screen, 12, 182, "68", "Storage", "%", "NORMAL", 0x14dc78);
    tile(screen, 12, 124, "12", "Network", "%", "QUIET", 0x1cb5f0);
#endif
}

void create_ui(void) {
    render_proxmox_page(lv_screen_active());
}
