#pragma once

#include "dreadnought/core/types.hpp"
#include "dreadnought/core/compiler.hpp"
#include <array>
#include <algorithm>

namespace dreadnought {

// L1-resident order book optimized for cache locality
// Target: Entire book fits in ~8KB
struct alignas(DESTRUCTIVE_SIZE) OrderBook {
    std::array<PriceLevel, MAX_PRICE_LEVELS> bids;
    std::array<PriceLevel, MAX_PRICE_LEVELS> asks;
    uint8_t bid_count;
    uint8_t ask_count;
    uint8_t _pad[6]; // Pad to 64-byte boundary
    
    OrderBook() noexcept : bid_count(0), ask_count(0), _pad{} {
        for (auto& level : bids) level.clear();
        for (auto& level : asks) level.clear();
    }
    
    // Update bid level - O(1) amortized for top-of-book updates
    FORCE_INLINE void update_bid(Price price, Quantity qty) noexcept {
        PREFETCH_WRITE(&bids[0]);
        
        // Fast path: top of book update
        if (bid_count > 0 && bids[0].price == price) {
            bids[0].qty = qty;
            if (qty == 0) {
                remove_bid_level(0);
            }
            return;
        }
        
        // Binary search for price level
        int idx = find_bid_level(price);
        
        if (qty == 0) {
            if (idx >= 0) remove_bid_level(idx);
            return;
        }
        
        if (idx >= 0) {
            bids[idx].qty = qty;
        } else {
            insert_bid_level(price, qty);
        }
    }
    
    FORCE_INLINE void update_ask(Price price, Quantity qty) noexcept {
        PREFETCH_WRITE(&asks[0]);
        
        if (ask_count > 0 && asks[0].price == price) {
            asks[0].qty = qty;
            if (qty == 0) {
                remove_ask_level(0);
            }
            return;
        }
        
        int idx = find_ask_level(price);
        
        if (qty == 0) {
            if (idx >= 0) remove_ask_level(idx);
            return;
        }
        
        if (idx >= 0) {
            asks[idx].qty = qty;
        } else {
            insert_ask_level(price, qty);
        }
    }
    
    // Accessors - always inline for zero overhead
    FORCE_INLINE Price best_bid() const noexcept {
        return bid_count > 0 ? bids[0].price : 0.0;
    }
    
    FORCE_INLINE Price best_ask() const noexcept {
        return ask_count > 0 ? asks[0].price : 0.0;
    }
    
    FORCE_INLINE Quantity best_bid_qty() const noexcept {
        return bid_count > 0 ? bids[0].qty : 0;
    }
    
    FORCE_INLINE Quantity best_ask_qty() const noexcept {
        return ask_count > 0 ? asks[0].qty : 0;
    }
    
    FORCE_INLINE Price mid_price() const noexcept {
        if (bid_count == 0 || ask_count == 0) return 0.0;
        return (bids[0].price + asks[0].price) * 0.5;
    }
    
    FORCE_INLINE Price spread() const noexcept {
        if (bid_count == 0 || ask_count == 0) return 0.0;
        return asks[0].price - bids[0].price;
    }
    
private:
    // Binary search in sorted bid levels (descending)
    FORCE_INLINE int find_bid_level(Price price) const noexcept {
        for (int i = 0; i < bid_count; ++i) {
            if (bids[i].price == price) return i;
            if (bids[i].price < price) return -1;
        }
        return -1;
    }
    
    FORCE_INLINE int find_ask_level(Price price) const noexcept {
        for (int i = 0; i < ask_count; ++i) {
            if (asks[i].price == price) return i;
            if (asks[i].price > price) return -1;
        }
        return -1;
    }
    
    FORCE_INLINE void insert_bid_level(Price price, Quantity qty) noexcept {
        if (bid_count >= MAX_PRICE_LEVELS) return;
        
        // Find insertion point
        int insert_idx = 0;
        while (insert_idx < bid_count && bids[insert_idx].price > price) {
            ++insert_idx;
        }
        
        // Shift down
        for (int i = bid_count; i > insert_idx; --i) {
            bids[i] = bids[i - 1];
        }
        
        bids[insert_idx].price = price;
        bids[insert_idx].qty = qty;
        ++bid_count;
    }
    
    FORCE_INLINE void insert_ask_level(Price price, Quantity qty) noexcept {
        if (ask_count >= MAX_PRICE_LEVELS) return;
        
        int insert_idx = 0;
        while (insert_idx < ask_count && asks[insert_idx].price < price) {
            ++insert_idx;
        }
        
        for (int i = ask_count; i > insert_idx; --i) {
            asks[i] = asks[i - 1];
        }
        
        asks[insert_idx].price = price;
        asks[insert_idx].qty = qty;
        ++ask_count;
    }
    
    FORCE_INLINE void remove_bid_level(int idx) noexcept {
        for (int i = idx; i < bid_count - 1; ++i) {
            bids[i] = bids[i + 1];
        }
        bids[bid_count - 1].clear();
        --bid_count;
    }
    
    FORCE_INLINE void remove_ask_level(int idx) noexcept {
        for (int i = idx; i < ask_count - 1; ++i) {
            asks[i] = asks[i + 1];
        }
        asks[ask_count - 1].clear();
        --ask_count;
    }
};

static_assert(sizeof(OrderBook) <= 8192, "OrderBook must fit in 8KB");

} 