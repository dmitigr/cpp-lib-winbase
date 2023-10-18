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

#include <cstdint>
#include <stdexcept>
#include <string>

namespace dmitigr::winbase::ipc::msg {

/// A message.
class Message {
public:
  /// A serialized message.
  struct Serialized final {
    std::int16_t format{};
    std::string bytes;
  };

  /// The destructor.
  virtual ~Message() = default;

  /// @returns The message identifier.
  virtual std::int64_t id() const noexcept = 0;

  /// @returns A message serialization.
  virtual Serialized to_serialized() const = 0;
};

/// A response message.
class Response : public Message {};

/// An error response message.
class Error : public Response, public std::runtime_error {
public:
  explicit Error(const std::string& what)
    : std::runtime_error{what}
  {}

  /// @returns The error code.
  virtual int code() const noexcept = 0;

  /// Throws this instance.
  virtual void throw_this() const = 0;
};

/// A request message.
class Request : public Message {};

} // namespace dmitigr::winbase::ipc::msg
