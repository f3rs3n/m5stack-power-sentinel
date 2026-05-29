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
constexpr uint32_t kPurple = 0x9b5cff;
constexpr uint32_t kPurpleText = 0xc4b5fd;
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
bool ringAnimationActive = false;
lv_obj_t *ringAnimationOverlay = nullptr;
LedcardsInterfaceNutView pendingAnimationView{};
bool hasPendingAnimationView = false;

struct SlotPosition {
  int x;
  int y;
};

struct RingGhostAnim {
  lv_obj_t *obj;
  uint8_t steps;
  uint8_t path[5];
  uint8_t baseOpa;
};

RingGhostAnim ringGhostAnimations[5]{};

static void render_nut_home(lv_obj_t *screen, const LedcardsInterfaceNutView &view);
static void draw_nut_home_static(lv_obj_t *screen, const LedcardsInterfaceNutView &view);
static void start_ring_transition(lv_obj_t *screen, MetricKind target, const LedcardsInterfaceNutView &view);

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

static uint32_t state_fill(uint32_t c) {
  if (c == kRed) return 0x200d0c;
  if (c == kOrange) return 0x241307;
  if (c == kYellow) return 0x221c08;
  if (c == kBlue) return 0x07161d;
  if (c == kPurple) return 0x180f26;
  if (c == kGray) return 0x101514;
  return 0x071c12;
}

static uint32_t state_text_color(uint32_t c) {
  if (c == kRed) return kRedText;
  if (c == kOrange) return kOrangeText;
  if (c == kYellow) return kYellow;
  if (c == kBlue) return kBlue;
  if (c == kPurple) return kPurpleText;
  if (c == kGray) return kGrayText;
  return 0x9bb2a0;
}

static uint32_t battery_color(const LedcardsInterfaceNutView &view) {
  if (!view.upsAvailable) return kPurple;
  if (view.offline || view.upsStale) return kGray;
  if (view.batteryPercent < 0) return kPurple;
  if (view.onBattery) {
    if (view.batteryPercent < 10) return kRed;
    if (view.lowBattery || view.batteryPercent < 20) return kOrange;
    return kYellow;
  }
  if (view.lowBattery || view.batteryPercent < 20) return kRed;
  if (view.batteryPercent < 50) return kOrange;
  if (view.batteryPercent < 90) return kYellow;
  return kGreen;
}

static uint32_t battery_fill(const LedcardsInterfaceNutView &view) {
  return state_fill(battery_color(view));
}

static uint32_t load_color(const LedcardsInterfaceNutView &view) {
  if (!view.upsAvailable) return kPurple;
  if (view.offline || view.upsStale) return kGray;
  if (view.loadPercent < 0) return kPurple;
  if (view.loadPercent >= 90) return kRed;
  if (view.loadPercent >= 70) return kOrange;
  if (view.loadPercent < 10) return kBlue;
  return kGreen;
}

static uint32_t load_fill(const LedcardsInterfaceNutView &view) {
  return state_fill(load_color(view));
}

static uint32_t input_color(const LedcardsInterfaceNutView &view) {
  if (!view.upsAvailable) return kPurple;
  if (view.offline || view.upsStale) return kGray;
  if (view.inputVoltage <= 0.0f) return view.onBattery ? kRed : kGray;
  if (view.inputVoltage < 190.0f) return kOrange;
  if (view.inputVoltage < 210.0f) return kYellow;
  return kGreen;
}

static uint32_t input_fill(const LedcardsInterfaceNutView &view) {
  return state_fill(input_color(view));
}

static uint32_t nut_color(const LedcardsInterfaceNutView &view) {
  if (!view.upsAvailable) return kPurple;
  if (view.offline || view.upsStale) return kGray;
  if (view.nutClientCount < 0) return kPurple;
  if (view.nutClientCount == 0) return kOrange;
  return kBlue;
}

static uint32_t nut_fill(const LedcardsInterfaceNutView &view) {
  return state_fill(nut_color(view));
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
    *unit = "m";
    return;
  }
  snprintf(value, valueLen, "%d", (seconds + 30) / 60);
  *unit = "m";
}

static bool telemetry_unavailable(const LedcardsInterfaceNutView &view) {
  return !view.upsAvailable;
}

