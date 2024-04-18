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

// Network Management

#pragma once
#pragma comment(lib, "netapi32")

#include "../base/traits.hpp"
#include "exceptions.hpp"

#include <memory>
#include <type_traits>

#include <Lm.h>

namespace dmitigr::winbase::netman {

template<class Info>
using Workstation_info = std::unique_ptr<Info, NET_API_STATUS(*)(LPVOID)>;

template<class Info>
Workstation_info<Info> workstation_info(const LMSTR server_name = {})
{
  using I = std::decay_t<Info>;
  constexpr DWORD level = []
  {
    if constexpr (std::is_same_v<I, WKSTA_INFO_100>) {
      return 100;
    } else if constexpr (std::is_same_v<I, WKSTA_INFO_101>) {
      return 101;
    } else if constexpr (std::is_same_v<I, WKSTA_INFO_102>) {
      return 102;
    } else
      static_assert(false_value<I>);
  }();

  LPBYTE buf{};
  if (const auto e = NetWkstaGetInfo(server_name, level, &buf); e != NERR_Success)
    throw Sys_exception{e, "cannot get workstation network information"};
  return Workstation_info<Info>{reinterpret_cast<I*>(buf), &NetApiBufferFree};
}

} // namespace dmitigr::winbase::netman
