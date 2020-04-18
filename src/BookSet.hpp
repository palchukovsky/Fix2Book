
#pragma once

#include "Book.hpp"
#include "Message.hpp"

#include <iostream>
#include <memory>
#include <unordered_map>

namespace fix2book {

class BookSet {
 public:
  BookSet() = default;
  BookSet(BookSet &&) = default;
  BookSet(const BookSet &) = delete;
  BookSet &operator=(BookSet &&) = default;
  BookSet &operator=(const BookSet &) = delete;
  ~BookSet() = default;

  size_t GetRevision() const { return m_seqNum; }

  template <typename OutStream>
  void Print(const size_t revision, const size_t size, OutStream &os) const {
    for (const auto &book : m_books) {
      if (revision > book.second.first) {
        continue;
      }
      os << std::endl << book.first << ":" << std::endl;
      book.second.second->Print(size, os);
    }
  }

  void Update(const Message &message) {
    switch (message.GetType()) {
      case 'W':  // snapshot
      case 'X':  // incremental update
        break;
      default:
        return;
    }
    const auto &seqNum = message.ReadMsgSecNum();
    if (m_seqNum >= seqNum) {
      return;
    }

    auto &book = m_books[message.ReadSymbol()];
    if (message.GetType() == 'W') {
      book.second = std::make_shared<Book>(message);
    } else if (!book.second) {
      // no snapshot for book
      throw ProtocolError();
    } else {
      book.second->Update(message);
    }

    m_seqNum = book.first = seqNum;
  }

 private:
  size_t m_seqNum = 0;
  std::unordered_map<std::string, std::pair<size_t, std::shared_ptr<Book>>>
      m_books;
};

}  // namespace fix2book
