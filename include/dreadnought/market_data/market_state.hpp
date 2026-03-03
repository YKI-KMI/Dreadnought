#pragma once

#include "dreadnought/market_data/order_book.hpp"
#include "dreadnought/core/compiler.hpp"

namespace dreadnought {

// Market data state - isolated cache line
struct alignas(DESTRUCTIVE_SIZE) MarketDataState {
    OrderBook book;
    Timestamp last_update_ts;
    uint64_t sequence_num;
    uint8_t _pad[48];
    
    MarketDataState() noexcept 
        : book(), last_update_ts(0), sequence_num(0), _pad{} {}
    
    FORCE_INLINE void update(const OrderBook& new_book, Timestamp ts) noexcept {
        book = new_book;
        last_update_ts = ts;
        ++sequence_num;
    }
};

} // namespace dreadnought