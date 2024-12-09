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

#include "../base/noncopymove.hpp"
#include "error.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace dmitigr::winbase {

class Sid final : private Noncopy {
public:
  ~Sid()
  {
    if (data_)
      FreeSid(data_);
  }

  Sid() = default;

  Sid(Sid&& rhs) noexcept
    : data_{rhs.data_}
  {
    rhs.data_ = {};
  }

  Sid& operator=(Sid&& rhs) noexcept
  {
    Sid tmp{std::move(rhs)};
    swap(tmp);
    return *this;
  }

  void swap(Sid& rhs) noexcept
  {
    using std::swap;
    swap(data_, rhs.data_);
  }

  template<typename ... S>
  Sid(SID_IDENTIFIER_AUTHORITY authority, const S ... sub_authorities)
  {
    constexpr const auto sub_auth_sz = sizeof...(sub_authorities);
    static_assert(sub_auth_sz <= 8);
    std::tuple<PSID_IDENTIFIER_AUTHORITY, BYTE,
      DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, PSID*> args{};
    std::get<0>(args) = &authority;
    std::get<1>(args) = static_cast<BYTE>(sub_auth_sz);
    fill_subs(args, std::make_index_sequence<sub_auth_sz>{},
      std::forward<const DWORD>(sub_authorities)...);
    std::get<10>(args) = &data_;
    if (!std::apply(AllocateAndInitializeSid, args))
      throw std::runtime_error{last_error_message()};
  }

  const PSID data() const noexcept
  {
    return data_;
  }

private:
  PSID data_{};

  template<class Tuple, std::size_t ... I, typename ... S>
  void fill_subs(Tuple& args, std::index_sequence<I...> seq,
    const S ... sub_authorities)
  {
    static_assert(sizeof...(sub_authorities) == seq.size());
    ((std::get<I + 2>(args) = sub_authorities), ...);
  }
};

struct Account final {
  SID_NAME_USE type{};
  std::vector<char> sid_buf;
  std::wstring name;
  std::wstring domain;

  const PSID sid() const noexcept
  {
    return !sid_buf.empty() ?
      reinterpret_cast<PSID>(const_cast<char*>(sid_buf.data())) : nullptr;
  }

  PSID sid() noexcept
  {
    return const_cast<PSID>(static_cast<const Account*>(this)->sid());
  }
};

inline Account lookup_account(const PSID sid,
  const std::wstring& system_name = {})
{
  if (!IsValidSid(sid))
    throw std::invalid_argument{"cannot lookup accout: invalid SID"};

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

  result.sid_buf.resize(GetLengthSid(sid));
  if (!CopySid(static_cast<DWORD>(result.sid_buf.size()),
      result.sid_buf.data(), sid))
    throw std::runtime_error{last_error_message()};

  return result;
}

inline Account lookup_account(const std::wstring& name,
  const std::wstring& system_name = {})
{
  if (name.empty())
    throw std::invalid_argument{"cannot lookup accout: invalid name"};

  const LPCWSTR system{!system_name.empty() ? system_name.c_str() : nullptr};
  DWORD sid_buf_size{};
  DWORD domain_size{};
  LookupAccountNameW(system, name.c_str(), nullptr, &sid_buf_size,
    nullptr, &domain_size, nullptr);
  if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    throw std::runtime_error{last_error_message()};

  Account result;
  result.name = name;
  result.sid_buf.resize(sid_buf_size);
  result.domain.resize(domain_size - 1);
  if (!LookupAccountNameW(system, name.c_str(),
      result.sid_buf.data(), &sid_buf_size,
      result.domain.data(), &domain_size, &result.type))
    throw std::runtime_error{last_error_message()};

  return result;
}

} // namespace dmitigr::winbase
