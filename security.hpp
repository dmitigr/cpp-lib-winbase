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

#include "windows.hpp"

#include <stdexcept>
#include <vector>

namespace dmitigr::winbase {

class Token_info final {
public:
  Token_info() = default;

  Token_info(const HANDLE token, const TOKEN_INFORMATION_CLASS type)
  {
    reset(token, type);
  }

  void reset(const HANDLE token, const TOKEN_INFORMATION_CLASS type)
  {
    DWORD sz{};
    GetTokenInformation(token, type, nullptr, 0, &sz);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
      throw std::runtime_error{last_error_message()};

    buf_.resize(sz);
    if (!GetTokenInformation(token, type, buf_.data(), buf_.size(), &sz))
      throw std::runtime_error{last_error_message()};

    type_ = type;
  }

  TOKEN_INFORMATION_CLASS type() const noexcept
  {
    return type_;
  }

  template<class T>
  const T& data() const noexcept
  {
    return *reinterpret_cast<const T*>(buf_.data());
  }

  template<class T>
  T& data() noexcept
  {
    return const_cast<T&>(static_cast<const Token_info*>(this)->data<T>());
  }

private:
  TOKEN_INFORMATION_CLASS type_{};
  std::vector<char> buf_;
};

} // namespace dmitigr::winbase
