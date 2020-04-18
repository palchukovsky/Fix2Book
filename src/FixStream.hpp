
#pragma once

#include "BookSet.hpp"
#include "Message.hpp"

#include <istream>

namespace fix2book {

class FixStream {
 public:
  explicit FixStream(const unsigned char soh, std::istream &stream)
      : m_soh(soh), m_stream(stream) {}
  FixStream(FixStream &&) = default;
  FixStream(const FixStream &) = delete;
  FixStream &operator=(FixStream &&) = delete;
  FixStream &operator=(const FixStream &) = delete;
  ~FixStream() = default;

  explicit operator bool() const { return static_cast<bool>(m_stream); }

  FixStream &operator>>(BookSet &books) {
    if (!m_stream) {
      return *this;
    }

    std::string messageContent;
    if (!std::getline(m_stream, messageContent)) {
      return *this;
    }

    books.Update(
        Message(m_soh, messageContent.cbegin(), messageContent.cend()));

    return *this;
  }

 private:
  const unsigned char m_soh;
  std::istream &m_stream;
};

}  // namespace fix2book