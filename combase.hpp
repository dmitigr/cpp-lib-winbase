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
#include "../base/traits.hpp"
#include "strconv.hpp"

#include <algorithm>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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
// SAFEARRAY
// -----------------------------------------------------------------------------

/// A wrapper around SAFEARRAY.
class Safe_array final {
public:
  /// Destroys the underlying data if `is_owns()`.
  ~Safe_array()
  {
    if (is_owns())
      SafeArrayDestroy(data_);
  }

  /// Constructs an empty instance.
  Safe_array() = default;

  /**
   * @brief Constructs an array of elements of the specified type.
   *
   * @param vt The array elements type.
   * @param rgsa The bounds of each dimension of the array.
   */
  Safe_array(const VARTYPE vt, std::vector<SAFEARRAYBOUND> rgsa)
    : is_owns_{true}
    , data_{SafeArrayCreate(vt, rgsa.size(), rgsa.data())}
  {
    if (!data_)
      throw std::runtime_error{"cannot create Safe_array"};
  }

  /**
   * @brief Constructs an array.
   *
   * @param data The existing array.
   * @param is_owns The value of `true` indicates ownership transferring of
   * `data` to this instance.
   */
  Safe_array(SAFEARRAY* const data, const bool is_owns)
    : is_owns_{is_owns}
    , data_{data}
  {}

  /**
   * @brief Copies the instance.
   *
   * @par Effects
   * `is_owns() == rhs.is_owns()`.
   */
  Safe_array(const Safe_array& rhs)
  {
    if (rhs.data_) {
      if (rhs.is_owns_) {
        auto copy = rhs.copy();
        swap(copy);
      } else
        data_ = rhs.data_;
      is_owns_ = rhs.is_owns_;
    }
  }

  /// Copy-assignable.
  Safe_array& operator=(const Safe_array& rhs)
  {
    Safe_array tmp{rhs};
    swap(tmp);
    return *this;
  }

  /// Move-constructible.
  Safe_array(Safe_array&& rhs) noexcept
    : is_owns_{rhs.is_owns_}
    , data_{rhs.data_}
  {
    rhs.is_owns_ = {};
    rhs.data_ = {};
  }

  /// Move-assignable.
  Safe_array& operator=(Safe_array&& rhs) noexcept
  {
    Safe_array tmp{std::move(rhs)};
    swap(tmp);
    return *this;
  }

  /// Swaps this instance with `rhs`.
  void swap(Safe_array& rhs) noexcept
  {
    using std::swap;
    swap(is_owns_, rhs.is_owns_);
    swap(data_, rhs.data_);
  }

  /// @returns A copy of this instance which owns the underlying data.
  Safe_array copy() const
  {
    Safe_array result;
    const auto err = SafeArrayCopy(data_, &result.data_);
    if (FAILED(err))
      // FIXME: use wincom::Win_error
      throw std::runtime_error{"cannot copy Safe_array"};
    result.is_owns_ = true;
    return result;
  }

  /// An array slice.
  class Slice final : Noncopymove {
  public:
    /// Decrements the lock count of the array.
    ~Slice()
    {
      if (self_.data_)
        SafeArrayUnlock(self_.data_);
    }

    /**
     * @tparam Can be `BSTR`, `IUnknown`, `IDispatch` and `VARIANT`.
     *
     * @returns The pointer to the first element of the underlying part of array
     * which is represented by this slice.
     */
    template<typename T>
    const T* array() const
    {
      using std::is_same_v;
      using D = std::decay_t<T>;
      USHORT feat{};
      const char* msg{};
      if constexpr (is_same_v<D, BSTR>) {
        feat = FADF_BSTR;
      } else if constexpr (is_same_v<D, IUnknown>) {
        feat = FADF_UNKNOWN;
      } else if constexpr (is_same_v<D, IDispatch>) {
        feat = FADF_DISPATCH;
      } else if constexpr (is_same_v<D, VARIANT>) {
        feat = FADF_VARIANT;
      } else
        static_assert(false_value<T>);
      if (!bool(self_.features() & feat))
        throw std::runtime_error{"cannot get array of requested type"};
      return static_cast<const T*>(self_.data().pvData) + absolute_offset_;
    }

    /// @overload
    template<typename T>
    T* array()
    {
      return const_cast<T*>(static_cast<const Slice*>(this)->array<T>());
    }

    /// @returns The dimension of this slice.
    USHORT dimension() const noexcept
    {
      return dim_;
    }

