// Copyright 2026 The Reode Authors

#ifndef REODE_LOADER_SRC_CORE_LOGGER_H_
#define REODE_LOADER_SRC_CORE_LOGGER_H_

#include <cstdio>
#include <filesystem>
#include <format>
#include <mutex>
#include <string_view>
#include <utility>

namespace reode {

enum class LogSeverity { kDebug = 0, kInfo = 1, kWarning = 2, kError = 3 };

// Thread-safe logger that writes to the debug console (when open) and to a
// log file under reode/logs. Use the free functions below instead of the
// class directly.
class Logger {
 public:
  static Logger& Instance();

  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  // Opens a new timestamped log file inside `directory`.
  void OpenFile(const std::filesystem::path& directory);
  void set_console_enabled(bool enabled) { console_enabled_ = enabled; }
  void set_min_severity(LogSeverity severity) { min_severity_ = severity; }

  void Write(LogSeverity severity, std::string_view source,
             std::string_view message);

 private:
  Logger() = default;
  ~Logger() = default;

  std::mutex mutex_;
  std::FILE* file_ = nullptr;
  bool console_enabled_ = false;
  LogSeverity min_severity_ = LogSeverity::kDebug;
};

template <typename... Args>
void LogDebug(std::format_string<Args...> format, Args&&... args) {
  Logger::Instance().Write(LogSeverity::kDebug, "Reode",
                           std::format(format, std::forward<Args>(args)...));
}

template <typename... Args>
void LogInfo(std::format_string<Args...> format, Args&&... args) {
  Logger::Instance().Write(LogSeverity::kInfo, "Reode",
                           std::format(format, std::forward<Args>(args)...));
}

template <typename... Args>
void LogWarning(std::format_string<Args...> format, Args&&... args) {
  Logger::Instance().Write(LogSeverity::kWarning, "Reode",
                           std::format(format, std::forward<Args>(args)...));
}

template <typename... Args>
void LogError(std::format_string<Args...> format, Args&&... args) {
  Logger::Instance().Write(LogSeverity::kError, "Reode",
                           std::format(format, std::forward<Args>(args)...));
}

}  // namespace reode

#endif  // REODE_LOADER_SRC_CORE_LOGGER_H_
