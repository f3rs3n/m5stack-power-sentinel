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
LV_FONT_DECLARE(ps_icon_status_14);

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
  lv_obj_t *heroObj;
  uint8_t steps;
  uint8_t path[5];
  uint16_t segmentLength[4];
  uint16_t totalLength;
  uint8_t baseOpa;
};

constexpr int kHeroCardX = 12;
constexpr int kHeroCardY = 25;
constexpr int kHeroCardW = 296;
constexpr int kHeroCardH = 91;

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
  // NUT can keep LB asserted while line power is back (OL CHRG LB) and the
  // battery is recovering. In that online state, avoid flapping between a red
  // LOW BATTERY alarm and the normal percent bucket as LB clears/reasserts.
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
  if (view.batteryPercent < 20) return "CHARGING";
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

  if (view.batteryPercent >= 0 && view.batteryPercent < 20) return METRIC_BATTERY;
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

static lv_obj_t *icon_label(lv_obj_t *parent, const char *text, int x, int y, int w, uint32_t color) {
  return label(parent, text, x, y, w, &ps_icon_status_14, color);
}

static const char *local_battery_icon(const LedcardsInterfaceNutView &view) {
  if (view.localBatteryCharging) return "\xF3\xB0\x82\x84";  // nf-md-battery_charging
  if (!view.localBatteryKnown || view.localBatteryPercent < 10) return "\xF3\xB0\x82\x83";  // nf-md-battery_alert
  if (view.localBatteryPercent < 30) return "\xF3\xB1\x8A\xA1";  // nf-md-battery_low
  return "\xF3\xB0\x81\xB9";  // nf-md-battery
}

static void top_status(lv_obj_t *screen, const LedcardsInterfaceNutView &view) {
  constexpr uint32_t kStatusOn = 0xaab0ac;
  constexpr uint32_t kStatusOff = 0x66706b;
  icon_label(screen, view.moduleLanConnected ? "\xF3\xB0\x8C\x98" : "\xF3\xB0\x8C\x99", 6, 2, 16,
             view.moduleLanConnected ? kStatusOn : kStatusOff);  // nf-md-lan_connect / nf-md-lan_disconnect
  icon_label(screen, view.wifiConnected ? "\xF3\xB0\x96\xA9" : "\xF3\xB0\x96\xAA", 22, 2, 16,
             view.wifiConnected ? kStatusOn : kStatusOff);  // wifi / wifi_off
  icon_label(screen, view.linkOk ? "\xF3\xB0\x8C\xB9" : "\xF3\xB0\x8C\xBA", 38, 2, 16,
             view.linkOk ? kStatusOn : kStatusOff);  // link_variant / link_variant_off

  uint8_t pageCount = view.pageCount == 0 ? 1 : view.pageCount;
  if (pageCount > 5) pageCount = 5;
  uint8_t pageIndex = view.pageIndex < pageCount ? view.pageIndex : 0;
  int totalW = static_cast<int>(pageCount) * 7 - 3;
  int x = 160 - totalW / 2;
  for (uint8_t i = 0; i < pageCount; ++i) {
    bool active = i == pageIndex;
    box(screen, x + static_cast<int>(i) * 7, 8, active ? 5 : 4, active ? 5 : 4, 3, active ? 0xf5f6f2 : 0x4d5450, 0);
  }

  const char *timeText = (view.moduleTimeHhmm[0] != '\0') ? view.moduleTimeHhmm : "--:--";
  lv_obj_t *time = label(screen, timeText, 234, 2, 48, &lv_font_montserrat_12, kStatusOn);
  lv_obj_set_style_text_align(time, LV_TEXT_ALIGN_RIGHT, 0);
  icon_label(screen, local_battery_icon(view), 292, 2, 20, view.localBatteryKnown ? kStatusOn : kStatusOff);
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

static lv_obj_t *hero_card(lv_obj_t *parent, int x, int y, const MetricRender &hero, bool includeChart = true) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_remove_style_all(card);
  lv_obj_set_pos(card, x, y);
  lv_obj_set_size(card, kHeroCardW, kHeroCardH);
  lv_obj_set_style_bg_opa(card, LV_OPA_TRANSP, 0);
  lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);

  int label_x = 84;
  size_t len = strlen(hero.value);
  if (len >= 5) label_x = 148;
  else if (len >= 3) label_x = 112;

  box(card, 7, 8, 7, 76, 4, hero.accent, 0);
  label(card, hero.value, 31, 8, 120, &ps_font_ddin_condensed_bold_60, 0xf5f6f2);
  label(card, hero.label, label_x, 8, 72, &lv_font_montserrat_12, 0xbeb8a0);
  label(card, hero.unit, label_x, 38, 72, &lv_font_montserrat_12, 0x968f78);
  label(card, hero.stateText, 33, 71, 142, &lv_font_montserrat_14, hero.stateColor);
  if (includeChart) chart_icon(card, 262, 8);
  return card;
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

