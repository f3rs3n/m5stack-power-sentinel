#include "lvgl.h"
#include "ps_ui_tab_12.c"
#include "ps_ui_tab_18.c"

#ifndef PS_ACTIVE_TAB
#define PS_ACTIVE_TAB 0
#endif

#ifndef PS_ICON_ONLY_SIDEBAR
#define PS_ICON_ONLY_SIDEBAR 0
#endif

#define PS_PAGE_CARD_WIDTH 252
#define PS_PAGE_CARD_HEIGHT 220

#define PS_ICON_SETTINGS "\xEF\x80\x93"       // U+F013 fa-gear
#define PS_ICON_HOME "\xEF\x80\x95"           // U+F015 fa-home
#define PS_ICON_NUT "\xF3\xB0\x9B\xB8"        // U+F06F8 nf-md-nut
#define PS_ICON_HOME_ASSISTANT "\xF3\xB0\x9F\x90"  // U+F07D0 nf-md-home_assistant
#define PS_ICON_SERVER "\xEF\x88\xB3"         // U+F233 fa-server

static lv_color_t C(uint32_t hex) { return lv_color_hex(hex); }

static void base_screen(lv_obj_t *screen) {
    lv_obj_set_style_bg_color(screen, C(0x070b12), 0);
    lv_obj_set_style_bg_grad_color(screen, C(0x121826), 0);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_text_font(screen, &lv_font_montserrat_14, 0);
}

