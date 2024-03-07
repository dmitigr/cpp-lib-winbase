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

#include "../base/assert.hpp"
#include "../rnd/uuid.hpp"
#include "detail.hpp"
#include "error.hpp"
#include "exceptions.hpp"

#include <array>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace dmitigr::winbase {

class Smbios_firmware_table final {
public:
  struct Header final {
    BYTE used_20_calling_method{};
    BYTE major_version{};
    BYTE minor_version{};
    BYTE dmi_revision{};
    DWORD length{};
  };
  static_assert(std::is_standard_layout_v<Header>);

  struct Structure {
    BYTE type{};
    BYTE length{};
    WORD handle{};
  };
  static_assert(std::is_standard_layout_v<Structure>);

  struct Bios_info final : Structure {
    std::string vendor;
    std::string version;
    std::string release_date;
    BYTE rom_size{};
  };

  struct Sys_info final : Structure {
    std::string manufacturer;
    std::string product;
    std::string version;
    std::string serial_number;
    rnd::Uuid uuid{};
  };

  struct Baseboard_info final : Structure {
    std::string manufacturer;
    std::string product;
    std::string version;
    std::string serial_number;
  };

  Smbios_firmware_table(const BYTE* const data, const std::size_t size)
    : data_(size)
  {
    if (size < sizeof(Header))
      throw std::invalid_argument{"cannot create SMBIOS firmware table:"
        " invalid data size"};
    std::memcpy(data_.data(), data, size);
  }

  static Smbios_firmware_table from_system()
  {
    Smbios_firmware_table result;
    auto& rd = result.data_;
    rd.resize(GetSystemFirmwareTable('RSMB', 0, nullptr, 0));
    if (!rd.empty() && GetSystemFirmwareTable('RSMB', 0, rd.data(), rd.size()))
      return result;
    throw Sys_exception{"cannot get SMBIOS firmware table"};
  }

  Header header() const
  {
    return *reinterpret_cast<const Header*>(data_.data());;
  }

  const std::vector<BYTE>& raw() const noexcept
  {
    return data_;
  }

  Bios_info bios_info() const
  {
    const auto* const s = structure(0);
    auto result = make_structure<Bios_info>(*s);
    result.vendor = field<std::string>(s, 0x4);
    result.version = field<std::string>(s, 0x5);
    result.release_date = field<std::string>(s, 0x8);
    result.rom_size = field<BYTE>(s, 0x9);
    return result;
  }

  Sys_info sys_info() const
  {
    const auto* const s = structure(1);
    auto result = make_structure<Sys_info>(*s);
    result.manufacturer = field<std::string>(s, 0x4);
    result.product = field<std::string>(s, 0x5);
    result.version = field<std::string>(s, 0x6);
    result.serial_number = field<std::string>(s, 0x7);
    result.uuid = field<std::array<BYTE, 16>>(s, 0x8);
    return result;
  }

  std::optional<Baseboard_info> baseboard_info() const
  {
    const auto* const s = structure(2, true);
    if (!s)
      return std::nullopt;
    auto result = make_structure<Baseboard_info>(*s);
    result.manufacturer = field<std::string>(s, 0x4);
    result.product = field<std::string>(s, 0x5);
    result.version = field<std::string>(s, 0x6);
    result.serial_number = field<std::string>(s, 0x7);
    return result;
  }

private:
  std::vector<BYTE> data_;

  Smbios_firmware_table() = default;

  template<class S>
  static S make_structure(const Structure& s)
  {
    static_assert(std::is_base_of_v<Structure, S>);
    S result;
    result.type = s.type;
    result.length = s.length;
    result.handle = s.handle;
    return result;
  }

  const Structure* structure(const BYTE type,
    const bool no_throw_if_not_found = false) const
  {
    for (auto* s = first_structure(); s; s = next_structure(s)) {
      if (s->type == type)
        return s;
    }
    if (no_throw_if_not_found)
      return nullptr;
    else
      throw std::runtime_error{"no BIOS information structure of type "
        +std::to_string(type)+" found in SMBIOS"};
  }