static uint16_t segment_visual_length(uint8_t fromSlot, uint8_t toSlot) {
  if (fromSlot == 0 || toSlot == 0) {
    // The HERO slot is visually a wide full-card slide; weight it heavier than
    // its raw pixel width so it has the inertia of a larger card in the chain.
    return 360;
  }
  SlotPosition from = slot_position(fromSlot);
  SlotPosition to = slot_position(toSlot);
  int dx = from.x > to.x ? from.x - to.x : to.x - from.x;
  int dy = from.y > to.y ? from.y - to.y : to.y - from.y;
  return static_cast<uint16_t>(dx > dy ? dx : dy);
}

static void finalize_anim_path_lengths(RingGhostAnim &anim) {
  anim.totalLength = 0;
  for (uint8_t step = 0; step < anim.steps; ++step) {
    anim.segmentLength[step] = segment_visual_length(anim.path[step], anim.path[step + 1]);
    anim.totalLength += anim.segmentLength[step];
  }
}

static uint32_t chain_duration_ms(uint16_t maxPathLength) {
  // About 1.2 ms per weighted pixel: adjacent HERO crossings land around
  // 430 ms, while two-segment chain moves land around 600-650 ms.
  uint32_t duration = static_cast<uint32_t>(maxPathLength) * 12 / 10;
  if (duration < 360) duration = 360;
  if (duration > 720) duration = 720;
  return duration;
}

static void place_between(lv_obj_t *obj, const SlotPosition &from, const SlotPosition &to, int32_t progress) {
  lv_obj_set_pos(obj,
                 from.x + ((to.x - from.x) * progress) / 1000,
                 from.y + ((to.y - from.y) * progress) / 1000);
}

static void place_hero_between_slots(lv_obj_t *obj, uint8_t fromSlot, uint8_t toSlot, int32_t segProgress) {
  int fromX = kHeroCardX;
  int toX = kHeroCardX;
  if (fromSlot == 0 && toSlot == 1) {
    toX = kHeroCardX + kHeroCardW;
  } else if (fromSlot == 0 && toSlot == 4) {
    toX = kHeroCardX - kHeroCardW;
  } else if (fromSlot == 1 && toSlot == 0) {
    fromX = kHeroCardX + kHeroCardW;
  } else if (fromSlot == 4 && toSlot == 0) {
    fromX = kHeroCardX - kHeroCardW;
  }
  lv_obj_set_pos(obj, fromX + ((toX - fromX) * segProgress) / 1000, kHeroCardY);
}

static void place_ghost_between_slots(RingGhostAnim *anim, uint8_t fromSlot, uint8_t toSlot, int32_t segProgress) {
  if (!anim || !anim->obj || !anim->heroObj) return;
  if (fromSlot == 0 || toSlot == 0) {
    // HERO is a full-width invisible card region. When a metric crosses it,
    // render that metric as a large hero card that scrolls horizontally;
    // mini-card ghosts never enter the hero area.
    lv_obj_set_style_opa(anim->obj, 0, 0);
    lv_obj_set_style_opa(anim->heroObj, anim->baseOpa, 0);
    place_hero_between_slots(anim->heroObj, fromSlot, toSlot, segProgress);
    return;
  }

  lv_obj_set_style_opa(anim->heroObj, 0, 0);
  lv_obj_set_style_opa(anim->obj, anim->baseOpa, 0);
  SlotPosition from = slot_position(fromSlot);
  SlotPosition to = slot_position(toSlot);
  place_between(anim->obj, from, to, segProgress);
}

