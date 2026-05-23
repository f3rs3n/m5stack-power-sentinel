#include "lvgl.h"

static lv_color_t C(uint32_t hex) { return lv_color_hex(hex); }

static void style_panel(lv_obj_t *card, uint32_t bg, uint32_t border) {
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
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
}

static lv_obj_t *add_line(lv_obj_t *parent, const char *text) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, C(0xc8d0df), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, lv_pct(100));
    return label;
}

static void add_badge(lv_obj_t *parent, const char *text, uint32_t color) {
    lv_obj_t *badge = lv_label_create(parent);
    lv_label_set_text(badge, text);
    lv_obj_set_style_text_color(badge, lv_color_white(), 0);
    lv_obj_set_style_text_font(badge, &lv_font_montserrat_12, 0);
    lv_obj_set_style_bg_color(badge, C(color), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(badge, 10, 0);
    lv_obj_set_style_pad_hor(badge, 8, 0);
    lv_obj_set_style_pad_ver(badge, 4, 0);
}

static void add_metric_row(lv_obj_t *parent, const char *name, const char *value) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *left = lv_label_create(row);
    lv_label_set_text(left, name);
    lv_obj_set_style_text_color(left, C(0x8fa0b8), 0);
    lv_obj_set_style_text_font(left, &lv_font_montserrat_12, 0);

    lv_obj_t *right = lv_label_create(row);
    lv_label_set_text(right, value);
    lv_obj_set_style_text_color(right, C(0xf8fbff), 0);
    lv_obj_set_style_text_font(right, &lv_font_montserrat_14, 0);
}

static void add_percent_bar(lv_obj_t *parent, int value, uint32_t color) {
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_set_width(bar, lv_pct(100));
    lv_obj_set_height(bar, 10);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, value, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, C(0x283041), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, C(color), LV_PART_INDICATOR);
}

static void add_status_pill_row(lv_obj_t *parent) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_gap(row, 4, 0);
    add_badge(row, "NUT OK", 0x22c55e);
    add_badge(row, "PVE OK", 0x22c55e);
    add_badge(row, "HA OK", 0x22c55e);
}

static void setup_page(lv_obj_t *tab) {
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(tab, 4, 0);
    lv_obj_set_style_pad_gap(tab, 4, 0);
}

