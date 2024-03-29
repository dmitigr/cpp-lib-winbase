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

#pragma once
#pragma comment(lib, "ole32")

#include "../base/noncopymove.hpp"
#include "windows.hpp"

#include <new>

#include <Objbase.h>

namespace dmitigr::winbase::com {

template<typename T>
class Taskmem final : private Noncopymove {
public:
  Taskmem() = default;

  ~Taskmem()
  {
    CoTaskMemFree(value_);
  }

  explicit Taskmem(T* const value)
    : value_{value}
  {}

  T* value() noexcept
  {
    return value_;
  }

  const T* value() const noexcept
  {
    return value_;
  }

  bool is_valid() const noexcept
  {
    return static_cast<bool>(value_);
  }

  explicit operator bool() const noexcept
  {
    return is_valid();
  }

private:
  T* value_{};
};

inline auto to_com_string(REFCLSID id)
{
  LPOLESTR str{};
  const HRESULT err{StringFromCLSID(id, &str)};
  if (err == E_OUTOFMEMORY)
    throw std::bad_alloc{};
  else if (!SUCCEEDED(err))
    throw std::runtime_error{"cannot convert CLSID to string: error "
      +std::to_string(err)};
  return Taskmem<OLECHAR>{str};
}

inline std::wstring server_registry_root(REFCLSID id)
{
  return std::wstring{LR"(SOFTWARE\Classes\CLSID\)"}
    .append(to_com_string(id).value());
}

inline std::wstring server_registry_localserver32(REFCLSID id)
{
  return server_registry_root(id).append(LR"(\LocalServer32)");
}

} // namespace dmitigr::winbase::com
