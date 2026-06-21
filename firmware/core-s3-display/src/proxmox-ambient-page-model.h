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
  int guestRunning = -1;
  int guestTotal = -1;
  char guestCondition[16] = "";
  int storagePercent = -1;
  char storageCondition[16] = "";
  int networkPercent = -1;
  char networkCondition[16] = "";
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

struct ProxmoxAmbientHeroPolicyInput {
  uint8_t currentHeroCardIndex;
  bool touchOverrideActive;
  uint8_t touchOverrideCardIndex;
  uint32_t touchOverrideUntilMs;
  uint32_t nowMillis;
};

struct ProxmoxAmbientHeroPolicyDecision {
  uint8_t acceptedHeroCardIndex;
  bool touchOverrideActive;
};

struct ProxmoxAmbientTouchHeroOverride {
  bool active;
  uint8_t cardIndex;
  uint32_t untilMs;
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
  return "green";
}

inline const char *proxmoxAmbientCpuVisualClass(const char *condition, int percent) {
  if (strcmp(condition, "critical") == 0) return "red";
  if (strcmp(condition, "warning") == 0) return "orange";
  if (strcmp(condition, "stale") == 0) return "gray";
  if (strcmp(condition, "unavailable") == 0 || percent < 0) return "purple";
  if (percent < 10) return "blue";
  if (percent < 60) return "green";
  return "yellow";
}

inline const char *proxmoxAmbientRamVisualClass(const char *condition, int percent) {
  if (strcmp(condition, "critical") == 0) return "red";
  if (strcmp(condition, "warning") == 0) return "orange";
  if (strcmp(condition, "stale") == 0) return "gray";
  if (strcmp(condition, "unavailable") == 0 || percent < 0) return "purple";
  if (percent < 25) return "blue";
  if (percent < 70) return "green";
  return "yellow";
}

inline const char *proxmoxAmbientStorageVisualClass(const char *condition, int percent) {
  if (strcmp(condition, "critical") == 0) return "red";
  if (strcmp(condition, "warning") == 0) return "orange";
  if (strcmp(condition, "stale") == 0) return "gray";
  if (strcmp(condition, "unavailable") == 0 || percent < 0) return "purple";
  if (percent < 40) return "blue";
  if (percent < 70) return "green";
  return "yellow";
}

inline const char *proxmoxAmbientNetworkVisualClass(const char *condition, int percent) {
  if (strcmp(condition, "critical") == 0) return "red";
  if (strcmp(condition, "warning") == 0) return "orange";
  if (strcmp(condition, "stale") == 0) return "gray";
  if (strcmp(condition, "unavailable") == 0 || percent < 0) return "purple";
  if (percent < 5) return "blue";
  if (percent < 40) return "green";
  return "yellow";
}

inline const char *proxmoxAmbientCpuStateText(const char *condition, bool hasValue, int percent) {
  if (!hasValue) return "PLACEHOLDER";
  if (strcmp(condition, "critical") == 0) return "CPU CRIT";
  if (strcmp(condition, "warning") == 0) return "CPU WARN";
  if (percent < 10) return "IDLE";
  if (percent >= 60) return "CPU BUSY";
  return "CPU OK";
}

inline const char *proxmoxAmbientRamStateText(const char *condition, bool hasValue, int percent) {
  if (!hasValue) return "PLACEHOLDER";
  if (strcmp(condition, "critical") == 0) return "RAM CRIT";
  if (strcmp(condition, "warning") == 0) return "RAM WARN";
  if (percent < 25) return "RAM LOW";
  if (percent >= 70) return "RAM HIGH";
  return "RAM OK";
}

inline const char *proxmoxAmbientStorageStateText(const char *condition, bool hasValue, int percent) {
  if (!hasValue) return "PLACEHOLDER";
  if (strcmp(condition, "critical") == 0) return "STOR CRIT";
  if (strcmp(condition, "warning") == 0) return "STOR WARN";
  if (percent < 40) return "STOR LOW";
  if (percent >= 70) return "STOR HIGH";
  return "STOR OK";
}

inline const char *proxmoxAmbientGuestStateText(const char *condition, bool hasValue, int running, int total) {
  if (!hasValue) return "PLACEHOLDER";
  if (strcmp(condition, "critical") == 0) return "GUEST CRIT";
  if (strcmp(condition, "warning") == 0) return "GUEST WARN";
  if (running == 0 && total == 0) return "GUEST EMPTY";
  return "GUEST OK";
}

