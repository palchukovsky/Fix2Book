
#pragma once

#include <exception>

namespace fix2book {

class Exception : public std::exception {};

class ProtocolError : public Exception {
 public:
  ~ProtocolError() override = default;

  const char* what() const noexcept override { return "protocol error"; }
};

class UnknownProtocolFieldError final : public ProtocolError {};

}  // namespace fix2book
