// Copyright 2026 The Reode Authors

#include "host.h"

#include <windows.h>

#include <string>
#include <string_view>

namespace reode::geode_shim {
namespace {

constinit const ReodeApi* g_api = nullptr;

}  // namespace

void SetApi(const ReodeApi* api) { g_api = api; }

const ReodeApi& Api() { return *g_api; }

bool HasApi() { return g_api != nullptr; }

void Log(ReodeMod* mod, ReodeLogLevel level, std::string_view message) {
  // Messages that do not belong to a mod come from the shim itself.
  std::string text = mod == nullptr ? "[Geode.dll] " + std::string(message)
                                    : std::string(message);
  if (g_api == nullptr) {
    OutputDebugStringA((text + "\n").c_str());
    return;
  }
  g_api->log(mod, level, text.c_str());
}

}  // namespace reode::geode_shim