static bool telemetry_stale(const LedcardsInterfaceNutView &view) {
  return view.offline || view.upsStale;
}

static bool telemetry_missing(const LedcardsInterfaceNutView &view) {
  return telemetry_unavailable(view) || telemetry_stale(view);
}

static const char *missing_state_text(const LedcardsInterfaceNutView &view) {
  return telemetry_unavailable(view) ? "UNAVAILABLE" : "STALE";
}

static const char *battery_state_text(const LedcardsInterfaceNutView &view) {
  if (telemetry_missing(view)) return missing_state_text(view);
  if (view.batteryPercent < 0) return "UNAVAILABLE";
  if (view.onBattery) {
    if (view.batteryPercent < 10) return "CRITICAL BATTERY";
    if (view.lowBattery || view.batteryPercent < 20) return "LOW BATTERY";
    return "ON BATTERY";
  }
  if (view.lowBattery || view.batteryPercent < 20) return "LOW BATTERY";
  if (view.batteryPercent >= 90) return "FULL";
  if (view.batteryPercent >= 50) return "ALMOST FULL";
  return "CHARGING";
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
    format_int_or_dash(telemetry_missing(view) ? -1 : view.batteryPercent, m.value, sizeof(m.value));
    m.label = "Battery";
    m.unit = "%";
    m.stateText = battery_state_text(view);
    m.accent = battery_color(view);
    m.fill = battery_fill(view);
    m.stateColor = state_text_color(m.accent);
    if (m.accent == kRed || m.accent == kOrange) m.state = STATE_LOW_BATTERY;
    else if (m.accent == kGray || m.accent == kPurple) m.state = STATE_STALE;
    return m;
  }

  if (kind == METRIC_TTE) {
    const char *unit = compactTte ? "m" : "mm:ss";
    int runtimeSeconds = telemetry_missing(view) ? -1 : view.runtimeSeconds;
    if (compactTte) {
      format_tte_minutes(runtimeSeconds, m.value, sizeof(m.value), &unit);
    } else {
      format_tte_full(runtimeSeconds, m.value, sizeof(m.value), &unit);
    }
    m.label = "Runtime";
    m.unit = unit;
    if (telemetry_missing(view)) {
      m.stateText = missing_state_text(view);
      m.accent = telemetry_unavailable(view) ? kPurple : kGray;
      m.fill = state_fill(m.accent);
      m.stateColor = state_text_color(m.accent);
      m.state = STATE_STALE;
    } else if (view.onBattery) {
      if (view.runtimeSeconds >= 0 && view.runtimeSeconds < 120) {
        m.stateText = "CRITICAL RUNTIME";
        m.accent = kRed;
        m.fill = state_fill(m.accent);
        m.stateColor = state_text_color(m.accent);
      } else if (view.runtimeSeconds >= 0 && view.runtimeSeconds < 300) {
        m.stateText = "SHORT RUNTIME";
        m.accent = kOrange;
        m.fill = state_fill(m.accent);
        m.stateColor = state_text_color(m.accent);
      } else {
        m.stateText = "ON BATTERY";
        m.accent = kYellow;
        m.fill = state_fill(m.accent);
        m.stateColor = state_text_color(m.accent);
      }
      m.state = STATE_ON_BATTERY;
    } else {
      if (view.runtimeSeconds >= 0 && view.runtimeSeconds < 120) {
        m.stateText = "CRITICAL RESERVE";
        m.accent = kRed;
      } else if (view.runtimeSeconds >= 0 && view.runtimeSeconds < 300) {
        m.stateText = "LOW RESERVE";
        m.accent = kYellow;
      } else {
        m.stateText = "RESERVE";
        m.accent = kBlue;
      }
      m.fill = state_fill(m.accent);
      m.stateColor = state_text_color(m.accent);
    }
    return m;
  }

  if (kind == METRIC_LOAD) {
    format_int_or_dash(telemetry_missing(view) ? -1 : view.loadPercent, m.value, sizeof(m.value));
    m.label = "Load";
    m.unit = "%";
    m.accent = load_color(view);
    m.fill = load_fill(view);
    if (m.accent == kRed) {
      m.stateText = "OVERLOAD";
      m.stateColor = kRedText;
      m.state = STATE_HIGH_LOAD;
    } else if (m.accent == kOrange) {
      m.stateText = "HIGH LOAD";
      m.stateColor = kOrangeText;
      m.state = STATE_HIGH_LOAD;
    } else if (m.accent == kBlue) {
      m.stateText = "LOW";
      m.stateColor = kBlue;
    } else if (m.accent == kGray || m.accent == kPurple) {
      m.stateText = (m.accent == kPurple) ? "UNAVAILABLE" : "STALE";
      m.stateColor = state_text_color(m.accent);
      m.state = STATE_STALE;
    } else {
      m.stateText = "NORMAL";
      m.stateColor = 0x9bb2a0;
    }
    return m;
  }

  if (kind == METRIC_INPUT) {
    format_input_voltage(telemetry_missing(view) ? 0.0f : view.inputVoltage, m.value, sizeof(m.value));
    m.label = "Input";
    m.unit = "V";
    m.accent = input_color(view);
    m.fill = input_fill(view);
    if (m.accent == kRed) {
      m.stateText = view.onBattery ? "GRID OFFLINE" : "INPUT LOST";
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
    } else if (m.accent == kGray || m.accent == kPurple) {
      m.stateText = (m.accent == kPurple) ? "UNAVAILABLE" : "STALE";
      m.stateColor = state_text_color(m.accent);
      m.state = STATE_STALE;
    } else {
      m.stateText = "GRID ONLINE";
      m.stateColor = 0x9bb2a0;
    }
    return m;
  }

  format_int_or_dash(telemetry_missing(view) ? -1 : view.nutClientCount, m.value, sizeof(m.value));
  m.label = "NUT";
  m.unit = "client";
  m.accent = nut_color(view);
  m.fill = nut_fill(view);
  if (m.accent == kOrange) {
    m.stateText = "NO CLIENTS";
    m.stateColor = kOrangeText;
  } else if (m.accent == kGray || m.accent == kPurple) {
    m.stateText = (m.accent == kPurple) ? "UNAVAILABLE" : "STALE";
    m.stateColor = state_text_color(m.accent);
    m.state = STATE_STALE;
  } else {
    m.stateText = "CLIENTS";
    m.stateColor = kBlue;
  }
  return m;
}

