// Copyright 2026 The Reode Authors
//
// geode::log, forwarded to Reode's logger under the mod's id.

#include <cstddef>
#include <string>

#include "Geode/loader/Log.hpp"
#include "Geode/loader/Mod.hpp"
#include "fmt/format.h"
#include "host.h"
#include "mod_registry.h"

namespace geode::log {
namespace {

// Nesting depth of the current thread's log messages (log::NestScope).
thread_local int nest_depth = 0;

}  // namespace

void pushNest(Mod* /*mod*/) { ++nest_depth; }

void popNest(Mod* /*mod*/) {
  if (nest_depth > 0) --nest_depth;
}

void vlogImpl(Severity severity, Mod* mod, fmt::string_view format,
              fmt::format_args args) {
  std::string message(static_cast<std::size_t>(nest_depth) * 2, ' ');
  message += fmt::vformat(format, args);
  ReodeLogLevel level = kReodeLogInfo;
  switch (static_cast<int>(severity)) {
    case Severity::Trace:
    case Severity::Debug:
      level = kReodeLogDebug;
      break;
    case Severity::Info:
      level = kReodeLogInfo;
      break;
    case Severity::Warning:
      level = kReodeLogWarning;
      break;
    default:
      level = kReodeLogError;
      break;
  }
  reode::geode_shim::ModState* state =
      mod != nullptr ? reode::geode_shim::ModRegistry::Instance().StateOf(mod)
                     : nullptr;
  reode::geode_shim::Log(state != nullptr ? state->reode_mod : nullptr, level,
                         message);
}

}  // namespace geode::log
