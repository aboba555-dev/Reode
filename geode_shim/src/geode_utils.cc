// Copyright 2026 The Reode Authors
//
// geode::utils: strings, files, number parsing, clipboard, random numbers,
// game control and small system helpers.

#include <bcrypt.h>
#include <shellapi.h>
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <compare>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "Geode/cocos/include/cocos2d.h"
#include "Geode/utils/file.hpp"
#include "Geode/utils/general.hpp"
#include "Geode/utils/random.hpp"
#include "Geode/utils/string.hpp"
#include "fmt/format.h"
#include "matjson.hpp"

namespace geode::utils {

// --- string -----------------------------------------------------------------

namespace string {
namespace {

bool IsSpace(char c) { return std::isspace(static_cast<unsigned char>(c)); }

}  // namespace

std::string wideToUtf8(std::wstring_view str) {
  if (str.empty()) return {};
  int size =
      WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()),
                          nullptr, 0, nullptr, nullptr);
  std::string result(size, '\0');
  WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()),
                      result.data(), size, nullptr, nullptr);
  return result;
}

std::wstring utf8ToWide(std::string_view str) {
  if (str.empty()) return {};
  int size = MultiByteToWideChar(CP_UTF8, 0, str.data(),
                                 static_cast<int>(str.size()), nullptr, 0);
  std::wstring result(size, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()),
                      result.data(), size);
  return result;
}

Result<std::string> utf16ToUtf8(std::u16string_view str) {
  int size = WideCharToMultiByte(
      CP_UTF8, WC_ERR_INVALID_CHARS, reinterpret_cast<LPCWCH>(str.data()),
      static_cast<int>(str.size()), nullptr, 0, nullptr, nullptr);
  if (size == 0 && !str.empty()) return Err("invalid UTF-16");
  std::string result(size, '\0');
  WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<LPCWCH>(str.data()),
                      static_cast<int>(str.size()), result.data(), size,
                      nullptr, nullptr);
  return Ok(std::move(result));
}

Result<std::u16string> utf8ToUtf16(std::string_view str) {
  int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.data(),
                                 static_cast<int>(str.size()), nullptr, 0);
  if (size == 0 && !str.empty()) return Err("invalid UTF-8");
  std::u16string result(size, u'\0');
  MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()),
                      reinterpret_cast<LPWSTR>(result.data()), size);
  return Ok(std::move(result));
}