static MetricKind choose_hero_metric(const LedcardsInterfaceNutView &view) {
  if (telemetry_missing(view)) return METRIC_NUT;

  if (view.onBattery) {
    if (view.runtimeSeconds >= 0 && view.runtimeSeconds < 120) return METRIC_TTE;
    if (view.lowBattery || (view.batteryPercent >= 0 && view.batteryPercent < 20)) return METRIC_BATTERY;
    if (view.runtimeSeconds >= 0) return METRIC_TTE;
    if (view.loadPercent >= 70) return METRIC_LOAD;
    return METRIC_TTE;
  }

  if (view.lowBattery || (view.batteryPercent >= 0 && view.batteryPercent < 20)) return METRIC_BATTERY;
  if (view.loadPercent >= 70) return METRIC_LOAD;
  if (view.inputVoltage > 0.0f && view.inputVoltage < 210.0f) return METRIC_INPUT;
  return METRIC_BATTERY;
}

// Ring order follows the physical device-reference loop, not a list-style
// promote-to-front stack. Internal slots are:
//   slot 0 HERO, slot 1 top-right, slot 2 bottom-right,
//   slot 3 bottom-left, slot 4 top-left.
// Forward path (reference >>Hero>>): HERO -> top-right -> bottom-right ->
//   bottom-left -> top-left -> HERO.
// Reverse path (reference <<Hero<<): HERO -> top-left -> bottom-left ->
//   bottom-right -> top-right -> HERO.
// Promoting a metric chooses the shorter of those two bidirectional paths so
// right-side cards do not travel around the whole loop just to reach HERO.
static int find_metric_slot_in_order(const MetricKind order[5], MetricKind kind) {
  for (int i = 0; i < 5; ++i) {
    if (order[i] == kind) return i;
  }
  return -1;
}

static int ring_direction_to_hero(int pos) {
  if (pos <= 0) return 0;
  int forwardSteps = (5 - pos) % 5;
  int reverseSteps = pos;
  return forwardSteps <= reverseSteps ? 1 : -1;
}

static uint8_t ring_step_slot(uint8_t slot, int direction) {
  if (direction >= 0) return static_cast<uint8_t>((slot + 1) % 5);
  return static_cast<uint8_t>((slot + 4) % 5);
}

