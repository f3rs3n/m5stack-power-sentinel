#pragma once

#include <stdint.h>
#include <string.h>

struct ProxmoxAmbientView {
  bool enabled;
  bool implemented;
  bool hasLiveData;
  char condition[16];
  char status[24];
};

struct ProxmoxAmbientCard {
  char label[12];
  char value[16];
  char unit[8];
  char stateText[24];
  char stateClass[16];
  char visualClass[16];
};

struct ProxmoxAmbientPageModel {
  char condition[16];
  char telemetryState[24];
  char heroTitle[16];
  char heroValue[16];
  char heroDisplayValue[16];
  char heroDetail[32];
  char visualClass[16];
  uint8_t cardCount;
  uint8_t heroCardIndex;
  ProxmoxAmbientCard cards[5];
};

inline void proxmoxAmbientCopy(char *dst, size_t dstSize, const char *src) {
  if (!dst || dstSize == 0) return;
  if (!src) src = "";
  strncpy(dst, src, dstSize - 1);
  dst[dstSize - 1] = '\0';
}

inline const char *proxmoxAmbientCondition(const ProxmoxAmbientView &view) {
  if (!view.enabled || !view.implemented) return "unavailable";
  if (view.condition[0] == '\0') return "unavailable";
  return view.condition;
}

inline const char *proxmoxAmbientTelemetryState(const ProxmoxAmbientView &view) {
  if (!view.enabled) return "disabled";
  if (!view.implemented) return "unavailable";
  if (view.status[0] != '\0') return view.status;
  if (!view.hasLiveData) return "not_observed";
  return "observed";
}

inline const char *proxmoxAmbientCardVisualClass(const ProxmoxAmbientView &view) {
  const char *condition = proxmoxAmbientCondition(view);
  if (strcmp(condition, "critical") == 0) return "red";
  if (strcmp(condition, "warning") == 0) return "orange";
  if (strcmp(condition, "stale") == 0) return "gray";
  if (strcmp(condition, "unavailable") == 0) return "purple";
  return "blue";
}

inline void fillProxmoxAmbientPlaceholderCard(ProxmoxAmbientCard &card, const ProxmoxAmbientView &view, const char *label, const char *unit) {
  proxmoxAmbientCopy(card.label, sizeof(card.label), label);
  proxmoxAmbientCopy(card.value, sizeof(card.value), "--");
  proxmoxAmbientCopy(card.unit, sizeof(card.unit), unit);
  proxmoxAmbientCopy(card.stateText, sizeof(card.stateText), "PLACEHOLDER");
  proxmoxAmbientCopy(card.stateClass, sizeof(card.stateClass), proxmoxAmbientCondition(view));
  proxmoxAmbientCopy(card.visualClass, sizeof(card.visualClass), proxmoxAmbientCardVisualClass(view));
}

inline uint8_t proxmoxAmbientDefaultHeroCardIndex(const ProxmoxAmbientPageModel &model) {
  (void)model;
  return 0;  // CPU is the default healthy or placeholder-healthy hero.
}

inline void proxmoxAmbientApplyHero(ProxmoxAmbientPageModel &model) {
  if (model.cardCount == 0 || model.heroCardIndex >= model.cardCount) return;
  const ProxmoxAmbientCard &hero = model.cards[model.heroCardIndex];
  proxmoxAmbientCopy(model.heroTitle, sizeof(model.heroTitle), hero.label);
  proxmoxAmbientCopy(model.heroValue, sizeof(model.heroValue), hero.value);
  proxmoxAmbientCopy(model.heroDisplayValue, sizeof(model.heroDisplayValue), hero.value);
  proxmoxAmbientCopy(model.heroDetail, sizeof(model.heroDetail), hero.stateText);
  proxmoxAmbientCopy(model.visualClass, sizeof(model.visualClass), hero.visualClass);
}

inline ProxmoxAmbientPageModel makeProxmoxAmbientPageModel(const ProxmoxAmbientView &view) {
  ProxmoxAmbientPageModel model{};
  proxmoxAmbientCopy(model.condition, sizeof(model.condition), proxmoxAmbientCondition(view));
  proxmoxAmbientCopy(model.telemetryState, sizeof(model.telemetryState), proxmoxAmbientTelemetryState(view));
  model.cardCount = 5;
  fillProxmoxAmbientPlaceholderCard(model.cards[0], view, "CPU", "%");
  fillProxmoxAmbientPlaceholderCard(model.cards[1], view, "RAM", "%");
  fillProxmoxAmbientPlaceholderCard(model.cards[2], view, "Guests", "run");
  fillProxmoxAmbientPlaceholderCard(model.cards[3], view, "Storage", "%");
  fillProxmoxAmbientPlaceholderCard(model.cards[4], view, "Network", "%");
  model.heroCardIndex = proxmoxAmbientDefaultHeroCardIndex(model);
  proxmoxAmbientApplyHero(model);
  return model;
}

inline uint8_t proxmoxReducedCardCount(const ProxmoxAmbientPageModel &model) {
  if (model.cardCount == 0) return 0;
  if (model.heroCardIndex >= model.cardCount) return model.cardCount;
  return static_cast<uint8_t>(model.cardCount - 1);
}

inline const ProxmoxAmbientCard *proxmoxReducedCardAt(const ProxmoxAmbientPageModel &model, uint8_t reducedIndex) {
  uint8_t seen = 0;
  for (uint8_t i = 0; i < model.cardCount; ++i) {
    if (i == model.heroCardIndex) continue;
    if (seen == reducedIndex) return &model.cards[i];
    ++seen;
  }
  return nullptr;
}