Result<std::string> utf32ToUtf8(std::u32string_view str) {
  std::string result;
  for (char32_t c : str) {
    if (c < 0x80) {
      result += static_cast<char>(c);
    } else if (c < 0x800) {
      result += static_cast<char>(0xC0 | (c >> 6));
      result += static_cast<char>(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
      if (c >= 0xD800 && c <= 0xDFFF) return Err("invalid code point");
      result += static_cast<char>(0xE0 | (c >> 12));
      result += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
      result += static_cast<char>(0x80 | (c & 0x3F));
    } else if (c <= 0x10FFFF) {
      result += static_cast<char>(0xF0 | (c >> 18));
      result += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
      result += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
      result += static_cast<char>(0x80 | (c & 0x3F));
    } else {
      return Err("invalid code point");
    }
  }
  return Ok(std::move(result));
}

Result<std::u32string> utf8ToUtf32(std::string_view str) {
  std::u32string result;
  for (std::size_t i = 0; i < str.size();) {
    auto byte = static_cast<unsigned char>(str[i]);
    int length = byte < 0x80         ? 1
                 : (byte >> 5) == 6  ? 2
                 : (byte >> 4) == 14 ? 3
                 : (byte >> 3) == 30 ? 4
                                     : 0;
    if (length == 0 || i + length > str.size()) return Err("invalid UTF-8");
    char32_t c = length == 1 ? byte : byte & (0x7F >> length);
    for (int k = 1; k < length; ++k) {
      auto next = static_cast<unsigned char>(str[i + k]);
      if ((next & 0xC0) != 0x80) return Err("invalid UTF-8");
      c = (c << 6) | (next & 0x3F);
    }
    result += c;
    i += length;
  }
  return Ok(std::move(result));
}

std::string pathToString(std::filesystem::path const& path) {
  return wideToUtf8(path.native());
}

void toLowerIP(std::string& str) {
  for (char& c : str)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

std::string toLower(std::string str) {
  toLowerIP(str);
  return str;
}

void toUpperIP(std::string& str) {
  for (char& c : str)
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
}

std::string toUpper(std::string str) {
  toUpperIP(str);
  return str;
}

void replaceIP(std::string& str, std::string_view orig, std::string_view repl) {
  if (orig.empty()) return;
  std::size_t pos = 0;
  while ((pos = str.find(orig, pos)) != std::string::npos) {
    str.replace(pos, orig.size(), repl);
    pos += repl.size();
  }
}

std::string replace(std::string str, std::string_view orig,
                    std::string_view repl) {
  replaceIP(str, orig, repl);
  return str;
}

std::vector<std::string_view> splitView(std::string_view str,
                                        std::string_view separator) {
  std::vector<std::string_view> parts;
  if (separator.empty()) {
    parts.push_back(str);
    return parts;
  }
  std::size_t start = 0;
  while (true) {
    std::size_t pos = str.find(separator, start);
    parts.push_back(str.substr(start, pos - start));
    if (pos == std::string_view::npos) break;
    start = pos + separator.size();
  }
  return parts;
}

std::vector<std::string> split(std::string_view str,
                               std::string_view separator) {
  std::vector<std::string> parts;
  for (std::string_view part : splitView(str, separator)) {
    parts.emplace_back(part);
  }
  return parts;
}

std::vector<char> split(std::string_view str) {
  return std::vector<char>(str.begin(), str.end());
}

std::string join(std::span<std::string const> strs,
                 std::string_view separator) {
  std::string result;
  for (std::size_t i = 0; i < strs.size(); ++i) {
    if (i != 0) result += separator;
    result += strs[i];
  }
  return result;
}

std::string join(std::span<std::string_view const> strs,
                 std::string_view separator) {
  std::string result;
  for (std::size_t i = 0; i < strs.size(); ++i) {
    if (i != 0) result += separator;
    result += strs[i];
  }
  return result;
}

bool contains(std::string_view str, std::string_view subs) {
  return str.find(subs) != std::string_view::npos;
}

bool contains(std::string_view str, char c) {
  return str.find(c) != std::string_view::npos;
}

bool containsAny(std::string_view str, std::span<std::string const> subs) {
  return std::ranges::any_of(
      subs, [&](const std::string& s) { return contains(str, s); });
}

bool containsAll(std::string_view str, std::span<std::string const> subs) {
  return std::ranges::all_of(
      subs, [&](const std::string& s) { return contains(str, s); });
}

size_t count(std::string_view str, char c) {
  return static_cast<size_t>(std::ranges::count(str, c));
}

void trimLeftIP(std::string& str) {
  auto it = std::find_if_not(str.begin(), str.end(), IsSpace);
  str.erase(str.begin(), it);
}

void trimLeftIP(std::string& str, std::string_view chars) {
  str.erase(0, std::min(str.find_first_not_of(chars), str.size()));
}

void trimRightIP(std::string& str) {
  auto it = std::find_if_not(str.rbegin(), str.rend(), IsSpace);
  str.erase(it.base(), str.end());
}

void trimRightIP(std::string& str, std::string_view chars) {
  std::size_t last = str.find_last_not_of(chars);
  str.erase(last == std::string::npos ? 0 : last + 1);
}

void trimIP(std::string& str) {
  trimRightIP(str);
  trimLeftIP(str);
}

void trimIP(std::string& str, std::string_view chars) {
  trimRightIP(str, chars);
  trimLeftIP(str, chars);
}

std::string trimLeft(std::string str) {
  trimLeftIP(str);
  return str;
}

std::string trimLeft(std::string str, std::string_view chars) {
  trimLeftIP(str, chars);
  return str;
}

std::string trimRight(std::string str) {
  trimRightIP(str);
  return str;
}

std::string trimRight(std::string str, std::string_view chars) {
  trimRightIP(str, chars);
  return str;
}

std::string trim(std::string str) {
  trimIP(str);
  return str;
}

std::string trim(std::string str, std::string_view chars) {
  trimIP(str, chars);
  return str;
}

std::string filter(std::string_view str, std::string_view allowed) {
  std::string result;
  for (char c : str) {
    if (allowed.find(c) != std::string_view::npos) result += c;
  }
  return result;
}

std::string remove(std::string_view str, std::string_view disallowed) {
  std::string result;
  for (char c : str) {
    if (disallowed.find(c) == std::string_view::npos) result += c;
  }
  return result;
}

bool startsWith(std::string_view str, std::string_view prefix) {
  return str.starts_with(prefix);
}

bool endsWith(std::string_view str, std::string_view suffix) {
  return str.ends_with(suffix);
}

std::strong_ordering caseInsensitiveCompare(std::string_view a,
                                            std::string_view b) {
  std::size_t length = std::min(a.size(), b.size());
  for (std::size_t i = 0; i < length; ++i) {
    int x = std::tolower(static_cast<unsigned char>(a[i]));
    int y = std::tolower(static_cast<unsigned char>(b[i]));
    if (x != y) return x <=> y;
  }
  return a.size() <=> b.size();
}

}  // namespace string

// --- number parsing ---------------------------------------------------------

namespace _detail {
namespace {

template <typename T>
Result<T> Parse(std::string_view str, int base) {
  T value{};
  auto [ptr, error] =
      std::from_chars(str.data(), str.data() + str.size(), value, base);
  if (error != std::errc() || ptr != str.data() + str.size() || str.empty()) {
    return Err(fmt::format("\"{}\" is not a valid number", str));
  }
  return Ok(value);
}

template <typename T>
Result<T> ParseFloat(std::string_view str) {
  T value{};
  auto [ptr, error] =
      std::from_chars(str.data(), str.data() + str.size(), value);
  if (error != std::errc() || ptr != str.data() + str.size() || str.empty()) {
    return Err(fmt::format("\"{}\" is not a valid number", str));
  }
  return Ok(value);
}

}  // namespace

Result<long double> longDoubleFromString(std::string_view str) {
  return ParseFloat<long double>(str);
}
Result<double> doubleFromString(std::string_view str) {
  return ParseFloat<double>(str);
}
Result<float> floatFromString(std::string_view str) {
  return ParseFloat<float>(str);
}
Result<uint64_t> uint64FromString(std::string_view str, int base) {
  return Parse<uint64_t>(str, base);
}
Result<int64_t> int64FromString(std::string_view str, int base) {
  return Parse<int64_t>(str, base);
}

}  // namespace _detail

// --- general ----------------------------------------------------------------

float getDisplayFactor() { return 1.0f; }

std::string getEnvironmentVariable(ZStringView name) {
  std::wstring wide = string::utf8ToWide(name);
  DWORD size = GetEnvironmentVariableW(wide.c_str(), nullptr, 0);
  if (size == 0) return {};
  std::wstring value(size, L'\0');
  value.resize(GetEnvironmentVariableW(wide.c_str(), value.data(), size));
  return string::wideToUtf8(value);
}

std::string formatSystemError(int code) {
  LPWSTR buffer = nullptr;
  DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                    FORMAT_MESSAGE_FROM_SYSTEM |
                                    FORMAT_MESSAGE_IGNORE_INSERTS,
                                nullptr, static_cast<DWORD>(code), 0,
                                reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
  std::string message = length != 0
                            ? string::trim(string::wideToUtf8({buffer, length}))
                            : fmt::format("error {}", code);
  LocalFree(buffer);
  return message;
}

cocos2d::CCRect getSafeAreaRect() {
  cocos2d::CCSize size = cocos2d::CCDirector::sharedDirector()->getWinSize();
  return cocos2d::CCRect(0, 0, size.width, size.height);
}

}  // namespace geode::utils

