#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include "pj_drone_core/util/sha256.hpp"

using pj_drone::util::Sha256;

namespace {
std::string hashOf(const std::string& s, size_t step = 0) {
  Sha256 h;
  const auto* p = reinterpret_cast<const uint8_t*>(s.data());
  if (step == 0) {
    h.update(p, s.size());
  } else {
    for (size_t i = 0; i < s.size(); i += step) {
      h.update(p + i, std::min(step, s.size() - i));
    }
  }
  return h.hexDigest();
}
}  // namespace

TEST(Sha256, KnownVectors) {
  EXPECT_EQ(hashOf(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  EXPECT_EQ(hashOf("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  EXPECT_EQ(hashOf("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST(Sha256, ChunkedUpdateMatchesOneShot) {
  const std::string data(1000, 'x');
  EXPECT_EQ(hashOf(data, 7), hashOf(data));
}
