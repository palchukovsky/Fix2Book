
#pragma once

#include "Message.hpp"

#include <iostream>
#include <map>

namespace fix2book {

namespace Details {

template <bool isAscendingSort, typename Key>
struct Bool2Sort {};
template <typename Key>
struct Bool2Sort<true, Key> {
  using Sort = std::less<Key>;
};
template <typename Key>
struct Bool2Sort<false, Key> {
  using Sort = std::greater<Key>;
};

}  // namespace Details

class Book {
  template <bool isAscendingSort>
  class Side {
   public:
    struct Level {
      double price;
      double value;
    };
    using Key = int64_t;
    using Levels =
        std::map<Key,
                 Level,
                 typename Details::Bool2Sort<isAscendingSort, Key>::Sort>;
    using Iterator = typename Levels::const_iterator;

    Side() = default;
    Side(Side&&) = default;
    Side(const Side&) = delete;
    Side& operator=(Side&&) = default;
    Side& operator=(const Side&) = delete;
    ~Side() = default;

    size_t GetSize() const { return m_levels.size(); }

    Iterator GetLevelAt(const size_t index) const {
      auto result = m_levels.cbegin();
      for (size_t i = 0; i < index; ++i) {
        ++result;
      }
      return result;
    }

    void Add(const double price, const double value) {
      if (!m_levels.emplace(CreateKey(price), Level{price, value}).second) {
        // Adding without removing.
        throw ProtocolError();
      }
    }

    void Set(const Message::MdEntry::MDUpdateAction& action,
             const double price,
             const double val) {
      if (action == Message::MdEntry::MDUpdateAction_New) {
        Add(price, val);
        return;
      }
      const auto it = m_levels.find(CreateKey(price));
      if (it == m_levels.cend()) {
        // Modifying without adding.
        throw ProtocolError();
      }
      if (action == Message::MdEntry::MDUpdateAction_Delete) {
        m_levels.erase(it);
      } else {
        it->second.value = val;
      }
    }

   private:
    static Key CreateKey(const double price) {
      return static_cast<Key>(price * 100000000);
    }

    Levels m_levels;
  };

 public:
  explicit Book(const Message& snapshot) {
    for (auto entry = snapshot.ReadFirstMDEntry(); entry;
         entry = entry->ReadNextMDEntry()) {
      // it better to read vals in this order as parser is streamed, and this
      // order will help with optimization
      const auto& type = entry->ReadMDEntryType();
      const auto& price = entry->ReadMDEntryPx();
      const auto& val = entry->ReadMDEntrySize();
      switch (type) {
        case Message::MdEntry::MDEntryType_Bid:
          m_bids.Add(price, val);
          break;
        case Message::MdEntry::MDEntryType_Offer:
          m_asks.Add(price, val);
          break;
        default:
          break;
      }
    }
  }
  Book(Book&&) = default;
  Book(const Book&) = delete;
  Book& operator=(Book&&) = default;
  Book& operator=(const Book&) = delete;
  ~Book() = default;

  void Update(const Message& message) {
    for (auto entry = message.ReadFirstMDEntry(); entry;
         entry = entry->ReadNextMDEntry()) {
      // it better to read vals in this order as parser is streamed, and this
      // order will help with optimization
      const auto& action = entry->ReadMDUpdateAction();
      const auto& type = entry->ReadMDEntryType();
      const auto& price = entry->ReadMDEntryPx();
      const auto& val = entry->ReadMDEntrySize();
      switch (type) {
        case Message::MdEntry::MDEntryType_Bid:
          m_bids.Set(action, price, val);
          break;
        case Message::MdEntry::MDEntryType_Offer:
          m_asks.Set(action, price, val);
          break;
        default:
          break;
      }
    }
  }

  template <typename OutStream>
  void Print(const size_t size, OutStream& os) const {
    os << "Total SELL: " << m_asks.GetSize() << std::endl;
    {
      const auto levelSize = std::min(size, m_asks.GetSize());
      if (levelSize > 0) {
        auto level = m_asks.GetLevelAt(levelSize - 1);
        for (size_t i = 1; i <= levelSize; ++i, --level) {
          os << '[' << (levelSize - i) << "] price: " << level->second.price
             << " (" << level->second.value << ")" << std::endl;
        }
      }
    }
    os << "==========" << std::endl;
    {
      const auto levelSize = std::min(size, m_asks.GetSize());
      if (levelSize > 0) {
        auto level = m_bids.GetLevelAt(0);
        for (size_t i = 0; i < levelSize; ++i, ++level) {
          os << '[' << i << "] price: " << level->second.price << " ("
             << level->second.value << ")" << std::endl;
        }
      }
    }
    os << "Total BUY: " << m_bids.GetSize() << std::endl;
  }

 private:
  Side<true> m_asks;
  Side<false> m_bids;
};

}  // namespace fix2book