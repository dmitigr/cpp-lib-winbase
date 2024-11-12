// -*- C++ -*-
//
// Copyright 2024 Dmitry Igrishin
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

// Windows Terminal Services

#pragma once
#pragma comment(lib, "wtsapi32")

#include "../base/noncopymove.hpp"
#include "exceptions.hpp"

#include <algorithm>
#include <string>

#include <wtsapi32.h>

namespace dmitigr::winbase::wts {

class Session_info_by_class final : private Noncopy {
public:
  ~Session_info_by_class()
  {
    if (value_)
      WTSFreeMemory(value_);
  }

  /**
   * Constructs instance for session information `info_class` for the specified
   * `session_id` on the specified Remote Desktop Session Host `server`.
   */
  Session_info_by_class(const HANDLE server, const DWORD session_id,
    const WTS_INFO_CLASS info_class)
    : info_class_{info_class}
  {
    if (!WTSQuerySessionInformationW(server, session_id, info_class,
        &value_, &value_size_))
      throw Sys_exception{"cannot query session information"};
  }

  Session_info_by_class(Session_info_by_class&& rhs) noexcept
    : info_class_{rhs.info_class_}
    , value_{rhs.value_}
    , value_size_{rhs.value_size_}
  {
    rhs.info_class_ = {};
    rhs.value_ = {};
    rhs.value_size_ = {};
  }

  Session_info_by_class& operator=(Session_info_by_class&& rhs) noexcept
  {
    Session_info_by_class tmp{std::move(rhs)};
    swap(tmp);
    return *this;
  }

  void swap(Session_info_by_class& rhs) noexcept
  {
    using std::swap;
    swap(info_class_, rhs.info_class_);
    swap(value_, rhs.value_);
    swap(value_size_, rhs.value_size_);
  }

  WTS_INFO_CLASS info_class() const noexcept
  {
    return info_class_;
  }

  LPCWSTR value() const noexcept
  {
    return value_;
  }

  std::size_t size_in_bytes() const noexcept
  {
    return value_size_;
  }

  std::size_t size_in_chars() const noexcept
  {
    using Ch = std::wstring_view::value_type;
    return value_size_ >= sizeof(Ch) ? value_size_/sizeof(Ch) - 1 : 0;
  }

  std::string_view to_string_view() const noexcept
  {
    return {reinterpret_cast<LPCSTR>(value()), size_in_bytes()};
  }

  std::wstring_view to_wstring_view() const noexcept
  {
    return {value(), size_in_chars()};
  }

private:
  WTS_INFO_CLASS info_class_{};
  LPWSTR value_{};
  DWORD value_size_{};
};

// -----------------------------------------------------------------------------

class Session_enumeration final : private Noncopy {
public:
  ~Session_enumeration()
  {
    if (info_)
      WTSFreeMemoryExW(WTSTypeSessionInfoLevel1, info_, info_count_);
  }

  /// Constructs enumeration of sessions on a Remote Desktop Session Host `server`.
  explicit Session_enumeration(const HANDLE server)
  {
    DWORD level{1};
    if (!WTSEnumerateSessionsExW(server, &level, 0, &info_, &info_count_))
      throw Sys_exception{"cannot enumerate server sessions"};
  }

  Session_enumeration(Session_enumeration&& rhs) noexcept
    : info_{rhs.info_}
    , info_count_{rhs.info_count_}
  {
    rhs.info_ = {};
    rhs.info_count_ = {};
  }

  Session_enumeration& operator=(Session_enumeration&& rhs) noexcept
  {
    Session_enumeration tmp{std::move(rhs)};
    swap(tmp);
    return *this;
  }

  void swap(Session_enumeration& rhs) noexcept
  {
    using std::swap;
    swap(info_, rhs.info_);
    swap(info_count_, rhs.info_count_);
  }

  const PWTS_SESSION_INFO_1W info() const noexcept
  {
    return info_;
  }

  DWORD count() const noexcept
  {
    return info_count_;
  }

private:
  PWTS_SESSION_INFO_1W info_{};
  DWORD info_count_{};
};

} // namespace dmitigr::winbase::wts
