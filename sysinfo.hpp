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
#pragma comment(lib, "kernel32")

#include "error.hpp"

#include <cassert>
#include <filesystem>
#include <stdexcept>

namespace dmitigr::winbase {

inline std::filesystem::path system_directory()
{
  std::wstring result;
  const auto size_with_null = GetSystemDirectoryW(result.data(), result.size());
  if (!size_with_null)
    throw std::runtime_error{last_error_message()};
  result.resize(size_with_null - 1);
  const auto sz = GetSystemDirectoryW(result.data(), result.size() + 1);
  if (!sz)
    throw std::runtime_error{last_error_message()};
  assert(sz == size_with_null - 1);
  return result;
}

} // namespace dmitigr::winbase