static void render_home(lv_obj_t *home) {
    setup_page(home);

    lv_obj_t *card = lv_obj_create(home);
    style_panel(card, 0x111827, 0x22c55e);
    lv_obj_set_height(card, 150);
    lv_obj_set_style_pad_all(card, 7, 0);
    lv_obj_set_style_pad_gap(card, 4, 0);

    lv_obj_t *header = lv_obj_create(card);
    lv_obj_remove_style_all(header);
    lv_obj_set_width(header, lv_pct(100));
    lv_obj_set_height(header, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "POWER SENTINEL");
    lv_obj_set_width(title, 215);
    lv_label_set_long_mode(title, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(title, C(0xf8fbff), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    add_badge(header, "OK", 0x22c55e);

    add_line(card, "GRID ONLINE");
    add_metric_row(card, "battery / runtime", "100%   57m00s");
    add_percent_bar(card, 100, 0x22c55e);
    add_metric_row(card, "load / input", "19%   232.0V");
    add_percent_bar(card, 19, 0x3b82f6);
    add_status_pill_row(card);

    lv_obj_t *bottom = lv_obj_create(home);
    lv_obj_remove_style_all(bottom);
    lv_obj_set_width(bottom, lv_pct(100));
    lv_obj_set_flex_grow(bottom, 1);
    lv_obj_set_flex_flow(bottom, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(bottom, 5, 0);

    lv_obj_t *local = lv_obj_create(bottom);
    lv_obj_set_width(local, 168);
    lv_obj_set_height(local, lv_pct(100));
    lv_obj_set_flex_flow(local, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(local, 2, 0);
    lv_obj_set_style_pad_all(local, 6, 0);
    lv_obj_set_style_radius(local, 12, 0);
    lv_obj_set_style_border_width(local, 1, 0);
    lv_obj_set_style_border_color(local, C(0x394152), 0);
    lv_obj_set_style_bg_color(local, C(0x171b24), 0);
    lv_obj_set_style_bg_opa(local, LV_OPA_COVER, 0);

    lv_obj_t *local_value = lv_label_create(local);
    lv_label_set_text(local_value, "local NET OK M5S OK");
    lv_obj_set_width(local_value, lv_pct(100));
    lv_label_set_long_mode(local_value, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(local_value, C(0xf8fbff), 0);
    lv_obj_set_style_text_font(local_value, &lv_font_montserrat_14, 0);

    lv_obj_t *problem_value = lv_label_create(local);
    lv_label_set_text(problem_value, "Problems: No active problems");
    lv_obj_set_width(problem_value, lv_pct(100));
    lv_label_set_long_mode(problem_value, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(problem_value, C(0xc8d0df), 0);
    lv_obj_set_style_text_font(problem_value, &lv_font_montserrat_12, 0);

    lv_obj_t *button = lv_button_create(bottom);
    lv_obj_set_flex_grow(button, 1);
    lv_obj_set_height(button, lv_pct(100));
    lv_obj_set_style_radius(button, 12, 0);
    lv_obj_set_style_bg_color(button, C(0x1f2937), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_border_color(button, C(0x4b5563), 0);

    lv_obj_t *sleep_label = lv_label_create(button);
    lv_label_set_text(sleep_label, "SLEEP DISPLAY");
    lv_label_set_long_mode(sleep_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(sleep_label, 88);
    lv_obj_set_style_text_align(sleep_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(sleep_label, C(0xe5e7eb), 0);
    lv_obj_set_style_text_font(sleep_label, &lv_font_montserrat_14, 0);
    lv_obj_center(sleep_label);
}

static void add_placeholder(lv_obj_t *tab, const char *title, const char *status) {
    setup_page(tab);
    lv_obj_t *card = lv_obj_create(tab);
    style_panel(card, 0x171b24, 0x394152);
    lv_obj_t *label = lv_label_create(card);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_color(label, C(0xe8eefc), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    add_line(card, status);
}

void create_ui(void) {
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, C(0x070b12), 0);
    lv_obj_set_style_bg_grad_color(screen, C(0x121826), 0);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_text_font(screen, &lv_font_montserrat_14, 0);

    lv_obj_t *tabview = lv_tabview_create(screen);
    lv_tabview_set_tab_bar_position(tabview, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(tabview, 24);
    lv_obj_set_style_bg_color(tabview, C(0x070b12), 0);

    lv_obj_t *tab_bar = lv_tabview_get_tab_bar(tabview);
    lv_obj_set_style_bg_color(tab_bar, C(0x0b1220), 0);
    lv_obj_set_style_bg_opa(tab_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(tab_bar, 4, 0);
    lv_obj_set_style_pad_gap(tab_bar, 2, 0);
    lv_obj_set_style_text_font(tab_bar, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(tab_bar, C(0x9fb0c8), LV_PART_ITEMS);
    lv_obj_set_style_text_color(tab_bar, C(0xf8fbff), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(tab_bar, C(0x2563eb), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(tab_bar, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_radius(tab_bar, 9, LV_PART_ITEMS);

    lv_obj_t *home = lv_tabview_add_tab(tabview, "HOME");
    lv_obj_t *nut = lv_tabview_add_tab(tabview, "NUT");
    lv_obj_t *pve = lv_tabview_add_tab(tabview, "PVE");
    lv_obj_t *ha = lv_tabview_add_tab(tabview, "HA");
    lv_obj_t *m5s = lv_tabview_add_tab(tabview, "M5S");

    render_home(home);
    add_placeholder(nut, "NUT", "ONLINE / NUT OK");
    add_placeholder(pve, "Proxmox", "PVE OK");
    add_placeholder(ha, "Home Assistant", "HA OK / Z2M OK");
    add_placeholder(m5s, "M5S", "LIVE / StackFlow OK");
}
