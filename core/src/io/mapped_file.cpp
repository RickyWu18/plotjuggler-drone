/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "pj_drone_core/io/mapped_file.hpp"

#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace pj_drone::io {

namespace {
void setError(std::string* error, const char* message) {
  if (error != nullptr) {
    *error = message;
  }
}
}  // namespace

MappedFile::~MappedFile() {
  close();
}

MappedFile::MappedFile(MappedFile&& other) noexcept {
  *this = std::move(other);
}

MappedFile& MappedFile::operator=(MappedFile&& other) noexcept {
  if (this != &other) {
    close();
    data_ = std::exchange(other.data_, nullptr);
    size_ = std::exchange(other.size_, 0);
    open_ = std::exchange(other.open_, false);
#ifdef _WIN32
    file_ = std::exchange(other.file_, nullptr);
    mapping_ = std::exchange(other.mapping_, nullptr);
#else
    fd_ = std::exchange(other.fd_, -1);
#endif
  }
  return *this;
}

#ifdef _WIN32

bool MappedFile::open(const std::filesystem::path& path, std::string* error) {
  close();
  HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    setError(error, "failed to open file");
    return false;
  }
  LARGE_INTEGER size{};
  if (!GetFileSizeEx(file, &size)) {
    CloseHandle(file);
    setError(error, "failed to get file size");
    return false;
  }
  if (size.QuadPart == 0) {
    CloseHandle(file);
    open_ = true;
    return true;
  }
  HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
  if (mapping == nullptr) {
    CloseHandle(file);
    setError(error, "failed to memory-map file");
    return false;
  }
  void* view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
  if (view == nullptr) {
    CloseHandle(mapping);
    CloseHandle(file);
    setError(error, "failed to memory-map file");
    return false;
  }
  file_ = file;
  mapping_ = mapping;
  data_ = static_cast<const uint8_t*>(view);
  size_ = static_cast<size_t>(size.QuadPart);
  open_ = true;
  return true;
}

void MappedFile::close() {
  if (data_ != nullptr) {
    UnmapViewOfFile(data_);
  }
  if (mapping_ != nullptr) {
    CloseHandle(static_cast<HANDLE>(mapping_));
  }
  if (file_ != nullptr) {
    CloseHandle(static_cast<HANDLE>(file_));
  }
  data_ = nullptr;
  size_ = 0;
  open_ = false;
  file_ = nullptr;
  mapping_ = nullptr;
}

#else

bool MappedFile::open(const std::filesystem::path& path, std::string* error) {
  close();
  const int fd = ::open(path.c_str(), O_RDONLY);
  if (fd < 0) {
    setError(error, "failed to open file");
    return false;
  }
  struct stat st{};
  if (::fstat(fd, &st) != 0) {
    ::close(fd);
    setError(error, "failed to get file size");
    return false;
  }
  if (st.st_size == 0) {
    ::close(fd);
    open_ = true;
    return true;
  }
  void* view = ::mmap(nullptr, static_cast<size_t>(st.st_size), PROT_READ, MAP_PRIVATE, fd, 0);
  if (view == MAP_FAILED) {
    ::close(fd);
    setError(error, "failed to memory-map file");
    return false;
  }
  fd_ = fd;
  data_ = static_cast<const uint8_t*>(view);
  size_ = static_cast<size_t>(st.st_size);
  open_ = true;
  return true;
}

void MappedFile::close() {
  if (data_ != nullptr) {
    ::munmap(const_cast<uint8_t*>(data_), size_);
  }
  if (fd_ >= 0) {
    ::close(fd_);
  }
  data_ = nullptr;
  size_ = 0;
  open_ = false;
  fd_ = -1;
}

#endif

}  // namespace pj_drone::io
