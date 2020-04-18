
#pragma once

#include "Exception.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

namespace fix2book {

class Content {
 public:
  using Iterator = std::string::const_iterator;

  explicit Content(const unsigned char soh, Iterator begin, Iterator end)
      : m_soh(soh),
        m_begin(std::move(begin)),
        m_end(std::move(end)),
        m_cursor(m_begin) {}
  Content(Content &&) = default;
  Content(const Content &) = delete;
  Content &operator=(Content &&) = delete;
  Content &operator=(const Content &) = delete;
  ~Content() = default;

  unsigned char GetSoh() const { return m_soh; }

 protected:
  template <typename Result, typename Read>
  Result ReadField(const std::string &tag, const Read &read) const {
    if (!FindTag(tag, m_cursor)) {
      throw UnknownProtocolFieldError();
    }
    return read(m_cursor);
  }

  template <typename Result>
  Result ReadIntField(const std::string &tag) const {
    return ReadField<Result>(
        tag, [this](Iterator &it) { return ReadIntValue<Result>(it); });
  }

  double ReadDoubleField(const std::string &tag) const {
    return ReadField<double>(
        tag, [this](Iterator &it) { return ReadDoubleValue<double>(it); });
  }

  std::string ReadStringField(const std::string &tag) const {
    return ReadField<std::string>(
        tag, [this](Iterator &it) { return ReadStringValue(it); });
  }

  bool FindTagBegin(const std::string &tag, Iterator &cursor) const {
    auto it = cursor;
    auto end = m_end;
    for (;;) {
      while (end > it &&
             static_cast<size_t>(std::distance(end, it)) >= tag.size() &&
             !std::equal(it, it + tag.size(), tag.cbegin(), tag.cend())) {
        it = std::find(it, end, m_soh);
        if (it == end) {
          break;
        }
        ++it;  // skipping SOH
      }
      if (it < end) {
        cursor = it;
        return true;
      }
      if (end == cursor) {
        break;
      }
      // checking tag in the first part of message
      end = cursor;
      it = m_begin;
    }
    return false;
  }

  bool FindTag(const std::string &tag, Iterator &cursor) const {
    if (!FindTagBegin(tag, cursor)) {
      return false;
    }
    cursor += tag.size();
    return true;
  }

  void CheckValueCursor(const Iterator &cursor) const {
    if (*cursor == m_soh || cursor >= m_end) {
      throw ProtocolError();
    }
  }

  template <typename Iterator>
  std::string ReadStringValue(Iterator &cursor) const {
    CheckValueCursor(cursor);
    const auto end = std::find(cursor, m_end, m_soh);
    if (end == m_end) {
      throw ProtocolError();
    }
    const std::string result(cursor, end);
    cursor = std::next(end);
    return result;
  }

  template <typename Result, typename Iterator>
  Result ReadIntValue(Iterator &cursor) const {
    CheckValueCursor(cursor);
    Result result = 0;
    auto it = cursor;
    do {
      result = result * 10 + (*it++ - '0');
      if (it >= m_end) {
        throw ProtocolError();
      }
    } while (*it != m_soh);
    ++it;
    cursor = std::move(it);
    return result;
  }

  template <typename Result, typename Iterator>
  Result ReadDoubleValue(Iterator &cursor) const {
    CheckValueCursor(cursor);
    Result result = .0;
    auto it = cursor;

    // https://tinodidriksen.com/2011/05/cpp-convert-string-to-double-speed/
    // https://tinodidriksen.com/uploads/code/cpp/speed-string-to-double.cpp
    // (native)

    auto isNegative = false;
    if (*it == '-') {
      isNegative = true;
      ++it;
    }

    while (*it >= '0' && *it <= '9') {
      result = (result * 10.0) + (*it - '0');
      ++it;
      if (it >= m_end) {
        throw ProtocolError();
      }
    }
    if (*it == '.') {
      auto f = 0.0;
      auto n = 0;
      ++it;
      while (*it >= '0' && *it <= '9') {
        f = (f * 10.0) + (*it - '0');
        ++it;
        if (it >= m_end) {
          throw ProtocolError();
        }
        ++n;
      }
      if (*it != m_soh) {
        throw ProtocolError();
      }
      result += f / std::pow(10.0, n);
    } else if (*it != m_soh) {
      throw ProtocolError();
    }
    ++it;

    if (isNegative) {
      result = -result;
    }

    cursor = std::move(it);
    return result;
  }

