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

#include "error.hpp"
#include "windows.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace dmitigr::winbase {

class Token_info final {
public:
  Token_info() = default;

  Token_info(const HANDLE token, const TOKEN_INFORMATION_CLASS type)
  {
    reset(token, type);
  }

  void reset(const HANDLE token, const TOKEN_INFORMATION_CLASS type)
  {
    DWORD sz{};
    GetTokenInformation(token, type, nullptr, 0, &sz);
    if (!(sz > 0)) {
      if (const auto err = GetLastError())
        throw std::runtime_error{system_message(err)};
    }

    buf_.resize(sz);
    if (!GetTokenInformation(token, type, buf_.data(), buf_.size(), &sz))
      throw std::runtime_error{last_error_message()};

    type_ = type;
  }

  TOKEN_INFORMATION_CLASS type() const noexcept
  {
    return type_;
  }

  template<class T>
  const T& data() const noexcept
  {
    return *reinterpret_cast<const T*>(buf_.data());
  }

  template<class T>
  T& data() noexcept
  {
    return const_cast<T&>(static_cast<const Token_info*>(this)->data<T>());
  }

private:
  TOKEN_INFORMATION_CLASS type_{};
  std::vector<char> buf_;
};

// -----------------------------------------------------------------------------

/**
 * @returns Locally unique identifier (LUID) used on a specified system to
 * locally represent the specified privilege name.
 */
inline LUID lookup_privilege_value(const std::wstring& privilege_name,
  const std::wstring& system_name = {})
{
  LUID result{};
  if (!LookupPrivilegeValueW(
      system_name.empty() ? nullptr : system_name.c_str(),
      privilege_name.c_str(), &result))
    throw std::runtime_error{last_error_message()};
  return result;
}

/**
 * @returns The name that corresponds to the privilege represented on a specific
 * system by a specified locally unique identifier (LUID).
 */
inline std::wstring lookup_privilege_name(LUID luid,
  const std::wstring& system_name = {})
{
  DWORD sz{64 + 1};
  std::wstring result(sz - 1, 0);
  while (true) {
    if (!LookupPrivilegeNameW(
        system_name.empty() ? nullptr : system_name.c_str(),
        &luid, result.data(), &sz)) {
      if (const DWORD err{GetLastError()}; err == ERROR_INSUFFICIENT_BUFFER)
        result.resize(sz - 1);
      else
        throw std::runtime_error{system_message(err)};
    } else {
      result.resize(sz);
      break;
    }
  }
  return result;
}

/// A token privileges.
class Token_privileges final {
public:
  Token_privileges()
    : Token_privileges{0}
  {}

  explicit Token_privileges(const DWORD count)
    : data_(required_data_size(count))
  {
    data()->PrivilegeCount = count;
  }

  void resize(const DWORD count)
  {
    data_.resize(required_data_size(count));
    data()->PrivilegeCount = count;
  }

  DWORD size() const noexcept
  {
    return data()->PrivilegeCount;
  }

  DWORD size_in_bytes() const noexcept
  {
    return static_cast<DWORD>(data_.size());
  }

  void set(const DWORD index, const LUID luid, const DWORD attributes)
  {
    if (!(0 <= index && index < size()))
      throw std::invalid_argument{"invalid privilege index"};

    data()->Privileges[index].Luid = luid;
    data()->Privileges[index].Attributes = attributes;
  }

  void set(const DWORD index, const std::wstring& privilege_name,
    const std::wstring& system_name, const DWORD attributes)
  {
    set(index, lookup_privilege_value(privilege_name, system_name), attributes);
  }

  void set(const DWORD index, const std::wstring& privilege_name,
    const DWORD attributes)
  {
    set(index, privilege_name, std::wstring{}, attributes);
  }

  const TOKEN_PRIVILEGES* data() const noexcept
  {
    return reinterpret_cast<const TOKEN_PRIVILEGES*>(data_.data());
  }

  TOKEN_PRIVILEGES* data() noexcept
  {
    return const_cast<TOKEN_PRIVILEGES*>(
      static_cast<const Token_privileges*>(this)->data());
  }

private:
  std::vector<char> data_;

  static std::size_t required_data_size(const DWORD count)
  {
    if (count < 0)
      throw std::invalid_argument{"invalid privilege count"};
    return sizeof(TOKEN_PRIVILEGES::PrivilegeCount) +
      sizeof(TOKEN_PRIVILEGES::Privileges) * count;
  }
};

/**
 * @brief Toggles privileges in the specified access `token`.
 *
 * @returns A pair of token privileges and error code, which can be
 *   - `ERROR_SUCCESS` indicating that the function adjusted all
 *   specified privileges;
 *   - `ERROR_NOT_ALL_ASSIGNED` indicating the `token` doesn't have
 *   one or more of the privileges specified in the `new_state`.
 *
 * @param token An access token.
 * @param disable_all_privileges If `true`, the function disables all privileges
 * ignoring the `new_state` parameter.
 * @param new_state Privileges for the `token` to be enabled, disabled or removed.
 *
 * @par Requires
 * `token` requires `TOKEN_ADJUST_PRIVILEGES` access.
 */
inline std::pair<Token_privileges, DWORD>
adjust_token_privileges(const HANDLE token,
  const bool disable_all_privileges,
  const Token_privileges& new_state)
{
  auto prev_state = new_state;
  auto prev_state_size_in_bytes = prev_state.size_in_bytes();
  if (!AdjustTokenPrivileges(token, disable_all_privileges,
      const_cast<TOKEN_PRIVILEGES*>(new_state.data()), new_state.size_in_bytes(),
      prev_state.data(), &prev_state_size_in_bytes))
    throw std::runtime_error{last_error_message()};
  prev_state.resize(prev_state.data()->PrivilegeCount);
  assert(prev_state.size_in_bytes() <= prev_state_size_in_bytes);
  return std::make_pair(std::move(prev_state), GetLastError());
}

/// @overload
inline void set_token_information(const HANDLE token,
  const TOKEN_INFORMATION_CLASS type, DWORD value)
{
  if (!SetTokenInformation(token, type, &value, sizeof(value)))
    throw std::runtime_error{last_error_message()};
}

} // namespace dmitigr::winbase