inline const char *proxmoxAmbientNetworkStateText(const char *condition, bool hasValue, int percent) {
  if (strcmp(condition, "unavailable") == 0) return "NET UNAVAIL";
  if (!hasValue) return "PLACEHOLDER";
  if (strcmp(condition, "critical") == 0) return "NET CRIT";
  if (strcmp(condition, "warning") == 0) return "NET WARN";
  if (percent < 5) return "NET IDLE";
  if (percent >= 40) return "NET BUSY";
  return "NET OK";
}

inline const char *proxmoxAmbientMissingTelemetryStateText(const ProxmoxAmbientView &view) {
  if (!view.enabled) return "DISABLED";
  if (strcmp(proxmoxAmbientTelemetryState(view), "unconfigured") == 0) return "UNCONFIGURED";
  if (strcmp(proxmoxAmbientCondition(view), "stale") == 0) return "STALE";
  if (!view.hasLiveData || strcmp(proxmoxAmbientCondition(view), "unavailable") == 0) return "UNAVAILABLE";
  return "PLACEHOLDER";
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
  bool hasValue = view.hasLiveData && view.cpuPercent >= 0;
  const char *condition = view.cpuCondition[0] == '\0' ? "healthy" : view.cpuCondition;
  proxmoxAmbientCopy(card.label, sizeof(card.label), "CPU");
  if (hasValue) snprintf(card.value, sizeof(card.value), "%d", view.cpuPercent);
  else proxmoxAmbientCopy(card.value, sizeof(card.value), "--");
  proxmoxAmbientCopy(card.unit, sizeof(card.unit), "%");
  proxmoxAmbientCopy(card.stateText, sizeof(card.stateText), hasValue ? proxmoxAmbientCpuStateText(condition, hasValue, view.cpuPercent) : proxmoxAmbientMissingTelemetryStateText(view));
  proxmoxAmbientCopy(card.stateClass, sizeof(card.stateClass), hasValue ? condition : proxmoxAmbientCondition(view));
  proxmoxAmbientCopy(card.visualClass, sizeof(card.visualClass), hasValue ? proxmoxAmbientCpuVisualClass(condition, view.cpuPercent) : proxmoxAmbientCardVisualClass(view));
}

inline void fillProxmoxAmbientRamCard(ProxmoxAmbientCard &card, const ProxmoxAmbientView &view) {
  bool hasValue = view.hasLiveData && view.ramPercent >= 0;
  const char *condition = view.ramCondition[0] == '\0' ? "healthy" : view.ramCondition;
  proxmoxAmbientCopy(card.label, sizeof(card.label), "RAM");
  if (hasValue) snprintf(card.value, sizeof(card.value), "%d", view.ramPercent);
  else proxmoxAmbientCopy(card.value, sizeof(card.value), "--");
  proxmoxAmbientCopy(card.unit, sizeof(card.unit), "%");
  proxmoxAmbientCopy(card.stateText, sizeof(card.stateText), hasValue ? proxmoxAmbientRamStateText(condition, hasValue, view.ramPercent) : proxmoxAmbientMissingTelemetryStateText(view));
  proxmoxAmbientCopy(card.stateClass, sizeof(card.stateClass), hasValue ? condition : proxmoxAmbientCondition(view));
  proxmoxAmbientCopy(card.visualClass, sizeof(card.visualClass), hasValue ? proxmoxAmbientRamVisualClass(condition, view.ramPercent) : proxmoxAmbientCardVisualClass(view));
}

inline void fillProxmoxAmbientGuestCard(ProxmoxAmbientCard &card, const ProxmoxAmbientView &view) {
  bool hasValue = view.hasLiveData && view.guestRunning >= 0 && view.guestTotal >= 0;
  const char *condition = view.guestCondition[0] == '\0' ? "healthy" : view.guestCondition;
  proxmoxAmbientCopy(card.label, sizeof(card.label), "Guests");
  if (hasValue) snprintf(card.value, sizeof(card.value), "%d/%d", view.guestRunning, view.guestTotal);
  else proxmoxAmbientCopy(card.value, sizeof(card.value), "--");
  proxmoxAmbientCopy(card.unit, sizeof(card.unit), "");
  proxmoxAmbientCopy(card.stateText, sizeof(card.stateText), hasValue ? proxmoxAmbientGuestStateText(condition, hasValue, view.guestRunning, view.guestTotal) : proxmoxAmbientMissingTelemetryStateText(view));
  proxmoxAmbientCopy(card.stateClass, sizeof(card.stateClass), hasValue ? condition : proxmoxAmbientCondition(view));
  if (hasValue && view.guestRunning == 0 && view.guestTotal == 0 && strcmp(condition, "healthy") == 0) {
    proxmoxAmbientCopy(card.visualClass, sizeof(card.visualClass), "blue");
  } else {
    proxmoxAmbientCopy(card.visualClass, sizeof(card.visualClass), hasValue ? proxmoxAmbientConditionVisualClass(condition) : proxmoxAmbientCardVisualClass(view));
  }
}