static uint8_t ring_steps_to_hero(int pos, int direction) {
  if (pos <= 0 || direction == 0) return 0;
  return static_cast<uint8_t>(direction > 0 ? (5 - pos) % 5 : pos);
}

static void rotate_order_to_hero(const MetricKind source[5], MetricKind kind, MetricKind dest[5]) {
  for (int i = 0; i < 5; ++i) dest[i] = source[i];
  int pos = find_metric_slot_in_order(source, kind);
  int direction = ring_direction_to_hero(pos);
  uint8_t steps = ring_steps_to_hero(pos, direction);
  if (steps == 0) return;

  for (int i = 0; i < 5; ++i) {
    int destSlot = i;
    for (uint8_t step = 0; step < steps; ++step) {
      destSlot = ring_step_slot(static_cast<uint8_t>(destSlot), direction);
    }
    dest[destSlot] = source[i];
  }
}

static void rotate_metric_to_hero(MetricKind kind) {
  MetricKind rotated[5];
  rotate_order_to_hero(metricOrder, kind, rotated);
  for (int i = 0; i < 5; ++i) metricOrder[i] = rotated[i];
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
    lastHeroSwapMs = view.nowMillis;
    return candidate;
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

static void on_tile_clicked(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED || ringAnimationActive) return;
  uintptr_t raw = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
  if (raw >= 5) return;
  touchHeroOverrideMetric = static_cast<MetricKind>(raw);
  uint32_t now = millis();
  touchHeroOverrideUntilMs = now + kTouchHeroOverrideMs;
  touchHeroOverrideActive = true;
  if (hasLastRenderedView) start_ring_transition(lv_screen_active(), touchHeroOverrideMetric, lastRenderedView);
}