namespace geode::utils::thread {
namespace {

thread_local std::string thread_name;

}  // namespace

std::string getDefaultName() {
  return fmt::format("Thread #{}", GetCurrentThreadId());
}

ZStringView getName() { return thread_name; }

void setName(std::string name) {
  // Also visible in debuggers and crash dumps.
  std::wstring wide = string::utf8ToWide(name);
  SetThreadDescription(GetCurrentThread(), wide.c_str());
  thread_name = std::move(name);
}

}  // namespace geode::utils::thread

namespace geode::utils::clipboard {

bool write(ZStringView data) {
  std::wstring wide = string::utf8ToWide(data);
  if (!OpenClipboard(nullptr)) return false;
  EmptyClipboard();
  HGLOBAL memory =
      GlobalAlloc(GMEM_MOVEABLE, (wide.size() + 1) * sizeof(wchar_t));
  bool ok = false;
  if (memory != nullptr) {
    std::memcpy(GlobalLock(memory), wide.c_str(),
                (wide.size() + 1) * sizeof(wchar_t));
    GlobalUnlock(memory);
    ok = SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
    if (!ok) GlobalFree(memory);
  }
  CloseClipboard();
  return ok;
}

std::string read() {
  if (!OpenClipboard(nullptr)) return {};
  std::string result;
  if (HANDLE data = GetClipboardData(CF_UNICODETEXT)) {
    if (const auto* text = static_cast<const wchar_t*>(GlobalLock(data))) {
      result = string::wideToUtf8(text);
      GlobalUnlock(data);
    }
  }
  CloseClipboard();
  return result;
}

}  // namespace geode::utils::clipboard

