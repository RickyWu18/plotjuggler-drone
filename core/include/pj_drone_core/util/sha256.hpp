// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace pj_drone::util {

/// Incremental SHA-256. Used to prove that the bytes exported from a log are the bytes that were scanned.
class Sha256 {
 public:
  Sha256();
  void update(const uint8_t* data, size_t length);
  /// Finishes the digest and returns it as 64 lowercase hex characters. The object must not be reused.
  std::string hexDigest();

 private:
  void processBlock(const uint8_t* block);

  std::array<uint32_t, 8> state_;
  std::array<uint8_t, 64> buffer_{};
  size_t buffered_ = 0;
  uint64_t total_bytes_ = 0;
};

}  // namespace pj_drone::util
