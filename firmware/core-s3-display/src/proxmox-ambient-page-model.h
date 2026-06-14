#pragma once

#include <stdio.h>
#include <stdint.h>
#include <string.h>

struct ProxmoxAmbientView {
  bool enabled;
  bool implemented;
  bool hasLiveData;
  char condition[16];
  char status[24];
  int cpuPercent = -1;
  char cpuCondition[16] = "";
  int ramPercent = -1;
  char ramCondition[16] = "";
  int storagePercent = -1;
  char storageCondition[16] = "";
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

inline const char *proxmoxAmbientConditionVisualClass(const char *condition) {
  if (strcmp(condition, "critical") == 0) return "red";
  if (strcmp(condition, "warning") == 0) return "orange";
  if (strcmp(condition, "stale") == 0) return "gray";
  if (strcmp(condition, "unavailable") == 0) return "purple";
  return "blue";
}

inline const char *proxmoxAmbientCpuStateText(const char *condition, bool hasValue) {
  if (!hasValue) return "PLACEHOLDER";
  if (strcmp(condition, "critical") == 0) return "CPU CRIT";
  if (strcmp(condition, "warning") == 0) return "CPU WARN";
  return "CPU OK";
}

inline const char *proxmoxAmbientRamStateText(const char *condition, bool hasValue) {
  if (!hasValue) return "PLACEHOLDER";
  if (strcmp(condition, "critical") == 0) return "RAM CRIT";
  if (strcmp(condition, "warning") == 0) return "RAM WARN";
  return "RAM OK";
}

inline const char *proxmoxAmbientStorageStateText(const char *condition, bool hasValue) {
  if (!hasValue) return "PLACEHOLDER";
  if (strcmp(condition, "critical") == 0) return "STOR CRIT";
  if (strcmp(condition, "warning") == 0) return "STOR WARN";
  return "STOR OK";
}

inline uint8_t proxmoxAmbientConditionRank(const char *condition) {
  if (strcmp(condition, "critical") == 0) return 3;
  if (strcmp(condition, "warning") == 0) return 2;
  if (strcmp(condition, "stale") == 0 || strcmp(condition, "unavailable") == 0) return 1;
  return 0;
}

inline void fillProxmoxAmbientPlaceholderCard(ProxmoxAmbientCard &card, const ProxmoxAmbientView &view, const char *label, const char *unit) {
  proxmoxAmbientCopy(card.label, sizeof(card.label), label);
  proxmoxAmbientCopy(card.value, sizeof(card.value), "--");
  proxmoxAmbientCopy(card.unit, sizeof(card.unit), unit);
  proxmoxAmbientCopy(card.stateText, sizeof(card.stateText), "PLACEHOLDER");
  proxmoxAmbientCopy(card.stateClass, sizeof(card.stateClass), proxmoxAmbientCondition(view));
  proxmoxAmbientCopy(card.visualClass, sizeof(card.visualClass), proxmoxAmbientCardVisualClass(view));
}

inline void fillProxmoxAmbientCpuCard(ProxmoxAmbientCard &card, const ProxmoxAmbientView &view) {
  bool hasValue = view.cpuPercent >= 0;
  const char *condition = view.cpuCondition[0] == '\0' ? "healthy" : view.cpuCondition;
  proxmoxAmbientCopy(card.label, sizeof(card.label), "CPU");
  if (hasValue) snprintf(card.value, sizeof(card.value), "%d", view.cpuPercent);
  else proxmoxAmbientCopy(card.value, sizeof(card.value), "--");
  proxmoxAmbientCopy(card.unit, sizeof(card.unit), "%");
  proxmoxAmbientCopy(card.stateText, sizeof(card.stateText), proxmoxAmbientCpuStateText(condition, hasValue));
  proxmoxAmbientCopy(card.stateClass, sizeof(card.stateClass), hasValue ? condition : proxmoxAmbientCondition(view));
  proxmoxAmbientCopy(card.visualClass, sizeof(card.visualClass), hasValue ? proxmoxAmbientConditionVisualClass(condition) : proxmoxAmbientCardVisualClass(view));
}

inline void fillProxmoxAmbientRamCard(ProxmoxAmbientCard &card, const ProxmoxAmbientView &view) {
  bool hasValue = view.ramPercent >= 0;
  const char *condition = view.ramCondition[0] == '\0' ? "healthy" : view.ramCondition;
  proxmoxAmbientCopy(card.label, sizeof(card.label), "RAM");
  if (hasValue) snprintf(card.value, sizeof(card.value), "%d", view.ramPercent);
  else proxmoxAmbientCopy(card.value, sizeof(card.value), "--");
  proxmoxAmbientCopy(card.unit, sizeof(card.unit), "%");
  proxmoxAmbientCopy(card.stateText, sizeof(card.stateText), proxmoxAmbientRamStateText(condition, hasValue));
  proxmoxAmbientCopy(card.stateClass, sizeof(card.stateClass), hasValue ? condition : proxmoxAmbientCondition(view));
  proxmoxAmbientCopy(card.visualClass, sizeof(card.visualClass), hasValue ? proxmoxAmbientConditionVisualClass(condition) : proxmoxAmbientCardVisualClass(view));
}

inline void fillProxmoxAmbientStorageCard(ProxmoxAmbientCard &card, const ProxmoxAmbientView &view) {
  bool hasValue = view.storagePercent >= 0;
  const char *condition = view.storageCondition[0] == '\0' ? "healthy" : view.storageCondition;
  proxmoxAmbientCopy(card.label, sizeof(card.label), "Storage");
  if (hasValue) snprintf(card.value, sizeof(card.value), "%d", view.storagePercent);
  else proxmoxAmbientCopy(card.value, sizeof(card.value), "--");
  proxmoxAmbientCopy(card.unit, sizeof(card.unit), "%");
  proxmoxAmbientCopy(card.stateText, sizeof(card.stateText), proxmoxAmbientStorageStateText(condition, hasValue));
  proxmoxAmbientCopy(card.stateClass, sizeof(card.stateClass), hasValue ? condition : proxmoxAmbientCondition(view));
  proxmoxAmbientCopy(card.visualClass, sizeof(card.visualClass), hasValue ? proxmoxAmbientConditionVisualClass(condition) : proxmoxAmbientCardVisualClass(view));
}

inline uint8_t proxmoxAmbientHeroPriority(uint8_t cardIndex) {
  if (cardIndex == 3) return 5;
  if (cardIndex == 0) return 4;
  if (cardIndex == 1) return 3;
  if (cardIndex == 4) return 2;
  return 1;
}

inline uint8_t proxmoxAmbientDefaultHeroCardIndex(const ProxmoxAmbientPageModel &model) {
  uint8_t bestIndex = 0;
  uint8_t bestRank = strcmp(model.cards[0].value, "--") == 0 ? 0 : proxmoxAmbientConditionRank(model.cards[0].stateClass);
  for (uint8_t i = 1; i < model.cardCount; ++i) {
    uint8_t rank = strcmp(model.cards[i].value, "--") == 0 ? 0 : proxmoxAmbientConditionRank(model.cards[i].stateClass);
    if (rank > bestRank || (rank > 0 && rank == bestRank && proxmoxAmbientHeroPriority(i) > proxmoxAmbientHeroPriority(bestIndex))) {
      bestRank = rank;
      bestIndex = i;
    }
  }
  return bestIndex;  // CPU wins ties because it appears first, RAM follows it.
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
  fillProxmoxAmbientCpuCard(model.cards[0], view);
  fillProxmoxAmbientRamCard(model.cards[1], view);
  fillProxmoxAmbientPlaceholderCard(model.cards[2], view, "Guests", "run");
  fillProxmoxAmbientStorageCard(model.cards[3], view);
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
