# -*- cmake -*-
#
# Copyright 2023 Dmitry Igrishin
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# ------------------------------------------------------------------------------
# Info
# ------------------------------------------------------------------------------

dmitigr_libs_set_library_info(winbase 0 0 0 "Windows base stuff")

# ------------------------------------------------------------------------------
# Sources
# ------------------------------------------------------------------------------

if (NOT WIN32)
  return()
endif()

set(dmitigr_winbase_headers
  dialog.hpp
  error.hpp
  hlocal.hpp
  menu.hpp
  resource.hpp
  strconv.hpp
  windows.hpp
)
