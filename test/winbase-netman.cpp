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

#include "../../base/assert.hpp"
#include "../account.hpp"
#include "../netman.hpp"
#include "../security.hpp"

#include <iostream>

#define ASSERT DMITIGR_ASSERT

int main()
{
  try {
    using std::cout;
    using std::wcout;
    using std::endl;
    namespace win = dmitigr::winbase;

    const win::Sid rdp_sid{SECURITY_NT_AUTHORITY,
      SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_REMOTE_DESKTOP_USERS};
    const win::Account rdp_grp{rdp_sid.ptr()};
    const win::Account dmitigr{L"dmitigr"};
    win::netman::local_group_add_members(rdp_grp.name(), {dmitigr.sid()});
  } catch (const std::exception& e) {
    std::clog << "error: " << e.what() << std::endl;
    return 1;
  } catch (...) {
    std::clog << "unknown error" << std::endl;
    return 2;
  }
}