inline void fillProxmoxAmbientNetworkCard(ProxmoxAmbientCard &card, const ProxmoxAmbientView &view) {
  bool hasValue = view.hasLiveData && view.networkPercent >= 0;
  const char *condition = view.networkCondition[0] == '\0' ? "healthy" : view.networkCondition;
  proxmoxAmbientCopy(card.label, sizeof(card.label), "Network");
  if (hasValue) snprintf(card.value, sizeof(card.value), "%d", view.networkPercent);
  else proxmoxAmbientCopy(card.value, sizeof(card.value), "--");
  proxmoxAmbientCopy(card.unit, sizeof(card.unit), "%");
  proxmoxAmbientCopy(card.stateText, sizeof(card.stateText), hasValue || strcmp(condition, "unavailable") == 0 ? proxmoxAmbientNetworkStateText(condition, hasValue, view.networkPercent) : proxmoxAmbientMissingTelemetryStateText(view));
  proxmoxAmbientCopy(card.stateClass, sizeof(card.stateClass), hasValue || strcmp(condition, "unavailable") == 0 ? condition : proxmoxAmbientCondition(view));
  proxmoxAmbientCopy(card.visualClass, sizeof(card.visualClass), hasValue || strcmp(condition, "unavailable") == 0 ? proxmoxAmbientNetworkVisualClass(condition, view.networkPercent) : proxmoxAmbientCardVisualClass(view));
}

inline void fillProxmoxAmbientStorageCard(ProxmoxAmbientCard &card, const ProxmoxAmbientView &view) {
  bool hasValue = view.hasLiveData && view.storagePercent >= 0;
  const char *condition = view.storageCondition[0] == '\0' ? "healthy" : view.storageCondition;
  proxmoxAmbientCopy(card.label, sizeof(card.label), "Storage");
  if (hasValue) snprintf(card.value, sizeof(card.value), "%d", view.storagePercent);
  else proxmoxAmbientCopy(card.value, sizeof(card.value), "--");
  proxmoxAmbientCopy(card.unit, sizeof(card.unit), "%");
  proxmoxAmbientCopy(card.stateText, sizeof(card.stateText), hasValue ? proxmoxAmbientStorageStateText(condition, hasValue, view.storagePercent) : proxmoxAmbientMissingTelemetryStateText(view));
  proxmoxAmbientCopy(card.stateClass, sizeof(card.stateClass), hasValue ? condition : proxmoxAmbientCondition(view));
  proxmoxAmbientCopy(card.visualClass, sizeof(card.visualClass), hasValue ? proxmoxAmbientStorageVisualClass(condition, view.storagePercent) : proxmoxAmbientCardVisualClass(view));
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
    uint8_t rank = proxmoxAmbientConditionRank(model.cards[i].stateClass);
    if (rank > 1 && strcmp(model.cards[i].value, "--") == 0) rank = 0;
    if (rank > bestRank || (rank > 0 && rank == bestRank && proxmoxAmbientHeroPriority(i) > proxmoxAmbientHeroPriority(bestIndex))) {
      bestRank = rank;
      bestIndex = i;
    }
  }
  return bestIndex;  // CPU wins ties because it appears first, RAM follows it.
}

inline int proxmoxAmbientFindSlotForCard(const uint8_t order[5], uint8_t count, uint8_t cardIndex) {
  for (uint8_t i = 0; i < count; ++i) {
    if (order[i] == cardIndex) return i;
  }
  return -1;
}

inline int proxmoxAmbientRingDirectionToHero(int slot, uint8_t count) {
  if (slot <= 0 || count == 0) return 0;
  int forwardSteps = (count - slot) % count;
  int reverseSteps = slot;
  return forwardSteps <= reverseSteps ? 1 : -1;
}

inline uint8_t proxmoxAmbientRingStep(uint8_t slot, int direction, uint8_t count) {
  if (count == 0) return slot;
  if (direction >= 0) return static_cast<uint8_t>((slot + 1) % count);
  return static_cast<uint8_t>((slot + count - 1) % count);
}

inline uint8_t proxmoxAmbientRingStepsToHero(int slot, int direction, uint8_t count) {
  if (slot <= 0 || direction == 0 || count == 0) return 0;
  return static_cast<uint8_t>(direction > 0 ? (count - slot) % count : slot);
}

