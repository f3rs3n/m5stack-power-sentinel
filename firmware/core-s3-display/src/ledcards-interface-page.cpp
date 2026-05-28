// Live-data Ledcards Interface NUT/UPS overview.
// Layout remains synchronized with assets/lvgl-spike/power-sentinel-nut-ledcards-interface-fixture.c.
#include "ledcards-interface-page.h"

#include <stdio.h>
#include <string.h>

#include "Arduino.h"
#include "lvgl.h"

LV_FONT_DECLARE(ps_font_ddin_condensed_bold_60);
LV_FONT_DECLARE(ps_font_ddin_condensed_bold_40);
LV_FONT_DECLARE(ps_icon_chart_32);

namespace {

enum MetricKind {
  METRIC_BATTERY,
  METRIC_TTE,
  METRIC_LOAD,
  METRIC_INPUT,
  METRIC_NUT,
};

enum HeroStateMarker {
  STATE_NOMINAL,
  STATE_ON_BATTERY,
  STATE_LOW_BATTERY,
  STATE_STALE,
  STATE_HIGH_LOAD,
  STATE_INPUT_LOW,
};

struct MetricRender {
  MetricKind kind;
  HeroStateMarker state;
  char value[16];
  const char *label;
  const char *unit;
  const char *stateText;
  uint32_t accent;
  uint32_t fill;
  uint32_t stateColor;
};

constexpr uint32_t kGreen = 0x14dc78;
constexpr uint32_t kBlue = 0x1cb5f0;
constexpr uint32_t kYellow = 0xfcca3d;
constexpr uint32_t kOrange = 0xff8a2a;
constexpr uint32_t kOrangeText = 0xffb064;
constexpr uint32_t kRed = 0xff4e3e;
constexpr uint32_t kRedText = 0xff6a57;
constexpr uint32_t kGray = 0x6c7470;
constexpr uint32_t kGrayText = 0xc1c5c1;
constexpr uint32_t kHeroCooldownMs = 5000;
constexpr uint32_t kTouchHeroOverrideMs = 60000;

MetricKind metricOrder[5] = {METRIC_BATTERY, METRIC_TTE, METRIC_LOAD, METRIC_INPUT, METRIC_NUT};
uint32_t lastHeroSwapMs = 0;
bool ledcardsInterfaceCreated = false;
bool touchHeroOverrideActive = false;
MetricKind touchHeroOverrideMetric = METRIC_BATTERY;
uint32_t touchHeroOverrideUntilMs = 0;
LedcardsInterfaceNutView lastRenderedView{};
bool hasLastRenderedView = false;

static void render_nut_home(lv_obj_t *screen, const LedcardsInterfaceNutView &view);

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

static uint32_t battery_color(const LedcardsInterfaceNutView &view) {
  if (view.offline || !view.upsAvailable || view.upsStale || view.batteryPercent < 0) return kGray;
  if (view.lowBattery || view.batteryPercent < 20) return kRed;
  if (view.batteryPercent < 50) return kYellow;
  return kGreen;
}

static uint32_t battery_fill(const LedcardsInterfaceNutView &view) {
  uint32_t c = battery_color(view);
  if (c == kRed) return 0x200d0c;
  if (c == kYellow) return 0x221c08;
  if (c == kGray) return 0x101514;
  return 0x071c12;
}

static uint32_t load_color(const LedcardsInterfaceNutView &view) {
  if (view.offline || !view.upsAvailable || view.upsStale || view.loadPercent < 0) return kGray;
  if (view.loadPercent >= 90) return kRed;
  if (view.loadPercent >= 70) return kOrange;
  if (view.loadPercent < 10) return kBlue;
  return kGreen;
}

static uint32_t load_fill(const LedcardsInterfaceNutView &view) {
  uint32_t c = load_color(view);
  if (c == kRed) return 0x200d0c;
  if (c == kOrange) return 0x241307;
  if (c == kBlue) return 0x07161d;
  if (c == kGray) return 0x101514;
  return 0x071c12;
}

static uint32_t input_color(const LedcardsInterfaceNutView &view) {
  if (view.offline || !view.upsAvailable || view.upsStale || view.inputVoltage <= 0.0f) return view.onBattery ? kRed : kGray;
  if (view.inputVoltage < 190.0f) return kOrange;
  if (view.inputVoltage < 210.0f) return kYellow;
  return kGreen;
}

static uint32_t input_fill(const LedcardsInterfaceNutView &view) {
  uint32_t c = input_color(view);
  if (c == kRed) return 0x200d0c;
  if (c == kOrange) return 0x241307;
  if (c == kYellow) return 0x221c08;
  if (c == kGray) return 0x101514;
  return 0x071e14;
}

static uint32_t nut_color(const LedcardsInterfaceNutView &view) {
  if (view.offline || !view.upsAvailable || view.upsStale || view.nutClientCount < 0) return kGray;
  if (view.nutClientCount == 0) return kOrange;
  return kBlue;
}

static uint32_t nut_fill(const LedcardsInterfaceNutView &view) {
  uint32_t c = nut_color(view);
  if (c == kOrange) return 0x241307;
  if (c == kGray) return 0x101514;
  return 0x07161d;
}

static void format_int_or_dash(int value, char *buf, size_t len) {
  if (value < 0) {
    snprintf(buf, len, "--");
  } else {
    snprintf(buf, len, "%d", value);
  }
}

static void format_input_voltage(float value, char *buf, size_t len) {
  if (value <= 0.0f) {
    snprintf(buf, len, "--");
  } else {
    snprintf(buf, len, "%.0f", value);
  }
}

static void format_tte_full(int seconds, char *value, size_t valueLen, const char **unit) {
  if (seconds < 0) {
    snprintf(value, valueLen, "--");
    *unit = "mm:ss";
    return;
  }
  snprintf(value, valueLen, "%02d:%02d", seconds / 60, seconds % 60);
  *unit = "mm:ss";
}

static void format_tte_minutes(int seconds, char *value, size_t valueLen, const char **unit) {
  if (seconds < 0) {
    snprintf(value, valueLen, "--");
    *unit = "mm";
    return;
  }
  snprintf(value, valueLen, "%d", (seconds + 30) / 60);
  *unit = "mm";
}

static const char *battery_state_text(const LedcardsInterfaceNutView &view) {
  if (view.offline || !view.upsAvailable || view.upsStale || view.batteryPercent < 0) return "UNKNOWN";
  if (view.charging) return "CHARGING";
  if (view.lowBattery || view.batteryPercent < 20) return "LOW BATTERY";
  if (view.batteryPercent == 100) return "FULL";
  if (view.batteryPercent >= 90) return "ALMOST FULL";
  return "GOOD";
}

static MetricRender metric_for(MetricKind kind, const LedcardsInterfaceNutView &view, bool compactTte = false) {
  MetricRender m{};
  m.kind = kind;
  m.state = STATE_NOMINAL;
  m.label = "";
  m.unit = "";
  m.stateText = "";
  m.accent = kGray;
  m.fill = 0x101514;
  m.stateColor = kGrayText;
  snprintf(m.value, sizeof(m.value), "--");

  if (kind == METRIC_BATTERY) {
    format_int_or_dash((view.offline || !view.upsAvailable || view.upsStale) ? -1 : view.batteryPercent, m.value, sizeof(m.value));
    m.label = "Battery";
    m.unit = "%";
    m.stateText = battery_state_text(view);
    m.accent = battery_color(view);
    m.fill = battery_fill(view);
    m.stateColor = (m.accent == kRed) ? kRedText : (m.accent == kGray ? kGrayText : (m.accent == kYellow ? kYellow : 0x9bb2a0));
    if (m.accent == kRed) m.state = STATE_LOW_BATTERY;
    else if (m.accent == kGray) m.state = STATE_STALE;
    return m;
  }

  if (kind == METRIC_TTE) {
    const char *unit = compactTte ? "mm" : "mm:ss";
    int runtimeSeconds = (view.offline || !view.upsAvailable || view.upsStale) ? -1 : view.runtimeSeconds;
    if (compactTte) {
      format_tte_minutes(runtimeSeconds, m.value, sizeof(m.value), &unit);
    } else {
      format_tte_full(runtimeSeconds, m.value, sizeof(m.value), &unit);
    }
    m.label = "TTE";
    m.unit = unit;
    if (view.offline || !view.upsAvailable || view.upsStale) {
      m.stateText = "UNKNOWN";
      m.accent = kGray;
      m.fill = 0x101514;
      m.stateColor = kGrayText;
      m.state = STATE_STALE;
    } else if (view.onBattery) {
      if (view.runtimeSeconds >= 0 && view.runtimeSeconds < 300) {
        m.stateText = "SHORT RUNTIME";
        m.accent = kRed;
        m.fill = 0x200d0c;
        m.stateColor = kRedText;
      } else if (view.runtimeSeconds >= 0 && view.runtimeSeconds < 600) {
        m.stateText = "SHORT RUNTIME";
        m.accent = kOrange;
        m.fill = 0x241307;
        m.stateColor = kOrangeText;
      } else {
        m.stateText = "ON BATTERY";
        m.accent = kYellow;
        m.fill = 0x221c08;
        m.stateColor = kYellow;
      }
      m.state = STATE_ON_BATTERY;
    } else {
      m.stateText = "RESERVE";
      m.accent = kBlue;
      m.fill = 0x07161d;
      m.stateColor = 0x9bb2a0;
    }
    return m;
  }

  if (kind == METRIC_LOAD) {
    format_int_or_dash((view.offline || !view.upsAvailable || view.upsStale) ? -1 : view.loadPercent, m.value, sizeof(m.value));
    m.label = "Load";
    m.unit = "%";
    m.accent = load_color(view);
    m.fill = load_fill(view);
    if (m.accent == kRed) {
      m.stateText = "LIMIT";
      m.stateColor = kRedText;
      m.state = STATE_HIGH_LOAD;
    } else if (m.accent == kOrange) {
      m.stateText = "HIGH";
      m.stateColor = kOrangeText;
      m.state = STATE_HIGH_LOAD;
    } else if (m.accent == kBlue) {
      m.stateText = "LOW";
      m.stateColor = kBlue;
    } else if (m.accent == kGray) {
      m.stateText = "UNKNOWN";
      m.stateColor = kGrayText;
      m.state = STATE_STALE;
    } else {
      m.stateText = "NORMAL";
      m.stateColor = 0x9bb2a0;
    }
    return m;
  }

  if (kind == METRIC_INPUT) {
    format_input_voltage((view.offline || !view.upsAvailable || view.upsStale) ? 0.0f : view.inputVoltage, m.value, sizeof(m.value));
    m.label = "Input";
    m.unit = "V";
    m.accent = input_color(view);
    m.fill = input_fill(view);
    if (m.accent == kRed) {
      m.stateText = "INPUT LOST";
      m.stateColor = kRedText;
      m.state = STATE_INPUT_LOW;
    } else if (m.accent == kOrange) {
      m.stateText = "INPUT LOW";
      m.stateColor = kOrangeText;
      m.state = STATE_INPUT_LOW;
    } else if (m.accent == kYellow) {
      m.stateText = "MARGINAL INPUT";
      m.stateColor = kYellow;
      m.state = STATE_INPUT_LOW;
    } else if (m.accent == kGray) {
      m.stateText = "UNKNOWN";
      m.stateColor = kGrayText;
      m.state = STATE_STALE;
    } else {
      m.stateText = "GRID ONLINE";
      m.stateColor = 0x9bb2a0;
    }
    return m;
  }

  format_int_or_dash((view.offline || !view.upsAvailable || view.upsStale) ? -1 : view.nutClientCount, m.value, sizeof(m.value));
  m.label = "NUT";
  m.unit = "client";
  m.accent = nut_color(view);
  m.fill = nut_fill(view);
  if (m.accent == kOrange) {
    m.stateText = "NO CLIENTS";
    m.stateColor = kOrangeText;
  } else if (m.accent == kGray) {
    m.stateText = view.offline ? "STALE" : "UNKNOWN";
    m.stateColor = kGrayText;
    m.state = STATE_STALE;
  } else {
    m.stateText = "CLIENTS";
    m.stateColor = kBlue;
  }
  return m;
}

static MetricKind choose_hero_metric(const LedcardsInterfaceNutView &view) {
  if (view.offline || !view.upsAvailable || view.upsStale) return METRIC_NUT;
  if (view.lowBattery || (view.batteryPercent >= 0 && view.batteryPercent < 20)) return METRIC_BATTERY;
  if (view.onBattery) return METRIC_TTE;
  if (view.loadPercent >= 70) return METRIC_LOAD;
  if (view.inputVoltage > 0.0f && view.inputVoltage < 210.0f) return METRIC_INPUT;
  return METRIC_BATTERY;
}

static void move_metric_to_front(MetricKind kind) {
  int pos = -1;
  for (int i = 0; i < 5; ++i) {
    if (metricOrder[i] == kind) pos = i;
  }
  if (pos <= 0) return;
  MetricKind selected = metricOrder[pos];
  for (int i = pos; i > 0; --i) metricOrder[i] = metricOrder[i - 1];
  metricOrder[0] = selected;
}

static MetricKind accepted_hero_metric(const LedcardsInterfaceNutView &view) {
  if (touchHeroOverrideActive) {
    if ((int32_t)(touchHeroOverrideUntilMs - view.nowMillis) > 0) {
      return touchHeroOverrideMetric;
    }
    touchHeroOverrideActive = false;
  }
  MetricKind candidate = choose_hero_metric(view);
  if (candidate == metricOrder[0]) return candidate;
  if (lastHeroSwapMs == 0 || view.nowMillis - lastHeroSwapMs >= kHeroCooldownMs) {
    move_metric_to_front(candidate);
    lastHeroSwapMs = view.nowMillis;
  }
  return metricOrder[0];
}

static void top_status(lv_obj_t *screen, const LedcardsInterfaceNutView &view, uint32_t batt_color) {
  box(screen, 6, 8, 2, 1, 0, 0x919792, 0);
  box(screen, 8, 7, 2, 1, 0, 0x919792, 0);
  box(screen, 10, 6, 2, 1, 0, 0x919792, 0);
  box(screen, 12, 7, 2, 1, 0, 0x919792, 0);
  box(screen, 14, 8, 2, 1, 0, 0x919792, 0);
  box(screen, 8, 11, 2, 1, 0, 0x919792, 0);
  box(screen, 10, 10, 2, 1, 0, 0x919792, 0);
  box(screen, 12, 11, 2, 1, 0, 0x919792, 0);
  box(screen, 10, 14, 3, 3, 2, 0x919792, 0);
  label(screen, view.offline ? "STALE" : "LIVE", 24, 2, 46, &lv_font_montserrat_10, view.offline ? kYellow : 0x919792);

  box(screen, 148, 8, 5, 5, 3, 0xf5f6f2, 0);
  box(screen, 160, 8, 4, 4, 2, 0x4d5450, 0);
  box(screen, 172, 8, 4, 4, 2, 0x4d5450, 0);

  int bw = 8;
  if (view.batteryPercent >= 0) bw = 2 + (view.batteryPercent > 100 ? 100 : view.batteryPercent) * 14 / 100;
  lv_obj_t *bat = box(screen, 294, 5, 20, 9, 2, 0x040607, 0xb1b6b2);
  box(bat, 2, 2, bw, 5, 1, batt_color, 0);
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

static void refresh_after_touch_override(void *) {
  if (hasLastRenderedView) render_nut_home(lv_screen_active(), lastRenderedView);
}

static void on_tile_clicked(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  uintptr_t raw = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
  if (raw >= 5) return;
  touchHeroOverrideMetric = static_cast<MetricKind>(raw);
  uint32_t now = millis();
  touchHeroOverrideUntilMs = now + kTouchHeroOverrideMs;
  touchHeroOverrideActive = true;
  if (hasLastRenderedView) lv_async_call(refresh_after_touch_override, nullptr);
}

static void tile(lv_obj_t *screen, int x, int y, const MetricRender &m) {
  lv_obj_t *t = box(screen, x, y, 142, 46, 7, m.fill, 0x141e1b);
  box(t, 7, 8, 5, 28, 3, m.accent, 0);
  lv_obj_t *name_l = label(t, m.label, 76, 6, 55, &lv_font_montserrat_12, 0xc9d0c9);
  lv_obj_set_style_text_align(name_l, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_t *unit_l = label(t, m.unit, 78, 23, 53, &lv_font_montserrat_12, 0x87918c);
  lv_obj_set_style_text_align(unit_l, LV_TEXT_ALIGN_RIGHT, 0);
  // Keep the metric value visually on top of the right-side label/unit objects.
  // Mini-card TTE is minutes-only because clock-form values such as "06:24"
  // are too tight for this physical TFT/card geometry.
  label(t, m.value, 20, 8, 58, &ps_font_ddin_condensed_bold_40, 0xf5f6f2);
  lv_obj_t *hit = lv_obj_create(t);
  lv_obj_remove_style_all(hit);
  lv_obj_set_size(hit, 142, 46);
  lv_obj_set_pos(hit, 0, 0);
  lv_obj_add_flag(hit, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_scrollbar_mode(hit, LV_SCROLLBAR_MODE_OFF);
  lv_obj_add_event_cb(hit, on_tile_clicked, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<uintptr_t>(m.kind)));
}

static int hero_label_x(const MetricRender &hero) {
  size_t len = strlen(hero.value);
  if (len >= 5) return 160;
  if (len >= 3) return 124;
  return 96;
}

static void render_nut_home(lv_obj_t *screen, const LedcardsInterfaceNutView &view) {
  lastRenderedView = view;
  hasLastRenderedView = true;
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, C(0x040607), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  set_no_border(screen);

  MetricKind heroKind = accepted_hero_metric(view);
  MetricRender hero = metric_for(heroKind, view);
  top_status(screen, view, hero.accent);

  int label_x = hero_label_x(hero);
  box(screen, 19, 33, 7, 76, 4, hero.accent, 0);
  label(screen, hero.value, 43, 33, 120, &ps_font_ddin_condensed_bold_60, 0xf5f6f2);
  label(screen, hero.label, label_x, 33, 72, &lv_font_montserrat_12, 0xbeb8a0);
  label(screen, hero.unit, label_x, 63, 72, &lv_font_montserrat_12, 0x968f78);
  label(screen, hero.stateText, 45, 96, 142, &lv_font_montserrat_14, hero.stateColor);
  chart_button(screen);

  int rendered = 0;
  const int xs[4] = {12, 166, 12, 166};
  const int ys[4] = {124, 124, 182, 182};
  for (int i = 0; i < 5 && rendered < 4; ++i) {
    if (metricOrder[i] == heroKind) continue;
    tile(screen, xs[rendered], ys[rendered], metric_for(metricOrder[i], view, true));
    ++rendered;
  }
}

}  // namespace

void createLedcardsInterfaceUi(const LedcardsInterfaceNutView &view) {
  ledcardsInterfaceCreated = true;
  render_nut_home(lv_screen_active(), view);
}

void updateLedcardsInterfaceUi(const LedcardsInterfaceNutView &view) {
  if (!ledcardsInterfaceCreated) {
    createLedcardsInterfaceUi(view);
    return;
  }
  render_nut_home(lv_screen_active(), view);
}
