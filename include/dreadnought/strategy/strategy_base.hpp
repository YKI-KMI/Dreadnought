#pragma once
#include "dreadnought/core/types.hpp"
#include "dreadnought/core/compiler.hpp"
#include "dreadnought/market_data/order_book.hpp"
#include <cstdint>

namespace dreadnought {

struct Signal {
    Side side;
    Price price;
    Quantity qty;
    uint8_t confidence;
    bool valid;
    uint8_t _pad[2];
};

template<typename Derived>
struct StrategyBase {
    FORCE_INLINE void on_tick(const OrderBook& book, Timestamp ts) noexcept {
        static_cast<Derived*>(this)->on_tick_impl(book, ts);
    }
    
    FORCE_INLINE void on_trade(Price price, Quantity qty, Side side, Timestamp ts) noexcept {
        static_cast<Derived*>(this)->on_trade_impl(price, qty, side, ts);
    }
    
    FORCE_INLINE bool should_send_order() const noexcept {
        return static_cast<const Derived*>(this)->should_send_order_impl();
    }
    
    FORCE_INLINE Side get_order_side() const noexcept {
        return static_cast<const Derived*>(this)->get_order_side_impl();
    }
    
    FORCE_INLINE Price get_order_price() const noexcept {
        return static_cast<const Derived*>(this)->get_order_price_impl();
    }
    
    FORCE_INLINE Quantity get_order_qty() const noexcept {
        return static_cast<const Derived*>(this)->get_order_qty_impl();
    }
};

} // namespace dreadnought