static void page(lv_obj_t *tab) {
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(tab, 8, 0);
    lv_obj_set_style_pad_gap(tab, 8, 0);
    lv_obj_set_scroll_dir(tab, LV_DIR_HOR);
    lv_obj_set_scroll_snap_x(tab, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scrollbar_mode(tab, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
    lv_obj_add_flag(tab, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
}

static void panel(lv_obj_t *card, uint32_t bg, uint32_t border) {
    lv_obj_set_width(card, PS_PAGE_CARD_WIDTH);
    lv_obj_set_height(card, PS_PAGE_CARD_HEIGHT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(card, 6, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, C(border), 0);
    lv_obj_set_style_bg_color(card, C(bg), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(card, 10, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_60, 0);
    lv_obj_set_style_shadow_color(card, C(0x000000), 0);
    lv_obj_set_style_shadow_ofs_y(card, 3, 0);
    lv_obj_set_scroll_dir(card, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_add_flag(card, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
}

static lv_obj_t *card(lv_obj_t *parent, const char *title) {
    lv_obj_t *c = lv_obj_create(parent);
    panel(c, 0x111827, 0x334155);
    lv_obj_t *label = lv_label_create(c);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_color(label, C(0xe8eefc), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    return c;
}

static void line(lv_obj_t *parent, const char *text, uint32_t color, const lv_font_t *font) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_width(l, lv_pct(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(l, C(color), 0);
    lv_obj_set_style_text_font(l, font, 0);
}

static void badge(lv_obj_t *parent, const char *text, uint32_t color) {
    lv_obj_t *b = lv_label_create(parent);
    lv_label_set_text(b, text);
    lv_obj_set_style_text_color(b, lv_color_white(), 0);
    lv_obj_set_style_text_font(b, &lv_font_montserrat_12, 0);
    lv_obj_set_style_bg_color(b, C(color), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(b, 10, 0);
    lv_obj_set_style_pad_hor(b, 8, 0);
    lv_obj_set_style_pad_ver(b, 4, 0);
}

static void row(lv_obj_t *parent, const char *left_text, const char *right_text) {
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_remove_style_all(r);
    lv_obj_set_width(r, lv_pct(100));
    lv_obj_set_height(r, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *left = lv_label_create(r);
    lv_label_set_text(left, left_text);
    lv_obj_set_width(left, 142);
    lv_label_set_long_mode(left, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(left, C(0x8fa0b8), 0);
    lv_obj_set_style_text_font(left, &lv_font_montserrat_12, 0);
    lv_obj_t *right = lv_label_create(r);
    lv_label_set_text(right, right_text);
    lv_label_set_long_mode(right, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(right, C(0xf8fbff), 0);
    lv_obj_set_style_text_font(right, &lv_font_montserrat_14, 0);
}

static void bar(lv_obj_t *parent, int value, uint32_t color, int height) {
    lv_obj_t *b = lv_bar_create(parent);
    lv_obj_set_width(b, lv_pct(100));
    lv_obj_set_height(b, height);
    lv_bar_set_range(b, 0, 100);
    lv_bar_set_value(b, value, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(b, C(0x283041), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(b, C(color), LV_PART_INDICATOR);
}

static void compact_metric(lv_obj_t *parent, const char *name, const char *pct, const char *total, int value, uint32_t color) {
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_remove_style_all(r);
    lv_obj_set_width(r, lv_pct(100));
    lv_obj_set_height(r, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    char left_buf[64];
    lv_snprintf(left_buf, sizeof(left_buf), "%s %s", name, pct);
    lv_obj_t *left = lv_label_create(r);
    lv_label_set_text(left, left_buf);
    lv_obj_set_width(left, total && total[0] ? 132 : lv_pct(100));
    lv_label_set_long_mode(left, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(left, C(0xc8d0df), 0);
    lv_obj_set_style_text_font(left, &lv_font_montserrat_12, 0);

    if (total && total[0]) {
        lv_obj_t *right = lv_label_create(r);
        lv_obj_set_width(right, 68);
        lv_label_set_text(right, total);
        lv_label_set_long_mode(right, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(right, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_color(right, C(0xf8fbff), 0);
        lv_obj_set_style_text_font(right, &lv_font_montserrat_12, 0);
    }

    bar(parent, value, color, 10);
}

static void pills3(lv_obj_t *parent, const char *a, uint32_t ca, const char *b, uint32_t cb, const char *c, uint32_t cc) {
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_remove_style_all(r);
    lv_obj_set_width(r, lv_pct(100));
    lv_obj_set_height(r, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_gap(r, 4, 0);
    badge(r, a, ca); badge(r, b, cb); badge(r, c, cc);
}

static void pve_header(lv_obj_t *parent) {
    lv_obj_t *h = lv_obj_create(parent);
    lv_obj_remove_style_all(h);
    lv_obj_set_width(h, lv_pct(100));
    lv_obj_set_height(h, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(h, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(h, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *title = lv_label_create(h);
    lv_label_set_text(title, "Proxmox");
    lv_obj_set_width(title, 160);
    lv_label_set_long_mode(title, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(title, C(0xe8eefc), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    badge(h, "ONLINE", 0x22c55e);
}

static void pve_net_pills(lv_obj_t *parent) {
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_remove_style_all(r);
    lv_obj_set_width(r, lv_pct(100));
    lv_obj_set_height(r, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_gap(r, 4, 0);
    badge(r, "eth25g", 0x06b6d4);
    badge(r, "vmbr0", 0x06b6d4);
}

static void force_sidebar_label_contrast(lv_obj_t *bar_obj) {
    // lv_tabview's generated button labels do not always inherit LV_PART_ITEMS
    // text styles in the simulator. Force the child labels to match the live
    // CoreS3: inactive labels are white/readable; active state is the blue pill.
    uint32_t count = lv_obj_get_child_count(bar_obj);
    for (uint32_t i = 0; i < count; ++i) {
        lv_obj_t *btn = lv_obj_get_child(bar_obj, i);
        if (!btn) continue;
        lv_obj_set_style_text_color(btn, C(0xf8fbff), 0);
        lv_obj_set_style_text_opa(btn, LV_OPA_COVER, 0);
        uint32_t child_count = lv_obj_get_child_count(btn);
        for (uint32_t j = 0; j < child_count; ++j) {
            lv_obj_t *label = lv_obj_get_child(btn, j);
            if (!label) continue;
            lv_obj_set_style_text_color(label, C(0xf8fbff), 0);
            lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
        }
    }
}

static void render_home(lv_obj_t *tab) {
    page(tab);
    lv_obj_t *c = lv_obj_create(tab);
    panel(c, 0x111827, 0x22c55e);
    lv_obj_set_style_pad_gap(c, 4, 0);

    lv_obj_t *h = lv_obj_create(c);
    lv_obj_remove_style_all(h);
    lv_obj_set_width(h, lv_pct(100));
    lv_obj_set_height(h, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(h, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(h, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *title = lv_label_create(h);
    lv_label_set_text(title, "POWER SENTINEL");
    lv_obj_set_width(title, 215);
    lv_label_set_long_mode(title, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(title, C(0xf8fbff), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    badge(h, "OK", 0x22c55e);
    line(c, "GRID ONLINE", 0xc8d0df, &lv_font_montserrat_14);
    row(c, "battery / runtime", "100%   57m00s");
    bar(c, 100, 0x22c55e, 10);
    row(c, "load / input", "19%   232.0V");
    bar(c, 19, 0x3b82f6, 10);
    pills3(c, "NUT OK", 0x22c55e, "PVE online", 0x22c55e, "HA online", 0x22c55e);

    lv_obj_t *bottom = lv_obj_create(tab);
    panel(bottom, 0x171b24, 0x394152);
    lv_obj_set_flex_flow(bottom, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(bottom, 8, 0);

    lv_obj_t *bottom_title = lv_label_create(bottom);
    lv_label_set_text(bottom_title, "Local controls");
    lv_obj_set_style_text_color(bottom_title, C(0xe8eefc), 0);
    lv_obj_set_style_text_font(bottom_title, &lv_font_montserrat_16, 0);

    lv_obj_t *local = lv_obj_create(bottom);
    lv_obj_set_width(local, lv_pct(100));
    lv_obj_set_height(local, 82);
    lv_obj_set_flex_flow(local, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(local, 2, 0);
    lv_obj_set_style_pad_all(local, 6, 0);
    lv_obj_set_style_radius(local, 12, 0);
    lv_obj_set_style_border_width(local, 1, 0);
    lv_obj_set_style_border_color(local, C(0x394152), 0);
    lv_obj_set_style_bg_color(local, C(0x171b24), 0);
    lv_obj_set_style_bg_opa(local, LV_OPA_COVER, 0);
    line(local, "local NET OK M5S OK", 0xf8fbff, &lv_font_montserrat_14);
    line(local, "Problems: No active problems", 0xc8d0df, &lv_font_montserrat_12);

    lv_obj_t *button = lv_button_create(bottom);
    lv_obj_set_width(button, lv_pct(100));
    lv_obj_set_height(button, 56);
    lv_obj_set_style_radius(button, 12, 0);
    lv_obj_set_style_bg_color(button, C(0x1f2937), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_border_color(button, C(0x4b5563), 0);
    lv_obj_t *sleep = lv_label_create(button);
    lv_label_set_text(sleep, "SLEEP\nDISPLAY");
    lv_obj_set_style_text_align(sleep, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(sleep, C(0xe5e7eb), 0);
    lv_obj_set_style_text_font(sleep, &lv_font_montserrat_14, 0);
    lv_obj_center(sleep);
}

static void render_nut(lv_obj_t *tab) {
    page(tab);
    lv_obj_t *c = card(tab, "NUT");
    badge(c, "ONLINE", 0x22c55e);
    line(c, "Status: Online (OL)", 0xc8d0df, &lv_font_montserrat_14);
    line(c, "Battery 100%   Runtime 57m00s", 0xc8d0df, &lv_font_montserrat_14);
    bar(c, 100, 0x22c55e, 10);
    line(c, "Load 19% / 95W   Input 232.0V", 0xc8d0df, &lv_font_montserrat_14);
    bar(c, 19, 0x3b82f6, 10);

    lv_obj_t *n = card(tab, "NUT server");
    line(n, "server OK   driver OK", 0xc8d0df, &lv_font_montserrat_14);
    line(n, "monitor off   mode netserver", 0xc8d0df, &lv_font_montserrat_14);
    line(n, "shutdown not_armed   clients 1", 0xc8d0df, &lv_font_montserrat_14);
    line(n, "client list: pve", 0xc8d0df, &lv_font_montserrat_14);
}

static void render_pve(lv_obj_t *tab) {
    page(tab);
    lv_obj_t *c = lv_obj_create(tab);
    panel(c, 0x171b24, 0x394152);
    lv_obj_set_style_pad_gap(c, 5, 0);
    pve_header(c);
    compact_metric(c, "CPU", "3%", "", 3, 0x3b82f6);
    compact_metric(c, "RAM", "51%", "31GB", 51, 0xa855f7);
    compact_metric(c, "Storage", "8%", "7.2TB", 8, 0x14b8a6);
    lv_obj_t *r = lv_obj_create(c);
    lv_obj_remove_style_all(r);
    lv_obj_set_width(r, lv_pct(100));
    lv_obj_set_height(r, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_gap(r, 4, 0);
    badge(r, "ZFS online", 0x22c55e);
    badge(r, "SMART ok", 0x22c55e);
    pve_net_pills(c);
    line(c, "NUT monitor idle   armed NO", 0xc8d0df, &lv_font_montserrat_14);

    lv_obj_t *w = card(tab, "VM haos");
    compact_metric(w, "CPU", "6%", "", 6, 0x3b82f6);
    compact_metric(w, "RAM", "90%", "4.0GB", 90, 0xa855f7);
    compact_metric(w, "HDD", "15%", "60GB", 15, 0x14b8a6);

    lv_obj_t *l = card(tab, "LXC docker");
    compact_metric(l, "CPU", "3%", "", 3, 0x3b82f6);
    compact_metric(l, "RAM", "41%", "8.0GB", 41, 0xa855f7);
    compact_metric(l, "HDD", "37%", "32GB", 37, 0x14b8a6);
}

static void render_ha(lv_obj_t *tab) {
    page(tab);
    lv_obj_t *h = card(tab, "Home Assistant");
    badge(h, "HA OK", 0x22c55e);
    row(h, "core", "API OK   MQTT OK");
    row(h, "updates", "Updates 0");

    lv_obj_t *z = card(tab, "Z2M / Coordinator");
    badge(z, "Z2M OK", 0x22c55e);
    line(z, "State online", 0xc8d0df, &lv_font_montserrat_14);
    row(z, "coordinator", "Coordinator OK");
    line(z, "EmberZNet   fw 7.4.5 [GA]", 0xc8d0df, &lv_font_montserrat_14);
    row(z, "devices", "Z2M devices: 29/29");
    line(z, "Disabled 0", 0xc8d0df, &lv_font_montserrat_14);
}

static void render_m5s(lv_obj_t *tab) {
    page(tab);
    lv_obj_t *m = card(tab, "M5S");
    badge(m, "M5S OK", 0x22c55e);
    line(m, "Temp 44.2 C   RAM 586 MB", 0xc8d0df, &lv_font_montserrat_14);
    line(m, "Disk free 5.6 GB", 0xc8d0df, &lv_font_montserrat_14);
    line(m, "StackFlow OK   OpenAI OK", 0xc8d0df, &lv_font_montserrat_14);
    line(m, "Chat smoke n/a", 0xc8d0df, &lv_font_montserrat_14);

    lv_obj_t *t = card(tab, "Transport");
    badge(t, "LIVE", 0x22c55e);
    line(t, "Source stackflow   Schema v1", 0xc8d0df, &lv_font_montserrat_14);
    line(t, "Transport: StackFlow OK", 0xc8d0df, &lv_font_montserrat_14);
    line(t, "Poll 123 ok / 0 fail / last 92ms", 0xc8d0df, &lv_font_montserrat_14);
    line(t, "PVE API 81ms", 0xc8d0df, &lv_font_montserrat_14);
    line(t, "FW stackflow-2026-05-23-pve-polish", 0xc8d0df, &lv_font_montserrat_14);
}

void create_ui(void) {
    lv_obj_t *screen = lv_screen_active();
    base_screen(screen);
    lv_obj_t *tv = lv_tabview_create(screen);
    lv_tabview_set_tab_bar_position(tv, LV_DIR_LEFT);
#if PS_ICON_ONLY_SIDEBAR
    lv_tabview_set_tab_bar_size(tv, 44);
#else
    lv_tabview_set_tab_bar_size(tv, 44);
#endif
    lv_obj_set_style_bg_color(tv, C(0x070b12), 0);

    lv_obj_t *content = lv_tabview_get_content(tv);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_set_scroll_snap_y(content, LV_SCROLL_SNAP_CENTER);

    lv_obj_t *bar_obj = lv_tabview_get_tab_bar(tv);
    lv_obj_set_style_bg_color(bar_obj, C(0x0b1220), 0);
    lv_obj_set_style_bg_opa(bar_obj, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(bar_obj, 2, 0);
    lv_obj_set_style_pad_ver(bar_obj, 4, 0);
    lv_obj_set_style_pad_gap(bar_obj, 3, 0);
#if PS_ICON_ONLY_SIDEBAR
    lv_obj_set_style_text_font(bar_obj, &ps_ui_tab_18, 0);
    lv_obj_set_style_pad_hor(bar_obj, 3, 0);
    lv_obj_set_style_pad_ver(bar_obj, 6, 0);
    lv_obj_set_style_pad_gap(bar_obj, 6, 0);
#else
    lv_obj_set_style_text_font(bar_obj, &ps_ui_tab_12, 0);
#endif
    lv_obj_set_style_text_align(bar_obj, LV_TEXT_ALIGN_CENTER, LV_PART_ITEMS);
    // Match the live CoreS3: inactive sidebar labels stay bright/readable,
    // selected tab is distinguished by the blue background rather than dimming
    // every other label.
    lv_obj_set_style_text_color(bar_obj, C(0xf8fbff), LV_PART_ITEMS);
    lv_obj_set_style_text_opa(bar_obj, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_opa(bar_obj, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(bar_obj, C(0xf8fbff), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_opa(bar_obj, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(bar_obj, C(0x0f1b2d), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(bar_obj, LV_OPA_60, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(bar_obj, C(0x2563eb), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(bar_obj, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_radius(bar_obj, 12, LV_PART_ITEMS);

#if PS_ICON_ONLY_SIDEBAR
    lv_obj_t *home = lv_tabview_add_tab(tv, PS_ICON_HOME);
    lv_obj_t *nut = lv_tabview_add_tab(tv, PS_ICON_NUT);
    lv_obj_t *pve = lv_tabview_add_tab(tv, PS_ICON_SERVER);
    lv_obj_t *ha = lv_tabview_add_tab(tv, PS_ICON_HOME_ASSISTANT);
    lv_obj_t *m5s = lv_tabview_add_tab(tv, PS_ICON_SETTINGS);
#else
    lv_obj_t *home = lv_tabview_add_tab(tv, PS_ICON_HOME "\nHM");
    lv_obj_t *nut = lv_tabview_add_tab(tv, PS_ICON_NUT "\nNT");
    lv_obj_t *pve = lv_tabview_add_tab(tv, PS_ICON_SERVER "\nPV");
    lv_obj_t *ha = lv_tabview_add_tab(tv, PS_ICON_HOME_ASSISTANT "\nHA");
    lv_obj_t *m5s = lv_tabview_add_tab(tv, PS_ICON_SETTINGS "\nM5");
#endif

    force_sidebar_label_contrast(bar_obj);

    render_home(home);
    render_nut(nut);
    render_pve(pve);
    render_ha(ha);
    render_m5s(m5s);
    lv_tabview_set_active(tv, PS_ACTIVE_TAB, LV_ANIM_OFF);
}
