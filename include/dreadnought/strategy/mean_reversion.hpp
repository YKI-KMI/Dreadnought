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
        Price last_mid;
        double last_vamp;
        int32_t position;
        Signal current_signal;
        bool initialized;
        uint8_t _pad[3];
    } state;
    
    [[no_unique_address]] RiskModel risk_model;
    
    MeanReversionStrategy() noexcept {
        state.ema = 0.0;
        state.variance = 0.0;
        state.last_mid = 0;
        state.last_vamp = 0.0;
        state.position = 0;
        state.initialized = false;
    }
    
    FORCE_INLINE void on_tick_impl(const OrderBook& book, Timestamp ts) noexcept {
        Price b0 = book.best_bid();
        Price a0 = book.best_ask();
        Quantity bq0 = book.best_bid_qty();
        Quantity aq0 = book.best_ask_qty();
        
        if (unlikely(b0 <= 0 || a0 <= 0 || bq0 + aq0 == 0)) return;
        
        // Micro-price (VAMP) - better estimator than simple mid
        const double vamp = static_cast<double>(b0 * aq0 + a0 * bq0) / (bq0 + aq0);
        const Price mid = (b0 + a0) / 2;
        
        if (unlikely(!state.initialized)) {
            state.ema = vamp;
            state.variance = 0.0;
            state.last_mid = mid;
            state.last_vamp = vamp;
            state.initialized = true;
            return;
        }
        
        // Optimization: Skip if book hasn't changed significantly
        if (mid == state.last_mid && std::abs(vamp - state.last_vamp) < 0.1) return;
        
        // Update rolling stats (EMA) using micro-price
        const double diff = vamp - state.ema;
        state.ema += ALPHA * diff;
        // Correct EMA variance: Var_t = (1-ALPHA) * (Var_{t-1} + ALPHA * diff^2)
        state.variance = (1.0 - ALPHA) * (state.variance + ALPHA * diff * diff);
        
        state.last_mid = mid;
        state.last_vamp = vamp;
        
        const double squared_diff = diff * diff;
        const double threshold_variance = state.variance + 1e-9;
        
        generate_signal(book, squared_diff, diff, threshold_variance, (double)(bq0 - aq0)/(bq0 + aq0));
    }
    
    FORCE_INLINE void on_trade_impl(Price price, Quantity qty, Side side, Timestamp ts) noexcept {
        int qty_signed = static_cast<int>(qty);
        state.position += (side == Side::BID) ? qty_signed : -qty_signed;
    }
    
    FORCE_INLINE bool should_send_order_impl() const noexcept {
        if (!state.current_signal.valid) return false;
        return risk_model.check_position_limit(state.position, state.current_signal.qty, state.current_signal.side);
    }
    
    FORCE_INLINE Side get_order_side_impl() const noexcept { return state.current_signal.side; }
    FORCE_INLINE Price get_order_price_impl() const noexcept { return state.current_signal.price; }
    FORCE_INLINE Quantity get_order_qty_impl() const noexcept { return state.current_signal.qty; }
    
private:
    FORCE_INLINE void generate_signal(const OrderBook& book, double squared_diff, double diff, double var, double imbalance) noexcept {
        state.current_signal.valid = false;
        
        // Threshold check + Book Imbalance filter
        if (squared_diff > ENTRY_THRESHOLD_SQ * var) {
            if (diff > 0 && state.position >= 0 && imbalance < -0.3) { // Price high + Ask pressure -> Sell
                state.current_signal.side = Side::ASK;
                state.current_signal.price = book.best_bid();
                state.current_signal.qty = compute_order_size();
                state.current_signal.valid = true;
            } else if (diff < 0 && state.position <= 0 && imbalance > 0.3) { // Price low + Bid pressure -> Buy
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