  const unsigned char m_soh;
  Iterator m_begin;
  Iterator m_end;
  mutable Iterator m_cursor;
};

class Message : public Content {
  friend class MdEntry;

 public:
  class MdEntry : public Content {
   public:
    // 269
    enum MDEntryType {
      MDEntryType_Bid = 0,
      MDEntryType_Offer = 1,
      MDEntryType_Trade = 2,
      MDEntryType_Index = 3,
      MDEntryType_SettlementPrice = 6,
    };
    // 279
    enum MDUpdateAction {
      MDUpdateAction_New = 0,
      MDUpdateAction_Change = 1,
      MDUpdateAction_Delete = 2,
    };

    explicit MdEntry(const Iterator &begin,
                     const Iterator &end,
                     const size_t numberOfNextEntities,
                     std::string firstTag,
                     const Message &message)
        : Content(message.GetSoh(), begin, end),
          m_numbeOfNextEntities(numberOfNextEntities),
          m_firstTag(std::move(firstTag)),
          m_message(message) {}

    MDUpdateAction ReadMDUpdateAction() const {
      const auto &result = ReadIntField<uint8_t>("279=");
      switch (result) {
        case MDUpdateAction_New:
        case MDUpdateAction_Change:
        case MDUpdateAction_Delete:
          break;
        default:
          throw ProtocolError();
      }
      return static_cast<MDUpdateAction>(result);
    }

    MDEntryType ReadMDEntryType() const {
      const auto &result = ReadIntField<uint8_t>("269=");
      switch (result) {
        case MDEntryType_Bid:
        case MDEntryType_Offer:
        case MDEntryType_Trade:
        case MDEntryType_Index:
        case MDEntryType_SettlementPrice:
          break;
        default:
          throw ProtocolError();
      }
      return static_cast<MDEntryType>(result);
    }

    double ReadMDEntryPx() const { return ReadDoubleField("270="); }
    double ReadMDEntrySize() const { return ReadDoubleField("271="); }

    std::unique_ptr<MdEntry> ReadNextMDEntry() const {
      if (m_numbeOfNextEntities == 0) {
        return {};
      }
      return m_message.ReadMDEntry(m_end, m_numbeOfNextEntities, m_firstTag);
    }

   private:
    const size_t m_numbeOfNextEntities;
    const std::string m_firstTag;
    const Message &m_message;
  };

  explicit Message(const unsigned char soh,
                   const Iterator &begin,
                   const Iterator &end)
      : Content(soh, begin, end) {
    Normalize();
  }

  char GetType() const { return m_type; }

  size_t ReadMsgSecNum() const { return ReadIntField<size_t>("34="); }
  size_t ReadNoMDEntries() const { return ReadIntField<size_t>("268="); }
  std::string ReadSymbol() const { return ReadStringField("55="); }

  std::unique_ptr<MdEntry> ReadFirstMDEntry() const {
    const auto &size = ReadNoMDEntries();
    if (!size) {
      return {};
    }
    auto begin = m_cursor;
    const auto it = std::find(m_cursor, m_end, '=');
    if (it == m_end) {
      throw ProtocolError();
    }
    return ReadMDEntry(begin, size, {begin, std::next(it)});
  }

