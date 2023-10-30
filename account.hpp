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
#pragma comment(lib, "advapi32")

#include "windows.hpp"

#include <stdexcept>
#include <string>

namespace dmitigr::winbase {

struct Account final {
  std::wstring name;
  std::wstring domain;
  SID_NAME_USE type{};
};

inline Account lookup_account(const PSID sid, const std::wstring& system_name = {})
{
  DWORD name_size{};
  DWORD domain_size{};
  const LPCWSTR system{!system_name.empty() ? system_name.c_str() : nullptr};

  LookupAccountSidW(system, sid,
    nullptr, &name_size, nullptr, &domain_size, nullptr);
  if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    throw std::runtime_error{last_error_message()};

  Account result;
  result.name.resize(name_size - 1);
  result.domain.resize(domain_size - 1);
  if (!LookupAccountSidW(system, sid,
      result.name.data(), &name_size,
      result.domain.data(), &domain_size, &result.type))
    throw std::runtime_error{last_error_message()};

  return result;
}

} // namespace dmitigr::winbase