static lv_obj_t *tile(lv_obj_t *screen, int x, int y, const MetricRender &m, bool clickable = true) {
  lv_obj_t *t = box(screen, x, y, 142, 46, 7, m.fill, 0x141e1b);
  box(t, 7, 8, 5, 28, 3, m.accent, 0);
  lv_obj_t *name_l = label(t, m.label, 76, 6, 55, &lv_font_montserrat_12, 0xc9d0c9);
  lv_obj_set_style_text_align(name_l, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_t *unit_l = label(t, m.unit, 78, 23, 53, &lv_font_montserrat_12, 0x87918c);
  lv_obj_set_style_text_align(unit_l, LV_TEXT_ALIGN_RIGHT, 0);
  // Keep the metric value visually on top of the right-side label/unit objects.
  // Mini-card Runtime is minutes-only because clock-form values such as "06:24"
  // are too tight for this physical TFT/card geometry.
  label(t, m.value, 20, 8, 58, &ps_font_ddin_condensed_bold_40, 0xf5f6f2);
  if (clickable) {
    lv_obj_t *hit = lv_obj_create(t);
    lv_obj_remove_style_all(hit);
    lv_obj_set_size(hit, 142, 46);
    lv_obj_set_pos(hit, 0, 0);
    lv_obj_add_flag(hit, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_scrollbar_mode(hit, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(hit, on_tile_clicked, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<uintptr_t>(m.kind)));
  }
  return t;
}

static SlotPosition slot_position(int slot) {
  switch (slot) {
    case 0: return {43, 57};
    case 1: return {166, 124};
    case 2: return {166, 182};
    case 3: return {12, 182};
    default: return {12, 124};
  }
}

static void place_ghost_in_slot(lv_obj_t *obj, uint8_t slot) {
  SlotPosition pos = slot_position(slot == 0 ? 4 : slot);
  lv_obj_set_pos(obj, pos.x, pos.y);
}

static void place_between(lv_obj_t *obj, const SlotPosition &from, const SlotPosition &to, int32_t progress) {
  lv_obj_set_pos(obj,
                 from.x + ((to.x - from.x) * progress) / 1000,
                 from.y + ((to.y - from.y) * progress) / 1000);
}

static void place_ghost_through_hero_lane(lv_obj_t *obj, uint8_t fromSlot, uint8_t toSlot, int32_t segProgress) {
  SlotPosition hero = slot_position(0);
  if (fromSlot == 0) {
    // Device-reference top segment: the current HERO exits horizontally first,
    // then drops into the adjacent mini-card column.
    SlotPosition to = slot_position(toSlot);
    SlotPosition edge = {to.x, hero.y};
    if (segProgress < 500) {
      place_between(obj, hero, edge, segProgress * 2);
    } else {
      place_between(obj, edge, to, (segProgress - 500) * 2);
    }
    return;
  }

  if (toSlot == 0) {
    // The incoming mini-card rises to the top lane first, then scrolls
    // horizontally into HERO instead of fading in place.
    SlotPosition from = slot_position(fromSlot);
    SlotPosition edge = {from.x, hero.y};
    if (segProgress < 500) {
      place_between(obj, from, edge, segProgress * 2);
    } else {
      place_between(obj, edge, hero, (segProgress - 500) * 2);
    }
  }
}

static void place_ghost_between_slots(lv_obj_t *obj, uint8_t fromSlot, uint8_t toSlot, int32_t segProgress) {
  if (fromSlot == 0 && toSlot == 0) {
    place_ghost_in_slot(obj, 4);
    return;
  }
  if (fromSlot == 0 || toSlot == 0) {
    place_ghost_through_hero_lane(obj, fromSlot, toSlot, segProgress);
    return;
  }
  SlotPosition from = slot_position(fromSlot);
  SlotPosition to = slot_position(toSlot);
  place_between(obj, from, to, segProgress);
}

static void anim_set_chain_progress(void *var, int32_t progress) {
  RingGhostAnim *anim = static_cast<RingGhostAnim *>(var);
  if (!anim || !anim->obj || anim->steps == 0) return;

  int32_t scaled = progress * anim->steps;
  uint8_t segment = scaled >= 1000 * anim->steps ? anim->steps - 1 : scaled / 1000;
  int32_t segProgress = scaled - (int32_t)segment * 1000;
  if (segProgress < 0) segProgress = 0;
  if (segProgress > 1000) segProgress = 1000;

  uint8_t fromSlot = anim->path[segment];
  uint8_t toSlot = anim->path[segment + 1];
  place_ghost_between_slots(anim->obj, fromSlot, toSlot, segProgress);

  int32_t opa = anim->baseOpa;
  if (fromSlot == 0 && toSlot != 0) {
    opa = (anim->baseOpa * segProgress) / 1000;
  } else if (fromSlot != 0 && toSlot == 0) {
    opa = (anim->baseOpa * (1000 - segProgress)) / 1000;
  }
  lv_obj_set_style_opa(anim->obj, opa, 0);
}

static void finish_ring_animation_async(void *) {
  lv_obj_t *screen = lv_screen_active();
  if (ringAnimationOverlay) {
    lv_obj_delete(ringAnimationOverlay);
    ringAnimationOverlay = nullptr;
  }
  ringAnimationActive = false;
  LedcardsInterfaceNutView view = hasPendingAnimationView ? pendingAnimationView : lastRenderedView;
  hasPendingAnimationView = false;
  draw_nut_home_static(screen, view);
}

static void finish_ring_animation(lv_anim_t *) {
  lv_async_call(finish_ring_animation_async, nullptr);
}

static void animate_ghost_chain(RingGhostAnim *anim, bool finish) {
  if (!anim || anim->steps == 0) return;
  // Keep perceived speed constant: a touch on the adjacent mini-card moves one
  // ring segment in 252 ms, while farther cards get proportionally more time.
  // A fixed whole-transition duration made 2/3/4-segment moves accelerate too much.
  constexpr uint32_t kRingAnimationStepMs = 252;
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, anim);
  lv_anim_set_values(&a, 0, 1000);
  lv_anim_set_duration(&a, kRingAnimationStepMs * anim->steps);
  lv_anim_set_exec_cb(&a, anim_set_chain_progress);
  if (finish) lv_anim_set_completed_cb(&a, finish_ring_animation);
  lv_anim_start(&a);
}

static void start_ring_transition(lv_obj_t *screen, MetricKind target, const LedcardsInterfaceNutView &view) {
  if (ringAnimationActive) {
    pendingAnimationView = view;
    hasPendingAnimationView = true;
    return;
  }
  if (target == metricOrder[0]) {
    draw_nut_home_static(screen, view);
    return;
  }

  MetricKind oldOrder[5];
  MetricKind newOrder[5];
  for (int i = 0; i < 5; ++i) oldOrder[i] = metricOrder[i];
  rotate_order_to_hero(oldOrder, target, newOrder);
  if (newOrder[0] == oldOrder[0]) {
    draw_nut_home_static(screen, view);
    return;
  }

  const LedcardsInterfaceNutView overlayView = hasLastRenderedView ? lastRenderedView : view;
  for (int i = 0; i < 5; ++i) metricOrder[i] = newOrder[i];
  pendingAnimationView = view;
  hasPendingAnimationView = true;
  ringAnimationActive = true;

  if (ringAnimationOverlay) lv_obj_delete(ringAnimationOverlay);
  ringAnimationOverlay = lv_obj_create(screen);
  lv_obj_remove_style_all(ringAnimationOverlay);
  lv_obj_set_pos(ringAnimationOverlay, 0, 0);
  lv_obj_set_size(ringAnimationOverlay, 320, 240);
  lv_obj_add_flag(ringAnimationOverlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_scrollbar_mode(ringAnimationOverlay, LV_SCROLLBAR_MODE_OFF);

  int targetOldSlot = find_metric_slot_in_order(oldOrder, target);
  int ringDirection = ring_direction_to_hero(targetOldSlot);
  uint8_t chainSteps = ring_steps_to_hero(targetOldSlot, ringDirection);
  for (int oldSlot = 0; oldSlot < 5; ++oldSlot) {
    MetricKind kind = oldOrder[oldSlot];
    RingGhostAnim &anim = ringGhostAnimations[oldSlot];
    anim.steps = chainSteps;
    anim.baseOpa = LV_OPA_80;
    anim.path[0] = static_cast<uint8_t>(oldSlot);
    for (uint8_t step = 1; step <= chainSteps; ++step) {
      anim.path[step] = ring_step_slot(anim.path[step - 1], ringDirection);
    }
    uint8_t firstVisibleSlot = anim.path[0] == 0 ? anim.path[1] : anim.path[0];
    SlotPosition start = slot_position(firstVisibleSlot);
    lv_obj_t *ghost = tile(ringAnimationOverlay, start.x, start.y, metric_for(kind, overlayView, true), false);
    anim.obj = ghost;
    lv_obj_set_style_opa(ghost, anim.path[0] == 0 ? 0 : anim.baseOpa, 0);
    animate_ghost_chain(&anim, oldSlot == 4);
  }
}

static int hero_label_x(const MetricRender &hero) {
  size_t len = strlen(hero.value);
  if (len >= 5) return 160;
  if (len >= 3) return 124;
  return 96;
}

static void draw_nut_home_static(lv_obj_t *screen, const LedcardsInterfaceNutView &view) {
  lastRenderedView = view;
  hasLastRenderedView = true;
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, C(0x040607), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  set_no_border(screen);

  MetricKind heroKind = metricOrder[0];
  MetricRender hero = metric_for(heroKind, view);
  top_status(screen, view, hero.accent);

  int label_x = hero_label_x(hero);
  box(screen, 19, 33, 7, 76, 4, hero.accent, 0);
  label(screen, hero.value, 43, 33, 120, &ps_font_ddin_condensed_bold_60, 0xf5f6f2);
  label(screen, hero.label, label_x, 33, 72, &lv_font_montserrat_12, 0xbeb8a0);
  label(screen, hero.unit, label_x, 63, 72, &lv_font_montserrat_12, 0x968f78);
  label(screen, hero.stateText, 45, 96, 142, &lv_font_montserrat_14, hero.stateColor);
  chart_button(screen);

  const int xs[4] = {166, 166, 12, 12};
  const int ys[4] = {124, 182, 182, 124};
  for (int i = 1; i < 5; ++i) {
    tile(screen, xs[i - 1], ys[i - 1], metric_for(metricOrder[i], view, true));
  }
}

static void render_nut_home(lv_obj_t *screen, const LedcardsInterfaceNutView &view) {
  if (ringAnimationActive) {
    pendingAnimationView = view;
    hasPendingAnimationView = true;
    return;
  }
  MetricKind target = accepted_hero_metric(view);
  if (target != metricOrder[0]) {
    start_ring_transition(screen, target, view);
    return;
  }
  draw_nut_home_static(screen, view);
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