    /// @returns The size of this slice.
    std::size_t size() const noexcept
    {
      return size_;
    }

    /// @returns The lower bound of this slice.
    LONG lower_bound() const
    {
      return self_.data().rgsabound[dim_].lLbound;
    }

    /// @returns `true` if this slice represents a vector.
    bool is_vector() const
    {
      return dimension() == self_.dimension_count() - 1;
    }

    /// @returns The slice count.
    std::size_t slice_count() const
    {
      return !is_vector() * self_.data().rgsabound[dim_].cElements;
    }

    /**
     * @param index Zero-based slice index.
     *
     * @returns The specified slice.
     */
    Slice slice(const std::size_t index) const
    {
      const auto& data = self_.data();
      if (!(dim_ + 1 < data.cDims))
        throw std::invalid_argument{"Safe_array dimension overflow"};
      else if (!(index < data.rgsabound[dim_].cElements))
        throw std::invalid_argument{"Safe_array index overflow"};
      return Slice{self_, static_cast<USHORT>(dim_ + 1), index, absolute_offset_};
    }

  private:
    friend Safe_array;

    const Safe_array& self_;
    USHORT dim_{};
    std::size_t absolute_offset_{};
    std::size_t size_{};

    /// Increments the lock count of the array.
    Slice(const Safe_array& self, const USHORT dim,
      const std::size_t slice_offset,
      const std::size_t absolute_offset)
      : self_{self}
      , dim_{dim}
    {
      {
        const auto err = SafeArrayLock(self.data_);
        if (FAILED(err))
          throw std::runtime_error{"cannot create Safe_array::Slice:"
            " cannot lock SAFEARRAY"};
      }

      size_ = [&]
      {
        std::size_t result{1};
        const USHORT dcount{self_.dimension_count()};
        for (USHORT d{dim_}; d < dcount; ++d)
          result *= self_.data().rgsabound[d].cElements;
        return result;
      }();
      absolute_offset_ = absolute_offset + slice_offset*size_;
    }
  };

  /// @returns `true` if this instance is owns the underlying data.
  bool is_owns() const noexcept
  {
    return is_owns_ && data_;
  }

  /// @returns `true` if this instance has the underlying data.
  bool has_data() const noexcept
  {
    return data_;
  }

  /// @returns The dimension count.
  USHORT dimension_count() const
  {
    return data().cDims;
  }

  /// @returns The feature flags.
  USHORT features() const
  {
    return data().fFeatures;
  }

  /// @returns The element size.
  ULONG element_size() const
  {
    return data().cbElements;
  }

  /// @returns The value of lock counter of the underlying data.
  ULONG lock_count() const
  {
    return data().cLocks;
  }

  /**
   * @brief Increments the lock count of the underying array.
   *
   * @returns The slice of zero dimension.
   */
  Slice slice() const
  {
    return Slice{*this, 0, 0, 0};
  }

  /// @returns The underlying data.
  const SAFEARRAY& data() const
  {
    if (!has_data())
      throw std::logic_error{"cannot use invalid instance of Safe_array"};
    return *data_;
  }

private:
  bool is_owns_{};
  SAFEARRAY* data_{};
};

// -----------------------------------------------------------------------------
// VARIANT
// -----------------------------------------------------------------------------

class Variant final {
public:
  ~Variant()
  {
    if (is_owns())
      VariantClear(&data_);
  }

  Variant()
  {
    VariantInit(&data_);
  }

  Variant(const VARIANT dat, const bool is_owns)
    : is_owns_{is_owns}
  {
    VariantInit(&data_);
    data_ = dat;
  }

  Variant(const Variant& rhs)
  {
    VariantInit(&data_);
    if (rhs.is_owns_) {
      auto copy = rhs.copy();
      swap(copy);
    } else
      data_ = rhs.data_;
    is_owns_ = rhs.is_owns_;
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
    rhs.is_owns_ = {};
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
    swap(is_owns_, rhs.is_owns_);
    swap(data_, rhs.data_);
  }

  /// @returns A copy of this instance which owns the underlying data.
  Variant copy() const
  {
    Variant result;
    const auto err = VariantCopyInd(&data_, &result.data_);
    if (FAILED(err))
      // FIXME: use wincom::Win_error
      throw std::runtime_error{"cannot copy Variant"};
    result.is_owns_ = true;
    return result;
  }

  /// @returns `true` if this instance is owns the underlying data.
  bool is_owns() const noexcept
  {
    return is_owns_;
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
  bool is_owns_{};
  mutable VARIANT data_{};

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
