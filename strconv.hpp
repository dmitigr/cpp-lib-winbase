// -*- C++ -*-
//
// Copyright 2023 Dmitry Igrishin
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "windows.hpp"

#include <cassert>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace dmitigr::winbase {

/// @returns The result of conversion UTF-8 string to UTF-16 wide string.
inline std::wstring utf8_to_utf16(const std::string_view utf8)
{
  if (utf8.empty())
    return std::wstring{};

  static const auto throw_error = []
  {
    throw std::runtime_error{"cannot convert an UTF-8 string to an UTF-16 string"};
  };

  if (utf8.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    throw_error();

  const int result_size = MultiByteToWideChar(CP_UTF8, 0,
    utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
  if (!result_size)
    throw_error();

  std::wstring result;
  result.resize(result_size);
  const int rs = MultiByteToWideChar(CP_UTF8, 0,
    utf8.data(), static_cast<int>(utf8.size()),
    result.data(), static_cast<int>(result.size()));
  if (!rs)
    throw_error();

  assert(result_size == rs);

  return result;
}

/// @returns The result of conversion UTF-16 wide-string to UTF-8 string.
inline std::string utf16_to_utf8(const std::wstring_view utf16)
{
  if (utf16.empty())
    return std::string{};

  static const auto throw_error = []
  {
    throw std::runtime_error{"cannot convert an UTF-16 string to an UTF-8 string"};
  };

  if (utf16.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    throw_error();

  const int result_size = WideCharToMultiByte(CP_UTF8, 0,
    utf16.data(), static_cast<int>(utf16.size()), nullptr, 0, nullptr, nullptr);
  if (!result_size)
    throw_error();

  std::string result;
  result.resize(result_size);
  const int rs = WideCharToMultiByte(CP_UTF8, 0,
    utf16.data(), static_cast<int>(utf16.size()),
    result.data(), static_cast<int>(result.size()),
    nullptr, nullptr);
  if (!rs)
    throw_error();

  assert(result_size == rs);

  return result;
}

} // namespace dmitigr::winbase