  static const char* unformed_section(const Structure* const s) noexcept
  {
    DMITIGR_ASSERT(s);
    return reinterpret_cast<const char*>(s) + s->length;
  }

  const Structure* first_structure() const noexcept
  {
    return reinterpret_cast<const Structure*>(data_.data() + sizeof(Header));
  }

  const Structure* next_structure(const Structure* const s) const noexcept
  {
    DMITIGR_ASSERT(s);
    bool is_prev_char_zero{};
    const auto hdr = header();
    const auto* const fst = first_structure();
    for (const char* ptr{unformed_section(s)};
         ptr + 1 - reinterpret_cast<const char*>(fst) < hdr.length; ++ptr) {
      if (*ptr == 0) {
        if (is_prev_char_zero)
          return reinterpret_cast<const Structure*>(ptr + 1);
        else
          is_prev_char_zero = true;
      } else
        is_prev_char_zero = false;
    }
    return nullptr;
  }

  template<typename T>
  static T field(const Structure* const s, const std::ptrdiff_t offset)
  {
    DMITIGR_ASSERT(offset >= 0x0);
    using Dt = std::decay_t<T>;
    using Qword = unsigned __int64;
    const BYTE* const ptr = reinterpret_cast<const BYTE*>(s) + offset;
    if constexpr (std::is_same_v<Dt, std::string>) {
      const int idx = *ptr;
      if (!idx)
        throw std::runtime_error{"cannot get string of structure "
          +std::string{static_cast<int>(s->type)}+" at offset "
          +std::to_string(offset)+": string field references no string"};
      const char* str = unformed_section(s);
      for (int i{1}; i < idx; ++i) {
        std::string_view view{str};
        str = view.data() + view.size() + 1;
        DMITIGR_ASSERT(*str != 0);
      }
      return std::string{str};
    } else if constexpr (detail::Is_std_array<Dt>::value) {
      using V = typename Dt::value_type;
      if constexpr (std::is_same_v<V, BYTE>) {
        Dt result;
        std::memcpy(result.data(), ptr, result.size());
        return result;
      } else
        static_assert(detail::false_value<V>, "unsupported type");
    } else if constexpr (
      std::is_same_v<Dt, BYTE>  || std::is_same_v<Dt, WORD> ||
      std::is_same_v<Dt, DWORD> || std::is_same_v<Dt, Qword>) {
      return *reinterpret_cast<const Dt*>(ptr);
    } else
      static_assert(detail::false_value<T>, "unsupported type");
  }
};

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
  DMITIGR_ASSERT(sz == size_with_null - 1);
  return result;
}

inline std::wstring computer_name(const COMPUTER_NAME_FORMAT type)
{
  DWORD sz{};
  GetComputerNameExW(type, nullptr, &sz);
  if (const auto e = GetLastError(); e != ERROR_MORE_DATA)
    throw Sys_exception{e, "cannot get required size of computer name of type "
      +std::to_string(type)};
  std::wstring result(sz, L'\0');
  if (!GetComputerNameExW(type, result.data(), &sz))
    throw Sys_exception{"cannot get computer name of type "+std::to_string(type)};
  return result;
}

inline std::string cpu_architecture_string(const WORD value)
{
  switch (value) {
  case PROCESSOR_ARCHITECTURE_AMD64:
    return "x64";
  case PROCESSOR_ARCHITECTURE_ARM:
    return "arm";
  case PROCESSOR_ARCHITECTURE_ARM64:
    return "arm64";
  case PROCESSOR_ARCHITECTURE_IA64:
    return "ia64";
  case PROCESSOR_ARCHITECTURE_INTEL:
    return "x86";
  case PROCESSOR_ARCHITECTURE_UNKNOWN:
    [[fallthrough]];
  default:
    return "unknown";
  }
}

} // namespace dmitigr::winbase
