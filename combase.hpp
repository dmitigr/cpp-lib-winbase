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
#pragma comment(lib, "oleaut32")

#include "../base/noncopymove.hpp"
#include "strconv.hpp"

#include <algorithm>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

#include <Objbase.h>
#include <oaidl.h> // VARIANT
#include <oleauto.h> // VARIANT manipulators

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

// -----------------------------------------------------------------------------
// BSTR
// -----------------------------------------------------------------------------

inline std::wstring_view to_wstring_view(const BSTR bstr)
{
  return {bstr, SysStringLen(bstr)};
}

inline std::wstring to_wstring(const BSTR bstr)
{
  return std::wstring{to_wstring_view(bstr)};
}

inline std::string to_string(const BSTR bstr, const UINT code_page = CP_UTF8)
{
  return winbase::utf16_to_utf8(to_wstring_view(bstr), code_page);
}

// -----------------------------------------------------------------------------
// VARIANT
// -----------------------------------------------------------------------------

struct Variant final {
  ~Variant()
  {
    VariantClear(&data_);
  }

  Variant()
  {
    VariantInit(&data_);
  }

  Variant(const VARIANT dat)
  {
    VariantInit(&data_);
    data_ = dat;
  }

  Variant(const Variant& rhs)
  {
    VariantInit(&data_);
    const auto err = VariantCopyInd(&data_, &rhs.data_);
    if (FAILED(err))
      // FIXME: use wincom::Win_error
      throw std::runtime_error{"cannot copy Variant"};
  }

  Variant& operator=(const Variant& rhs)
  {
    Variant tmp{rhs};
    swap(tmp);
    return *this;
  }

  Variant(Variant&& rhs) noexcept
  {
    VariantInit(&data_);
    data_ = std::move(rhs.data_);
    rhs.data_ = {};
    VariantInit(&rhs.data_);
  }

  Variant& operator=(Variant&& rhs) noexcept
  {
    Variant tmp{std::move(rhs)};
    swap(tmp);
    return *this;
  }

  void swap(Variant& rhs) noexcept
  {
    using std::swap;
    swap(data_, rhs.data_);
  }

  VARENUM type() const noexcept
  {
    return static_cast<VARENUM>(data_.vt);
  }

  const BSTR bstr() const
  {
    check(VT_BSTR, "BSTR string");
    return data_.bstrVal;
  }

  std::string to_string_utf8() const
  {
    check(VT_BSTR, "UTF-8 string");
    return com::to_string(data_.bstrVal);
  }

  std::string to_string_acp() const
  {
    check(VT_BSTR, "ACP string");
    return com::to_string(data_.bstrVal, CP_ACP);
  }

  std::wstring to_wstring() const
  {
    check(VT_BSTR, "UTF-16 string");
    return com::to_wstring(data_.bstrVal);
  }

  std::int8_t to_int8() const
  {
    check(VT_I1, "int8");
    return data_.cVal;
  }

  std::uint8_t to_uint8() const
  {
    check(VT_UI1, "uint8");
    return data_.bVal;
  }

  std::int16_t to_int16() const
  {
    check(VT_I2, "int16");
    return data_.iVal;
  }

  std::uint16_t to_uint16() const
  {
    check(VT_UI2, "uint16");
    return data_.uiVal;
  }

  std::int32_t to_int32() const
  {
    if (is(VT_I4))
      return data_.lVal;
    else if (is(VT_INT))
      return data_.intVal;
    throw_conversion_error("int32");
  }

  std::uint32_t to_uint32() const
  {
    if (is(VT_UI4))
      return data_.ulVal;
    else if (is(VT_UINT))
      return data_.uintVal;
    throw_conversion_error("uint32");
  }

  std::int64_t to_int64() const
  {
    check(VT_I8, "int64");
    return data_.llVal;
  }

  std::uint64_t to_uint64() const
  {
    check(VT_UI8, "uint64");
    return data_.ullVal;
  }

  float to_real32() const
  {
    check(VT_R4, "real32");
    return data_.fltVal;
  }

  double to_real64() const
  {
    check(VT_R8, "real64");
    return data_.dblVal;
  }

  bool to_bool() const
  {
    check(VT_BOOL, "bool");
    return data_.boolVal == VARIANT_TRUE;
  }

  DATE to_date() const
  {
    check(VT_DATE, "DATE");
    return data_.date;
  }

  PVOID to_pvoid() const
  {
    check(VT_BYREF, "PVOID");
    return data_.byref;
  }

  const VARIANT& data() const noexcept
  {
    return data_;
  }

private:
  VARIANT data_{};

  bool is(const VARENUM tp) const noexcept
  {
    return data_.vt == tp;
  }

  [[noreturn]] static void throw_conversion_error(const std::string& tpnm)
  {
    throw std::logic_error{"cannot convert Variant to "+tpnm};
  }

  void check(const VARENUM tp, const std::string& tpnm) const
  {
    if (!is(tp))
      throw_conversion_error(tpnm);
  }
};

} // namespace dmitigr::winbase::com
