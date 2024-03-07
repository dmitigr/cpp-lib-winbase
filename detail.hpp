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

#include <array>
#include <type_traits>

#pragma once

namespace dmitigr::winbase::detail {

template<typename>
constexpr bool false_value{};

template<class T>
struct Is_std_array : std::false_type {};

template<class T, auto N>
struct Is_std_array<std::array<T, N>> : std::true_type {};

} // namespace dmitigr::winbase::detail
