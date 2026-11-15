// Copyright 2026 The Reode Authors

#ifndef REODE_LOADER_SRC_CORE_CONSOLE_H_
#define REODE_LOADER_SRC_CORE_CONSOLE_H_

#include <string_view>

namespace reode {

// Opens the Reode debug console window. Quick-edit mode and the close button
// are disabled so that selecting text cannot freeze the game and closing the
// window cannot kill it. Returns false if no console could be created.
bool OpenDebugConsole();

// Writes UTF-8 text (which may contain ANSI escape codes) to the console.
void WriteToConsole(std::string_view utf8_text);

}  // namespace reode

#endif  // REODE_LOADER_SRC_CORE_CONSOLE_H_
