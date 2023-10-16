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

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace dmitigr::winbase::ipc::msg {

/// A message.
class Message {
public:
  /// The destructor.
  virtual ~Message() = default;

  /// @returns A string (or bytes) representation of message.
  virtual std::string to_string() const = 0;

  /// @returns A message format code.
  virtual int format() const noexcept = 0;

  /// The default constructor.
  Message() = default;

  /// The constructor.
  explicit Message(const std::int64_t id)
    : id_{id}
  {}

  /// @returns The message identifier.
  std::int64_t id() const noexcept
  {
    return id_;
  }

private:
  std::int64_t id_{};
};

/// A response message.
class Response : public Message {
protected:
  using Message::Message;
};

/// An error response message.
class Error : public Response, public std::runtime_error {
public:
  /// The default constructor.
  Error() = default;

  /// The constructor.
  Error(const std::int64_t id, const int code, const std::string& message)
    : Response{id}
    , std::runtime_error{message}
    , code_{code}
  {}

  /// @returns The error code.
  int code() const noexcept
  {
    return code_;
  }

  /// Throws this instance.
  virtual void throw_this() const = 0;

private:
  int code_{};
};

/// A request message.
class Request : public Message {
public:
  /// The default constructor.
  Request()
    : Message{seq_++}
  {}

private:
  inline static std::atomic_int64_t seq_{1};
};

} // namespace dmitigr::winbase::ipc::msg
