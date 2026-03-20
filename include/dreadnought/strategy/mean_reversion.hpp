#pragma once

#include "dreadnought/strategy/strategy_base.hpp"
#include "dreadnought/risk/risk_models.hpp"
#include "dreadnought/core/compiler.hpp"
#include <array>
#include <cmath>
#include <cstdint>

namespace dreadnought {

// Mean reversion strategy with template-based risk model injection
template<typename RiskModel>
struct MeanReversionStrategy : StrategyBase<MeanReversionStrategy<RiskModel>> {
    // Strategy parameters
    static constexpr double ALPHA = 0.1;           // EMA smoothing factor
    static constexpr double ENTRY_THRESHOLD_SQ = 4.0; // 2.0^2
    static constexpr double EXIT_THRESHOLD_SQ = 0.25; // 0.5^2
    static constexpr Quantity MAX_POSITION = 1000;
    
    // State
    alignas(DESTRUCTIVE_SIZE) struct State {
        double ema;
        double variance;
        int32_t position;
        Signal current_signal;
        bool initialized;
        uint8_t _pad[3];
    } state;
    
    [[no_unique_address]] RiskModel risk_model;
    
    MeanReversionStrategy() noexcept {
        state.ema = 0.0;
        state.variance = 0.0;
        state.position = 0;
        state.initialized = false;
    }
    
    FORCE_INLINE void on_tick_impl(const OrderBook& book, Timestamp ts) noexcept {
        Price mid = book.mid_price();
        if (unlikely(mid <= 0.0)) return;
        
        if (unlikely(!state.initialized)) {
            state.ema = mid;
            state.variance = 0.0;
            state.initialized = true;
            return;
        }
        
        // Update rolling stats (EMA) - O(1)
        const double diff = mid - state.ema;
        state.ema += ALPHA * diff;
        state.variance = (1.0 - ALPHA) * (state.variance + ALPHA * diff * diff);
        
        // Z-score logic using squared values to avoid sqrt (Newton-Raphson) and division
        // Formula: (mid - mean)^2 > threshold^2 * variance
        const double squared_diff = diff * diff;
        const double threshold_variance = state.variance + 1e-9; // Avoid zero
        
        generate_signal(book, squared_diff, diff, threshold_variance);
    }
    
    FORCE_INLINE void on_trade_impl(Price price, Quantity qty, Side side, Timestamp ts) noexcept {
        int qty_signed = static_cast<int>(qty);
        state.position += (side == Side::BID) ? qty_signed : -qty_signed;
    }
    
    FORCE_INLINE bool should_send_order_impl() const noexcept {
        if (!state.current_signal.valid) return false;
        return risk_model.check_position_limit(state.position, state.current_signal.qty);
    }
    
    FORCE_INLINE Side get_order_side_impl() const noexcept { return state.current_signal.side; }
    FORCE_INLINE Price get_order_price_impl() const noexcept { return state.current_signal.price; }
    FORCE_INLINE Quantity get_order_qty_impl() const noexcept { return state.current_signal.qty; }
    
private:
    FORCE_INLINE void generate_signal(const OrderBook& book, double squared_diff, double diff, double var) noexcept {
        state.current_signal.valid = false;
        
        if (squared_diff > ENTRY_THRESHOLD_SQ * var) {
            if (diff > 0 && state.position >= 0) { // Price too high -> Sell
                state.current_signal.side = Side::ASK;
                state.current_signal.price = book.best_bid();
                state.current_signal.qty = compute_order_size();
                state.current_signal.valid = true;
            } else if (diff < 0 && state.position <= 0) { // Price too low -> Buy
                state.current_signal.side = Side::BID;
                state.current_signal.price = book.best_ask();
                state.current_signal.qty = compute_order_size();
                state.current_signal.valid = true;
            }
        } else if (state.position != 0 && squared_diff < EXIT_THRESHOLD_SQ * var) {
            // Exit position - price returned to mean
            Side exit_side = (state.position > 0) ? Side::ASK : Side::BID;
            state.current_signal.side = exit_side;
            state.current_signal.price = (exit_side == Side::BID) ? book.best_ask() : book.best_bid();
            state.current_signal.qty = static_cast<Quantity>(__builtin_abs(state.position));
            state.current_signal.valid = true;
        }
    }
    
    FORCE_INLINE Quantity compute_order_size() const noexcept {
        Quantity base_size = 100;
        if (__builtin_abs(state.position) > MAX_POSITION / 2) base_size = 50;
        return base_size;
    }
};

} // namespace dreadnought