static void anim_set_chain_progress(void *var, int32_t progress) {
  RingGhostAnim *anim = static_cast<RingGhostAnim *>(var);
  if (!anim || !anim->obj || !anim->heroObj || anim->steps == 0 || anim->totalLength == 0) return;

  int32_t traveled = (static_cast<int32_t>(anim->totalLength) * progress) / 1000;
  uint8_t segment = 0;
  while (segment + 1 < anim->steps && traveled >= anim->segmentLength[segment]) {
    traveled -= anim->segmentLength[segment];
    ++segment;
  }
  int32_t segLen = anim->segmentLength[segment] ? anim->segmentLength[segment] : 1;
  int32_t segProgress = (traveled * 1000) / segLen;
  if (segProgress < 0) segProgress = 0;
  if (segProgress > 1000) segProgress = 1000;

  uint8_t fromSlot = anim->path[segment];
  uint8_t toSlot = anim->path[segment + 1];
  place_ghost_between_slots(anim, fromSlot, toSlot, segProgress);
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

static uint32_t chain_stagger_delay_ms(int oldSlot, int targetOldSlot, int ringDirection) {
  constexpr uint32_t kChainStaggerStepMs = 18;
  int delayUnits = 0;
  if (ringDirection > 0) {
    delayUnits = (targetOldSlot - oldSlot + 5) % 5;
  } else if (ringDirection < 0) {
    delayUnits = (oldSlot - targetOldSlot + 5) % 5;
  }
  return static_cast<uint32_t>(delayUnits) * kChainStaggerStepMs;
}

static void animate_ghost_chain(RingGhostAnim *anim, uint32_t durationMs, uint32_t delayMs, bool finish) {
  if (!anim || anim->steps == 0 || durationMs == 0) return;
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, anim);
  lv_anim_set_values(&a, 0, 1000);
  lv_anim_set_duration(&a, durationMs);
  lv_anim_set_delay(&a, delayMs);
  lv_anim_set_exec_cb(&a, anim_set_chain_progress);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
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

  // Mask the old static hero lane; animated metrics crossing HERO are drawn as
  // full-size hero cards, while mini-card ghosts remain below this region.
  box(ringAnimationOverlay, 0, kHeroCardY, 320, kHeroCardH, 0, 0x040607, 0);

  int targetOldSlot = find_metric_slot_in_order(oldOrder, target);
  int ringDirection = ring_direction_to_hero(targetOldSlot);
  uint8_t chainSteps = ring_steps_to_hero(targetOldSlot, ringDirection);
  uint16_t maxPathLength = 0;
  for (int oldSlot = 0; oldSlot < 5; ++oldSlot) {
    MetricKind kind = oldOrder[oldSlot];
    RingGhostAnim &anim = ringGhostAnimations[oldSlot];
    anim.steps = chainSteps;
    anim.baseOpa = LV_OPA_80;
    anim.path[0] = static_cast<uint8_t>(oldSlot);
    for (uint8_t step = 1; step <= chainSteps; ++step) {
      anim.path[step] = ring_step_slot(anim.path[step - 1], ringDirection);
    }
    finalize_anim_path_lengths(anim);
    if (anim.totalLength > maxPathLength) maxPathLength = anim.totalLength;

    uint8_t firstMiniSlot = anim.path[0] == 0 ? anim.path[1] : anim.path[0];
    if (firstMiniSlot == 0) firstMiniSlot = anim.path[chainSteps];
    SlotPosition start = slot_position(firstMiniSlot == 0 ? 4 : firstMiniSlot);
    lv_obj_t *ghost = tile(ringAnimationOverlay, start.x, start.y, metric_for(kind, overlayView, true), false);
    lv_obj_t *heroGhost = hero_card(ringAnimationOverlay, kHeroCardX, kHeroCardY, metric_for(kind, overlayView), true);
    anim.obj = ghost;
    anim.heroObj = heroGhost;
    lv_obj_set_style_opa(ghost, 0, 0);
    lv_obj_set_style_opa(heroGhost, 0, 0);
  }

  uint32_t durationMs = chain_duration_ms(maxPathLength);
  uint32_t maxDelayMs = 0;
  int finishSlot = 0;
  for (int oldSlot = 0; oldSlot < 5; ++oldSlot) {
    uint32_t delayMs = chain_stagger_delay_ms(oldSlot, targetOldSlot, ringDirection);
    if (delayMs >= maxDelayMs) {
      maxDelayMs = delayMs;
      finishSlot = oldSlot;
    }
  }
  for (int oldSlot = 0; oldSlot < 5; ++oldSlot) {
    uint32_t delayMs = chain_stagger_delay_ms(oldSlot, targetOldSlot, ringDirection);
    animate_ghost_chain(&ringGhostAnimations[oldSlot], durationMs, delayMs, oldSlot == finishSlot);
  }
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
  top_status(screen, view);

  hero_card(screen, kHeroCardX, kHeroCardY, hero, true);

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
