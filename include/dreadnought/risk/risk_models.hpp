#pragma once

#include "dreadnought/core/types.hpp"
#include "dreadnought/core/compiler.hpp"

namespace dreadnought {

// Stateless risk model - zero memory overhead via [[no_unique_address]]
struct StaticRiskModel {
    static constexpr int32_t MAX_POSITION = 1000;
    static constexpr int32_t MAX_ORDER_SIZE = 500;
    
    FORCE_INLINE bool check_position_limit(int32_t current_pos, Quantity order_qty) const noexcept {
        int32_t new_pos = __builtin_abs(current_pos) + static_cast<int32_t>(order_qty);
        return new_pos <= MAX_POSITION;
    }
    
    FORCE_INLINE bool check_order_size(Quantity qty) const noexcept {
        return qty <= MAX_ORDER_SIZE;
    }
};

// Dynamic risk model with state
struct DynamicRiskModel {
    alignas(DESTRUCTIVE_SIZE) struct State {
        int32_t max_position;
        int32_t max_order_size;
        double max_notional;
        uint8_t _pad[44];
    } state;
    
    DynamicRiskModel() noexcept {
        state.max_position = 1000;
        state.max_order_size = 500;
        state.max_notional = 1000000.0;
    }
    
    FORCE_INLINE bool check_position_limit(int32_t current_pos, Quantity order_qty) const noexcept {
        int32_t new_pos = __builtin_abs(current_pos) + static_cast<int32_t>(order_qty);
        return new_pos <= state.max_position;
    }
    
    FORCE_INLINE bool check_order_size(Quantity qty) const noexcept {
        return qty <= static_cast<Quantity>(state.max_order_size);
    }
};

} // namespace dreadnought