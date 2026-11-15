// Copyright 2026 The Reode Authors

#include "mods/mod_package.h"

#include <chrono>
#include <cstdio>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

#include "core/paths.h"
#include "miniz.h"

namespace reode::mods {
namespace {

constexpr char kStampFile[] = ".reode-package-stamp";

// RAII owner for a miniz zip reader opened on a file.
class ZipReader {
 public:
  ZipReader() = default;
  ~ZipReader() {
    if (open_) mz_zip_reader_end(&archive_);
  }
  ZipReader(const ZipReader&) = delete;
  ZipReader& operator=(const ZipReader&) = delete;

  bool Open(const std::filesystem::path& path) {
    // miniz takes a narrow path; read the archive into memory instead so
    // non-ASCII install directories work.
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return false;
    data_.assign(std::istreambuf_iterator<char>(stream),
                 std::istreambuf_iterator<char>());
    open_ = mz_zip_reader_init_mem(&archive_, data_.data(), data_.size(), 0);
    return open_;
  }

  mz_zip_archive* get() { return &archive_; }

 private:
  mz_zip_archive archive_{};
  std::string data_;
  bool open_ = false;
};

bool IsSafeEntryName(const std::filesystem::path& relative) {
  if (relative.empty() || relative.is_absolute() || relative.has_root_name()) {
    return false;
  }
  for (const auto& part : relative) {
    if (part == "..") return false;
  }
  return true;
}

std::string PackageStamp(const std::filesystem::path& package) {
  std::error_code error;
  auto size = std::filesystem::file_size(package, error);
  auto time = std::filesystem::last_write_time(package, error);
  return std::format("{}:{}", size, time.time_since_epoch().count());
}

}  // namespace

std::expected<std::string, std::string> ReadPackageManifest(
    const std::filesystem::path& package) {
  ZipReader zip;
  if (!zip.Open(package)) return std::unexpected("not a valid zip archive");
  int index = mz_zip_reader_locate_file(zip.get(), "mod.json", nullptr, 0);
  if (index < 0) return std::unexpected("package has no mod.json");
  std::size_t size = 0;
  void* data = mz_zip_reader_extract_to_heap(
      zip.get(), static_cast<mz_uint>(index), &size, 0);
  if (data == nullptr) return std::unexpected("failed to read mod.json");
  std::string text(static_cast<const char*>(data), size);
  mz_free(data);
  return text;
}

std::expected<void, std::string> ExtractPackage(
    const std::filesystem::path& package,
    const std::filesystem::path& destination) {
  const std::string stamp = PackageStamp(package);
  {
    std::ifstream existing(destination / kStampFile, std::ios::binary);
    std::string previous((std::istreambuf_iterator<char>(existing)),
                         std::istreambuf_iterator<char>());
    if (existing && previous == stamp) return {};
  }

  ZipReader zip;
  if (!zip.Open(package)) return std::unexpected("not a valid zip archive");

  std::error_code error;
  std::filesystem::remove_all(destination, error);
  std::filesystem::create_directories(destination, error);
  if (error) {
    return std::unexpected(std::format(
        "cannot create {}: {}", PathToUtf8(destination), error.message()));
  }

  const mz_uint count = mz_zip_reader_get_num_files(zip.get());
  for (mz_uint i = 0; i < count; ++i) {
    mz_zip_archive_file_stat stat{};
    if (!mz_zip_reader_file_stat(zip.get(), i, &stat)) {
      return std::unexpected("corrupt zip directory");
    }
    std::u8string name(reinterpret_cast<const char8_t*>(stat.m_filename));
    std::filesystem::path relative =
        std::filesystem::path(name).lexically_normal();
    if (!IsSafeEntryName(relative)) {
      return std::unexpected(
          std::format("unsafe entry name \"{}\"", stat.m_filename));
    }
    std::filesystem::path target = destination / relative;
    if (stat.m_is_directory) {
      std::filesystem::create_directories(target, error);
      continue;
    }
    std::filesystem::create_directories(target.parent_path(), error);
    std::size_t size = 0;
    void* data = mz_zip_reader_extract_to_heap(zip.get(), i, &size, 0);
    if (data == nullptr) {
      return std::unexpected(
          std::format("failed to extract {}", stat.m_filename));
    }
    std::ofstream out(target, std::ios::binary | std::ios::trunc);
    out.write(static_cast<const char*>(data),
              static_cast<std::streamsize>(size));
    mz_free(data);
    if (!out) {
      return std::unexpected(
          std::format("failed to write {}", PathToUtf8(target)));
    }
  }

  std::ofstream stamp_file(destination / kStampFile,
                           std::ios::binary | std::ios::trunc);
  stamp_file << stamp;
  return {};
}

}  // namespace reode::mods