 private:
  void Normalize() {
    m_end = std::find_if(std::make_reverse_iterator(m_end),
                         std::make_reverse_iterator(m_begin),
                         [](const int ch) { return ch != '\r' && ch != '\n'; })
                .base();

    static const std::string protoTest = "8=FIX.4.4";
    static const std::string lenTagTest = "9=";
    static const std::string typeTagTest = "35=";
    static const std::string checksumTagTest = "10=";
    static const auto minLen = protoTest.size() + 1 /* SOH */ +
                               lenTagTest.size() + 1 /* SOH */ +
                               typeTagTest.size() + 1 /* value */ + 1 /* SOH */
                               + checksumTagTest.size() + 3 /* checksum value */
                               + 1 /* SOH */;
    const auto messageSize = std::distance(m_begin, m_end);
    if (messageSize <= 0 || static_cast<size_t>(messageSize) <= minLen) {
      throw ProtocolError();
    }
    if (*std::prev(m_end) != m_soh) {
      throw ProtocolError();
    }

    // Checks protocol and version.
    if (!std::equal(m_begin, m_begin + protoTest.size(), protoTest.cbegin(),
                    protoTest.cend())) {
      throw ProtocolError();
    }
    m_cursor = m_begin + protoTest.size() + 1 /* SOH */;
    if (*std::prev(m_cursor) != m_soh) {
      throw ProtocolError();
    }

    // Extracts and checks message length.
    if (!std::equal(m_cursor, m_cursor + lenTagTest.size(), lenTagTest.cbegin(),
                    lenTagTest.cend())) {
      throw ProtocolError();
    }
    m_cursor += lenTagTest.size();
    const auto &len = ReadIntValue<size_t>(m_cursor);
    {
      const auto realLen = m_end - m_cursor;
      if (realLen <= 0 || static_cast<size_t>(realLen) < len + 7) {
        throw ProtocolError();
      }
    }

    // Extracts message control sum.
    auto checksumBegin = m_cursor + len;
    if (*std::prev(checksumBegin) != m_soh) {
      throw ProtocolError();
    }
    if (!std::equal(checksumBegin, checksumBegin + checksumTagTest.size(),
                    checksumTagTest.cbegin(), checksumTagTest.cend())) {
      throw ProtocolError();
    }
    const auto &controlChecksum = CalcCheckSum(m_begin, checksumBegin);
    {
      auto cursor = checksumBegin + 3;
      const auto &messageChecksum = ReadIntValue<size_t>(cursor);
      if (controlChecksum != messageChecksum || cursor != m_end) {
        throw ProtocolError();
      }
    }

    // Extracts message type.
    if (!std::equal(m_cursor, m_cursor + typeTagTest.size(),
                    typeTagTest.cbegin(), typeTagTest.cend())) {
      throw ProtocolError();
    }
    m_cursor += typeTagTest.size();
    if (*m_cursor == m_soh) {
      throw ProtocolError();
    }
    m_type = *m_cursor;
    ++m_cursor;
    if (*m_cursor != m_soh) {
      throw ProtocolError();
    }
    ++m_cursor;

    // Now begin and end show range inside message, without checked fields.
    m_begin = m_cursor;
    m_cursor = m_begin;
    m_end = checksumBegin;
  }

  template <typename It>
  uint32_t CalcCheckSum(It begin, const It &end) const {
    uint32_t result = 0;
    for (; begin != end; ++begin) {
      result += static_cast<decltype(result)>(*begin == m_soh ? 1 : *begin);
    }
    return result % 256;
  }

  std::unique_ptr<MdEntry> ReadMDEntry(Iterator begin,
                                       const size_t size,
                                       std::string tag) const {
    if (size > 1) {
      m_cursor = begin + tag.size();
      if (!FindTagBegin(tag, m_cursor)) {
        throw ProtocolError();
      }
    } else {
      m_cursor = m_end;
    }
    return std::make_unique<MdEntry>(begin, m_cursor, size - 1, std::move(tag),
                                     *this);
  }

  char m_type;
};

}  // namespace fix2book
