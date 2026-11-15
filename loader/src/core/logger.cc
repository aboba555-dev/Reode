// Copyright 2026 The Reode Authors

#include "core/logger.h"

#include <windows.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <format>
#include <mutex>
#include <string>
#include <string_view>

#include "core/console.h"

namespace reode {
namespace {

std::string_view SeverityName(LogSeverity severity) {
  switch (severity) {
    case LogSeverity::kDebug:
      return "DEBUG";
    case LogSeverity::kInfo:
      return "INFO ";
    case LogSeverity::kWarning:
      return "WARN ";
    case LogSeverity::kError:
      return "ERROR";
  }
  return "?????";
}

// ANSI colours; the console is put into virtual terminal mode on open.
std::string_view SeverityColor(LogSeverity severity) {
  switch (severity) {
    case LogSeverity::kDebug:
      return "\x1b[90m";
    case LogSeverity::kInfo:
      return "\x1b[97m";
    case LogSeverity::kWarning:
      return "\x1b[93m";
    case LogSeverity::kError:
      return "\x1b[91m";
  }
  return "";
}

std::string CurrentTime() {
  SYSTEMTIME time{};
  GetLocalTime(&time);
  return std::format("{:02}:{:02}:{:02}.{:03}", time.wHour, time.wMinute,
                     time.wSecond, time.wMilliseconds);
}

}  // namespace

Logger& Logger::Instance() {
  // Intentionally leaked so logging keeps working during process shutdown.
  static Logger* const kLogger = new Logger();
  return *kLogger;
}

void Logger::OpenFile(const std::filesystem::path& directory) {
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  SYSTEMTIME time{};
  GetLocalTime(&time);
  std::wstring name = std::format(
      L"reode_{:04}-{:02}-{:02}_{:02}-{:02}-{:02}.log", time.wYear, time.wMonth,
      time.wDay, time.wHour, time.wMinute, time.wSecond);
  std::lock_guard lock(mutex_);
  if (file_ != nullptr) std::fclose(file_);
  file_ = _wfsopen((directory / name).c_str(), L"w", _SH_DENYWR);
}

void Logger::Write(LogSeverity severity, std::string_view source,
                   std::string_view message) {
  if (severity < min_severity_) return;
  std::string line = std::format("[{}] [{}] [{}] {}\n", CurrentTime(),
                                 SeverityName(severity), source, message);
  std::lock_guard lock(mutex_);
  if (file_ != nullptr) {
    std::fwrite(line.data(), 1, line.size(), file_);
    std::fflush(file_);
  }
  if (console_enabled_) {
    WriteToConsole(std::format("{}{}\x1b[0m", SeverityColor(severity), line));
  }
  OutputDebugStringA(line.c_str());
}

}  // namespace reode
