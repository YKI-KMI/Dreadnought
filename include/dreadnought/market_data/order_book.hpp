#pragma once

#include "dreadnought/core/types.hpp"
#include "dreadnought/core/compiler.hpp"
#include <array>
#include <algorithm>
#include <cstring>

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
        return bid_count > 0 ? bids[0].price : 0;
    }
    
    FORCE_INLINE Price best_ask() const noexcept {
        return ask_count > 0 ? asks[0].price : 0;
    }
    
    FORCE_INLINE Quantity best_bid_qty() const noexcept {
        return bid_count > 0 ? bids[0].qty : 0;
    }
    
    FORCE_INLINE Quantity best_ask_qty() const noexcept {
        return ask_count > 0 ? asks[0].qty : 0;
    }
    
    FORCE_INLINE Price mid_price() const noexcept {
        if (unlikely(bid_count == 0 || ask_count == 0)) return 0;
        return (bids[0].price + asks[0].price) / 2;
    }
    
    FORCE_INLINE Price spread() const noexcept {
        if (unlikely(bid_count == 0 || ask_count == 0)) return 0;
        return asks[0].price - bids[0].price;
    }
    
private:
    // Binary search in sorted bid levels (descending)
    FORCE_INLINE int find_bid_level(Price price) const noexcept {
        int lo = 0, hi = static_cast<int>(bid_count) - 1;
        while (lo <= hi) {
            int mid = (lo + hi) >> 1;
            if (bids[mid].price == price) return mid;
            if (bids[mid].price > price) lo = mid + 1;
            else hi = mid - 1;
        }
        return -1;
    }
    
    FORCE_INLINE int find_ask_level(Price price) const noexcept {
        int lo = 0, hi = static_cast<int>(ask_count) - 1;
        while (lo <= hi) {
            int mid = (lo + hi) >> 1;
            if (asks[mid].price == price) return mid;
            if (asks[mid].price < price) lo = mid + 1;
            else hi = mid - 1;
        }
        return -1;
    }
    
    FORCE_INLINE void insert_bid_level(Price price, Quantity qty) noexcept {
        if (unlikely(bid_count >= MAX_PRICE_LEVELS)) return;
        
        // Find insertion point - bids are descending
        int lo = 0, hi = static_cast<int>(bid_count);
        while (lo < hi) {
            int mid = (lo + hi) >> 1;
            if (bids[mid].price > price) lo = mid + 1;
            else hi = mid;
        }
        int insert_idx = lo;
        
        // Shift down using memmove
        if (insert_idx < bid_count) {
            std::memmove(&bids[insert_idx + 1], &bids[insert_idx], 
                         (bid_count - insert_idx) * sizeof(PriceLevel));
        }
        
        bids[insert_idx].price = price;
        bids[insert_idx].qty = qty;
        ++bid_count;
    }
    
    FORCE_INLINE void insert_ask_level(Price price, Quantity qty) noexcept {
        if (unlikely(ask_count >= MAX_PRICE_LEVELS)) return;
        
        // Find insertion point - asks are ascending
        int lo = 0, hi = static_cast<int>(ask_count);
        while (lo < hi) {
            int mid = (lo + hi) >> 1;
            if (asks[mid].price < price) lo = mid + 1;
            else hi = mid;
        }
        int insert_idx = lo;
        
        if (insert_idx < ask_count) {
            std::memmove(&asks[insert_idx + 1], &asks[insert_idx], 
                         (ask_count - insert_idx) * sizeof(PriceLevel));
        }
        
        asks[insert_idx].price = price;
        asks[insert_idx].qty = qty;
        ++ask_count;
    }
    
    FORCE_INLINE void remove_bid_level(int idx) noexcept {
        --bid_count;
        if (idx < bid_count) {
            std::memmove(&bids[idx], &bids[idx + 1], 
                         (bid_count - idx) * sizeof(PriceLevel));
        }
        bids[bid_count].clear();
    }
    
    FORCE_INLINE void remove_ask_level(int idx) noexcept {
        --ask_count;
        if (idx < ask_count) {
            std::memmove(&asks[idx], &asks[idx + 1], 
                         (ask_count - idx) * sizeof(PriceLevel));
        }
        asks[ask_count].clear();
    }
};

static_assert(sizeof(OrderBook) <= 8192, "OrderBook must fit in 8KB");

} 