namespace geode::utils::game {
namespace {

// Closing the window goes through the game's normal shutdown, which saves.
void CloseGameWindow() {
  HWND window = FindWindowW(nullptr, L"Geometry Dash");
  if (window != nullptr) {
    PostMessageW(window, WM_CLOSE, 0, 0);
  } else {
    cocos2d::CCDirector::sharedDirector()->end();
  }
}

}  // namespace

void exit(bool /*saveData*/) { CloseGameWindow(); }

void restart(bool saveData) { restart(saveData, false); }

void restart(bool /*saveData*/, bool /*safeMode*/) {
  std::wstring path(MAX_PATH, L'\0');
  path.resize(GetModuleFileNameW(nullptr, path.data(), MAX_PATH));
  std::wstring directory = std::filesystem::path(path).parent_path().native();
  ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, directory.c_str(),
                SW_SHOWNORMAL);
  CloseGameWindow();
}

}  // namespace geode::utils::game

namespace geode::utils::random {

Generator& _getGenerator() {
  thread_local Generator generator;
  return generator;
}

void fillBytes(void* buffer, size_t size) {
  BCryptGenRandom(nullptr, static_cast<PUCHAR>(buffer),
                  static_cast<ULONG>(size), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
}

std::string generateString(size_t length, std::string_view alphabet) {
  std::string result;
  if (alphabet.empty()) return result;
  for (size_t i = 0; i < length; ++i) {
    result += alphabet[_getGenerator().next() % alphabet.size()];
  }
  return result;
}

std::string generateHexString(size_t length) {
  return generateString(length, "0123456789abcdef");
}

std::string generateAlphanumericString(size_t length) {
  return generateString(
      length, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
}

std::string generateUUID() {
  std::uint8_t bytes[16];
  fillBytes(bytes, sizeof(bytes));
  bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0F) | 0x40);  // v4
  bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3F) | 0x80);
  std::string text;
  for (int i = 0; i < 16; ++i) {
    if (i == 4 || i == 6 || i == 8 || i == 10) text += '-';
    text += fmt::format("{:02x}", bytes[i]);
  }
  return text;
}

}  // namespace geode::utils::random

