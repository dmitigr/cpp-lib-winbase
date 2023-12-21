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
#include "detail.hpp"
#include "exceptions.hpp"

#include <type_traits>
#include <utility>

namespace dmitigr::winbase::registry {

/// A very thin wrapper around the HKEY data type.
class Hkey_guard final : private Noncopy {
public:
  /// The destructor.
  ~Hkey_guard()
  {
    close();
  }

  /// The constructor.
  explicit Hkey_guard(const HKEY handle = NULL) noexcept
    : handle_{handle}
  {}

  /// The move constructor.
  Hkey_guard(Hkey_guard&& rhs) noexcept
    : handle_{rhs.handle_}
  {
    rhs.handle_ = NULL;
  }

  /// The move assignment operator.
  Hkey_guard& operator=(Hkey_guard&& rhs) noexcept
  {
    if (this != &rhs) {
      Hkey_guard tmp{std::move(rhs)};
      swap(tmp);
    }
    return *this;
  }

  /// The swap operation.
  void swap(Hkey_guard& other) noexcept
  {
    using std::swap;
    swap(handle_, other.handle_);
  }

  /// @returns The guarded HKEY.
  HKEY handle() const noexcept
  {
    return handle_;
  }

  /// @returns The guarded HKEY.
  operator HKEY() const noexcept
  {
    return handle();
  }

  /// @returns The error code.
  LSTATUS close() noexcept
  {
    LSTATUS result{ERROR_SUCCESS};
    if (handle_ != NULL) {
      if ( (result = RegCloseKey(handle_)) != ERROR_SUCCESS)
        handle_ = NULL;
    }
    return result;
  }

private:
  HKEY handle_{NULL};
};

inline Hkey_guard open_key(const HKEY key, LPCWSTR const subkey, const REGSAM mask,
  const DWORD options = 0)
{
  HKEY result{};
  const auto err = RegOpenKeyExW(key, subkey, options, mask, &result);
  if (err == ERROR_FILE_NOT_FOUND)
    return Hkey_guard{};
  else if (err != ERROR_SUCCESS)
    throw Sys_exception{err, "cannot open registry key"};
  return Hkey_guard{result};
}

/// @returns A pair with created/opened key and disposition information.
inline std::pair<Hkey_guard, DWORD>
create_key(const HKEY key, LPCWSTR const subkey,
  const REGSAM mask, const LPSECURITY_ATTRIBUTES secattrs = {},
  const DWORD options = 0)
{
  HKEY res_key{};
  DWORD res_disp{};
  const auto err = RegCreateKeyExW(key,
    subkey,
    0,
    NULL,
    options,
    mask,
    secattrs,
    &res_key,
    &res_disp);
  if (err != ERROR_SUCCESS)
    throw Sys_exception{err, "cannot create registry key"};
  return std::make_pair(Hkey_guard{res_key}, res_disp);
}

inline void set_value(const HKEY key, LPCWSTR const name, const DWORD type,
  const BYTE* data, const DWORD size)
{
  const auto err = RegSetValueExW(key, name, 0, type, data, size);
  if (err != ERROR_SUCCESS)
    throw Sys_exception{err, "cannot set value of registry key"};
}

template<typename T>
void set_value(const HKEY key, LPCWSTR const name, const T& value)
{
  if constexpr (std::is_same_v<T, DWORD>) {
    set_value(key, name, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(T));
  } else
    static_assert(detail::false_value<T>, "unsupported type specified");
}

inline void remove_value(const HKEY key,
  LPCWSTR const subkey = {}, LPCWSTR const value = {})
{
  const auto err = RegDeleteKeyValueW(key, subkey, value);
  if (err != ERROR_FILE_NOT_FOUND && err != ERROR_SUCCESS)
    throw Sys_exception{err, "cannot remove value of registry key"};
}

template<typename T>
std::optional<T> value(const HKEY key, LPCWSTR const subkey, LPCWSTR const name)
{
  if constexpr (std::is_same_v<T, DWORD>) {
    DWORD result{};
    DWORD result_size{sizeof(result)};
    const auto err = RegGetValueW(key,
      subkey,
      name,
      RRF_RT_REG_DWORD,
      NULL, // FIXME: support REG_DWORD_BIG_ENDIAN
      &result,
      &result_size);
    if (err == ERROR_FILE_NOT_FOUND)
      return std::nullopt;
    else if (err == ERROR_SUCCESS)
      return result;
    else
      throw Sys_exception{err, "cannot get value of registry key"};
  } else
    static_assert(detail::false_value<T>, "unsupported type specified");
}

} // namespace dmitigr::winbase::registry
