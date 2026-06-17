#pragma once

#include <stdint.h>

enum AmbientConsolePageTransitionDirection : uint8_t {
  AMBIENT_PAGE_TRANSITION_NONE = 0,
  AMBIENT_PAGE_TRANSITION_FORWARD = 1,
  AMBIENT_PAGE_TRANSITION_REVERSE = 2,
};

struct AmbientConsolePageTransitionState {
  uint8_t selectedPage = 0;
  uint8_t previousPage = 0;
  AmbientConsolePageTransitionDirection direction = AMBIENT_PAGE_TRANSITION_NONE;
  bool active = false;
  bool pendingRefresh = false;
};

inline uint8_t ambientConsoleClampSelectedPage(AmbientConsolePageTransitionState &state, uint8_t pageCount) {
  if (pageCount == 0) pageCount = 1;
  if (state.selectedPage >= pageCount) state.selectedPage = 0;
  if (state.previousPage >= pageCount) state.previousPage = state.selectedPage;
  return state.selectedPage;
}

inline AmbientConsolePageTransitionDirection ambientConsolePageTransitionDirection(uint8_t fromPage, uint8_t toPage) {
  if (fromPage == toPage) return AMBIENT_PAGE_TRANSITION_NONE;
  return toPage > fromPage ? AMBIENT_PAGE_TRANSITION_FORWARD : AMBIENT_PAGE_TRANSITION_REVERSE;
}

inline bool ambientConsoleStartPageTransition(AmbientConsolePageTransitionState &state, uint8_t targetPage, uint8_t pageCount) {
  ambientConsoleClampSelectedPage(state, pageCount);
  if (pageCount <= 1 || targetPage >= pageCount) return false;
  if (state.active) {
    if (targetPage != state.selectedPage) {
      state.selectedPage = targetPage;
      state.direction = ambientConsolePageTransitionDirection(state.previousPage, targetPage);
      state.pendingRefresh = true;
    }
    return false;
  }
  if (targetPage == state.selectedPage) return false;
  state.previousPage = state.selectedPage;
  state.selectedPage = targetPage;
  state.direction = ambientConsolePageTransitionDirection(state.previousPage, state.selectedPage);
  state.active = true;
  state.pendingRefresh = false;
  return true;
}

inline bool ambientConsoleNotePageRefresh(AmbientConsolePageTransitionState &state) {
  if (!state.active) return false;
  state.pendingRefresh = true;
  return true;
}

inline void ambientConsoleFinishPageTransition(AmbientConsolePageTransitionState &state) {
  state.active = false;
  state.previousPage = state.selectedPage;
  state.direction = AMBIENT_PAGE_TRANSITION_NONE;
  state.pendingRefresh = false;
}
