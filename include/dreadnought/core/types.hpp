#pragma once

#include <cstdint>
#include <array>
#include "compiler.hpp"

namespace dreadnought {

using Price = double;
using Quantity = uint32_t;
using OrderID = uint64_t;
using Timestamp = uint64_t;
using SymbolID = uint32_t;

constexpr int MAX_PRICE_LEVELS = 64;
constexpr int MAX_SYMBOL_COUNT = 16;

enum class Side : uint8_t {
    BID = 0,
    ASK = 1
};

enum class OrderType : uint8_t {
    LIMIT = 0,
    MARKET = 1,
    IOC = 2
};

struct alignas(16) PriceLevel {
    Price price;
    Quantity qty;
    
    FORCE_INLINE void clear() noexcept {
        price = 0.0;
        qty = 0;
    }
    
    FORCE_INLINE bool is_empty() const noexcept {
        return qty == 0;
    }
};

static_assert(sizeof(PriceLevel) == 16, "PriceLevel must be 16 bytes");

} 