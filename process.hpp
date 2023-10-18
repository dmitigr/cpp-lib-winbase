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

#include "error.hpp"
#include "windows.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace dmitigr::winbase {

inline std::filesystem::path module_filename(const HMODULE module = {})
{
  std::wstring result;
  result.resize(512);
  while (true) {
    const DWORD result_size{GetModuleFileNameW(module,
      result.data(), result.size())};
    const DWORD err{GetLastError()};
    if (err == ERROR_INSUFFICIENT_BUFFER) {
      result.resize(result.size() * 2);
    } else if (!result_size) {
      throw std::runtime_error{system_message(err)};
    } else if (result_size <= result.size()) {
      result.resize(result_size);
      break;
    } else
      throw std::logic_error{"bug of GetModuleFileNameW()"};
  }
  return result;
}

} // namespace dmitigr::winbase
