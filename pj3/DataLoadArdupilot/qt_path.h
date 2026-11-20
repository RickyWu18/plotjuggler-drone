// SPDX-License-Identifier: MIT

#pragma once

#include <QString>
#include <filesystem>

// A Qt path as a std::filesystem::path, without a lossy code-page round trip on Windows.
inline std::filesystem::path pathFromQString(const QString& s)
{
#ifdef _WIN32
  return std::filesystem::path(s.toStdWString());
#else
  return std::filesystem::path(s.toStdString());  // Qt encodes as UTF-8
#endif
}