inline bool proxmoxAmbientRotateSlotOrderToHero(uint8_t order[5], uint8_t count, uint8_t heroCardIndex) {
  if (!order || count == 0 || count > 5) return false;
  int slot = proxmoxAmbientFindSlotForCard(order, count, heroCardIndex);
  int direction = proxmoxAmbientRingDirectionToHero(slot, count);
  uint8_t steps = proxmoxAmbientRingStepsToHero(slot, direction, count);
  if (steps == 0) return false;

  uint8_t source[5]{};
  for (uint8_t i = 0; i < count; ++i) source[i] = order[i];
  for (uint8_t i = 0; i < count; ++i) {
    uint8_t destSlot = i;
    for (uint8_t step = 0; step < steps; ++step) {
      destSlot = proxmoxAmbientRingStep(destSlot, direction, count);
    }
    order[destSlot] = source[i];
  }
  return true;
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

inline bool applyProxmoxAmbientHeroCard(ProxmoxAmbientPageModel &model, uint8_t cardIndex) {
  if (model.cardCount == 0 || cardIndex >= model.cardCount) return false;
  model.heroCardIndex = cardIndex;
  proxmoxAmbientApplyHero(model);
  return true;
}

inline bool proxmoxAmbientModelIsObserved(const ProxmoxAmbientPageModel &model) {
  return strcmp(model.telemetryState, "observed") == 0 &&
         strcmp(model.condition, "stale") != 0 &&
         strcmp(model.condition, "unavailable") != 0;
}

inline bool proxmoxAmbientTouchHeroOverrideStillActive(uint32_t nowMillis, uint32_t untilMs) {
  return (int32_t)(untilMs - nowMillis) > 0;
}

inline ProxmoxAmbientTouchHeroOverride makeProxmoxAmbientTouchHeroOverride(uint8_t cardIndex, uint32_t nowMillis, uint32_t durationMs) {
  ProxmoxAmbientTouchHeroOverride touch{};
  touch.active = true;
  touch.cardIndex = cardIndex;
  touch.untilMs = nowMillis + durationMs;
  return touch;
}

inline ProxmoxAmbientHeroPolicyDecision acceptProxmoxAmbientHeroCard(const ProxmoxAmbientPageModel &model,
                                                                    const ProxmoxAmbientHeroPolicyInput &input) {
  ProxmoxAmbientHeroPolicyDecision decision{};
  decision.acceptedHeroCardIndex = model.heroCardIndex;
  decision.touchOverrideActive = input.touchOverrideActive;

  if (!proxmoxAmbientModelIsObserved(model)) {
    decision.touchOverrideActive = false;
    return decision;
  }

  if (input.touchOverrideActive) {
    if (input.touchOverrideCardIndex < model.cardCount &&
        proxmoxAmbientTouchHeroOverrideStillActive(input.nowMillis, input.touchOverrideUntilMs)) {
      decision.acceptedHeroCardIndex = input.touchOverrideCardIndex;
      return decision;
    }
    decision.touchOverrideActive = false;
  }

  if (input.currentHeroCardIndex < model.cardCount && input.currentHeroCardIndex == model.heroCardIndex) {
    decision.acceptedHeroCardIndex = input.currentHeroCardIndex;
  }
  return decision;
}

inline bool proxmoxAmbientShouldAnimateHeroTransition(const ProxmoxAmbientPageModel &previous,
                                                     const ProxmoxAmbientPageModel &next) {
  if (!proxmoxAmbientModelIsObserved(previous) || !proxmoxAmbientModelIsObserved(next)) return false;
  if (previous.cardCount == 0 || next.cardCount == 0) return false;
  if (previous.heroCardIndex >= previous.cardCount || next.heroCardIndex >= next.cardCount) return false;
  return previous.heroCardIndex != next.heroCardIndex;
}

inline ProxmoxAmbientPageModel makeProxmoxAmbientPageModel(const ProxmoxAmbientView &view) {
  ProxmoxAmbientPageModel model{};
  proxmoxAmbientCopy(model.condition, sizeof(model.condition), proxmoxAmbientCondition(view));
  proxmoxAmbientCopy(model.telemetryState, sizeof(model.telemetryState), proxmoxAmbientTelemetryState(view));
  model.cardCount = 5;
  fillProxmoxAmbientCpuCard(model.cards[0], view);
  fillProxmoxAmbientRamCard(model.cards[1], view);
  fillProxmoxAmbientGuestCard(model.cards[2], view);
  fillProxmoxAmbientStorageCard(model.cards[3], view);
  fillProxmoxAmbientNetworkCard(model.cards[4], view);
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

inline uint8_t proxmoxReducedCardIndexAt(const ProxmoxAmbientPageModel &model, uint8_t reducedIndex) {
  uint8_t seen = 0;
  for (uint8_t i = 0; i < model.cardCount; ++i) {
    if (i == model.heroCardIndex) continue;
    if (seen == reducedIndex) return i;
    ++seen;
  }
  return 255;
}
