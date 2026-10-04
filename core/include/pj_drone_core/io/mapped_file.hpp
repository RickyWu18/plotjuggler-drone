// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace pj_drone::io {

/// Read-only memory mapping of a whole file. An empty file maps to {nullptr, 0}.
class MappedFile {
 public:
  MappedFile() = default;
  ~MappedFile();
  MappedFile(const MappedFile&) = delete;
  MappedFile& operator=(const MappedFile&) = delete;
  MappedFile(MappedFile&& other) noexcept;
  MappedFile& operator=(MappedFile&& other) noexcept;

  /// Returns false and fills `error` (when non-null) on failure.
  bool open(const std::filesystem::path& path, std::string* error = nullptr);
  void close();

  const uint8_t* data() const {
    return data_;
  }
  size_t size() const {
    return size_;
  }
  bool isOpen() const {
    return open_;
  }

 private:
  const uint8_t* data_ = nullptr;
  size_t size_ = 0;
  bool open_ = false;
#ifdef _WIN32
  void* file_ = nullptr;     // HANDLE
  void* mapping_ = nullptr;  // HANDLE
#else
  int fd_ = -1;
#endif
};

}  // namespace pj_drone::io