namespace geode::utils::file {
namespace {

std::string PathText(std::filesystem::path const& path) {
  return string::pathToString(path);
}

Result<> WriteBytes(std::filesystem::path const& path, const char* data,
                    std::size_t size) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) return Err(fmt::format("cannot open {}", PathText(path)));
  stream.write(data, static_cast<std::streamsize>(size));
  if (!stream) return Err(fmt::format("cannot write {}", PathText(path)));
  return Ok();
}

// Writes to a temporary file first, so an interrupted write never leaves a
// truncated file behind.
Result<> WriteBytesSafe(std::filesystem::path const& path, const char* data,
                        std::size_t size) {
  std::filesystem::path temp = path;
  temp += L".tmp";
  if (auto result = WriteBytes(temp, data, size); !result) return result;
  std::error_code error;
  std::filesystem::rename(temp, path, error);
  if (error) {
    return Err(
        fmt::format("cannot replace {}: {}", PathText(path), error.message()));
  }
  return Ok();
}

}  // namespace

Result<std::string> readString(std::filesystem::path const& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) return Err(fmt::format("cannot open {}", PathText(path)));
  return Ok(std::string((std::istreambuf_iterator<char>(stream)),
                        std::istreambuf_iterator<char>()));
}

Result<matjson::Value> readJson(std::filesystem::path const& path) {
  auto text = readString(path);
  if (!text) return Err(text.unwrapErr());
  auto json = matjson::parse(text.unwrap());
  if (!json) {
    return Err(fmt::format("{} is not valid JSON: {}", PathText(path),
                           json.unwrapErr().message));
  }
  return Ok(std::move(json).unwrap());
}

Result<ByteVector> readBinary(std::filesystem::path const& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) return Err(fmt::format("cannot open {}", PathText(path)));
  return Ok(ByteVector((std::istreambuf_iterator<char>(stream)),
                       std::istreambuf_iterator<char>()));
}

Result<> writeString(std::filesystem::path const& path, std::string_view data) {
  return WriteBytes(path, data.data(), data.size());
}

Result<> writeStringSafe(std::filesystem::path const& path,
                         std::string_view data) {
  return WriteBytesSafe(path, data.data(), data.size());
}

Result<> writeBinary(std::filesystem::path const& path, ByteSpan data) {
  return WriteBytes(path, reinterpret_cast<const char*>(data.data()),
                    data.size());
}

Result<> writeBinarySafe(std::filesystem::path const& path, ByteSpan data) {
  return WriteBytesSafe(path, reinterpret_cast<const char*>(data.data()),
                        data.size());
}

Result<> createDirectory(std::filesystem::path const& path) {
  std::error_code error;
  std::filesystem::create_directory(path, error);
  if (error) return Err(fmt::format("cannot create {}", PathText(path)));
  return Ok();
}

Result<> createDirectoryAll(std::filesystem::path const& path) {
  std::error_code error;
  std::filesystem::create_directories(path, error);
  if (error) return Err(fmt::format("cannot create {}", PathText(path)));
  return Ok();
}

Result<std::vector<std::filesystem::path>> readDirectory(
    std::filesystem::path const& path, bool recursive) {
  std::error_code error;
  std::vector<std::filesystem::path> entries;
  if (recursive) {
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(path, error)) {
      entries.push_back(entry.path());
    }
  } else {
    for (const auto& entry : std::filesystem::directory_iterator(path, error)) {
      entries.push_back(entry.path());
    }
  }
  if (error) return Err(fmt::format("cannot read {}", PathText(path)));
  return Ok(std::move(entries));
}

bool openFolder(std::filesystem::path const& path) {
  auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(
      nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
  return result > 32;
}

}  // namespace geode::utils::file
