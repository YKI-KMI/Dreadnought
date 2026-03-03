#pragma once

#include "dreadnought/strategy/strategy_base.hpp"
#include "dreadnought/risk/risk_models.hpp"
#include "dreadnought/core/compiler.hpp"
#include <array>

namespace dreadnought {

// Mean reversion strategy with template-based risk model injection
template<typename RiskModel>
struct MeanReversionStrategy : StrategyBase<MeanReversionStrategy<RiskModel>> {
    // Strategy parameters - compile-time constants where possible
    static constexpr int LOOKBACK_WINDOW = 20;
    static constexpr double ENTRY_THRESHOLD = 2.0;  // Standard deviations
    static constexpr double EXIT_THRESHOLD = 0.5;
    static constexpr Quantity MAX_POSITION = 1000;
    
    // State
    alignas(DESTRUCTIVE_SIZE) struct State {
        std::array<Price, LOOKBACK_WINDOW> mid_prices;
        int window_idx;
        Price mean;
        Price std_dev;
        int32_t position;
        Signal current_signal;
        uint8_t _pad[7];
    } state;
    
    [[no_unique_address]] RiskModel risk_model;
    
    MeanReversionStrategy() noexcept {
        state.window_idx = 0;
        state.mean = 0.0;
        state.std_dev = 0.0;
        state.position = 0;
        state.mid_prices.fill(0.0);
    }
    
    FORCE_INLINE void on_tick_impl(const OrderBook& book, Timestamp ts) noexcept {
        Price mid = book.mid_price();
        if (mid <= 0.0) return;
        
        // Update rolling window
        state.mid_prices[state.window_idx] = mid;
        state.window_idx = (state.window_idx + 1) % LOOKBACK_WINDOW;
        
        // Compute statistics - unrolled for determinism
        Price sum = 0.0;
        for (int i = 0; i < LOOKBACK_WINDOW; ++i) {
            sum += state.mid_prices[i];
        }
        state.mean = sum / LOOKBACK_WINDOW;
        
        Price var_sum = 0.0;
        for (int i = 0; i < LOOKBACK_WINDOW; ++i) {
            Price diff = state.mid_prices[i] - state.mean;
            var_sum += diff * diff;
        }
        state.std_dev = __builtin_sqrt(var_sum / LOOKBACK_WINDOW);
        
        // Z-score calculation
        Price z_score = (mid - state.mean) / (state.std_dev + 1e-8);
        
        // Signal generation - branch-free where possible
        generate_signal(book, z_score, mid);
    }
    
    FORCE_INLINE void on_trade_impl(Price price, Quantity qty, Side side, Timestamp ts) noexcept {
        // Update position based on our fills
        int qty_signed = static_cast<int>(qty);
        state.position += (side == Side::BID) ? qty_signed : -qty_signed;
    }
    
    FORCE_INLINE bool should_send_order_impl() const noexcept {
        if (!state.current_signal.valid) return false;
        if (!risk_model.check_position_limit(state.position, state.current_signal.qty)) return false;
        return true;
    }
    
    FORCE_INLINE Side get_order_side_impl() const noexcept {
        return state.current_signal.side;
    }
    
    FORCE_INLINE Price get_order_price_impl() const noexcept {
        return state.current_signal.price;
    }
    
    FORCE_INLINE Quantity get_order_qty_impl() const noexcept {
        return state.current_signal.qty;
    }
    
private:
    FORCE_INLINE void generate_signal(const OrderBook& book, Price z_score, Price mid) noexcept {
        state.current_signal.valid = false;
        
        // Mean reversion logic
        // Sell when price is too high (z > ENTRY_THRESHOLD)
        // Buy when price is too low (z < -ENTRY_THRESHOLD)
        
        if (z_score > ENTRY_THRESHOLD && state.position >= 0) {
            // Sell signal - price too high
            state.current_signal.side = Side::ASK;
            state.current_signal.price = book.best_bid(); // Join bid to get filled
            state.current_signal.qty = compute_order_size();
            state.current_signal.confidence = 80;
            state.current_signal.valid = true;
        } else if (z_score < -ENTRY_THRESHOLD && state.position <= 0) {
            // Buy signal - price too low
            state.current_signal.side = Side::BID;
            state.current_signal.price = book.best_ask(); // Join ask to get filled
            state.current_signal.qty = compute_order_size();
            state.current_signal.confidence = 80;
            state.current_signal.valid = true;
        } else if (__builtin_fabs(z_score) < EXIT_THRESHOLD && state.position != 0) {
            // Exit position - price returned to mean
            Side exit_side = (state.position > 0) ? Side::ASK : Side::BID;
            state.current_signal.side = exit_side;
            state.current_signal.price = (exit_side == Side::BID) ? book.best_ask() : book.best_bid();
            state.current_signal.qty = __builtin_abs(state.position);
            state.current_signal.confidence = 90;
            state.current_signal.valid = true;
        }
    }
    
    FORCE_INLINE Quantity compute_order_size() const noexcept {
        // Simple fixed size for now
        Quantity base_size = 100;
        
        // Reduce size as position grows
        int abs_pos = __builtin_abs(state.position);
        if (abs_pos > MAX_POSITION / 2) {
            base_size = 50;
        }
        
        return base_size;
    }
};

} // namespace dreadnought