#pragma once

#include <stdint.h>

constexpr uint8_t kAmbientConsolePageMissing = 255;
constexpr uint8_t kAmbientConsoleMaxPages = 2;

enum AmbientConsolePageId : uint8_t {
  AMBIENT_CONSOLE_PAGE_NUT = 0,
  AMBIENT_CONSOLE_PAGE_PROXMOX = 1,
};

struct AmbientConsolePageAvailability {
  bool nutEnabled = true;
  bool proxmoxEnabled = false;
  bool proxmoxImplemented = false;
};

struct AmbientConsolePage {
  AmbientConsolePageId id = AMBIENT_CONSOLE_PAGE_NUT;
  uint8_t index = 0;
};

struct AmbientConsolePageRegistry {
  AmbientConsolePage pages[kAmbientConsoleMaxPages]{};
  uint8_t pageCount = 0;
};

inline void ambientConsoleRegisterPage(AmbientConsolePageRegistry &registry, AmbientConsolePageId id) {
  if (registry.pageCount >= kAmbientConsoleMaxPages) return;
  uint8_t index = registry.pageCount;
  registry.pages[index].id = id;
  registry.pages[index].index = index;
  ++registry.pageCount;
}

inline AmbientConsolePageRegistry ambientConsoleBuildPageRegistry(const AmbientConsolePageAvailability &availability) {
  AmbientConsolePageRegistry registry{};
  if (availability.nutEnabled) ambientConsoleRegisterPage(registry, AMBIENT_CONSOLE_PAGE_NUT);
  if (availability.proxmoxEnabled && availability.proxmoxImplemented) {
    ambientConsoleRegisterPage(registry, AMBIENT_CONSOLE_PAGE_PROXMOX);
  }
  if (registry.pageCount == 0) ambientConsoleRegisterPage(registry, AMBIENT_CONSOLE_PAGE_NUT);
  return registry;
}

inline const AmbientConsolePage *ambientConsolePageAt(const AmbientConsolePageRegistry &registry, uint8_t index) {
  if (index >= registry.pageCount) return nullptr;
  return &registry.pages[index];
}

inline uint8_t ambientConsolePageIndex(const AmbientConsolePageRegistry &registry, AmbientConsolePageId id) {
  for (uint8_t i = 0; i < registry.pageCount; ++i) {
    if (registry.pages[i].id == id) return registry.pages[i].index;
  }
  return kAmbientConsolePageMissing;
}

inline uint8_t ambientConsoleClampPageIndex(const AmbientConsolePageRegistry &registry, uint8_t pageIndex) {
  if (registry.pageCount == 0) return 0;
  return pageIndex < registry.pageCount ? pageIndex : 0;
}
