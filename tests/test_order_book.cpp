#include "dreadnought/market_data/order_book.hpp"
#include <iostream>
#include <cassert>

using namespace dreadnought;

void test_basic_updates() {
    OrderBook book;
    
    book.update_bid(100 * PRICE_SCALER, 500);
    book.update_ask(101 * PRICE_SCALER, 300);
    
    assert(book.best_bid() == 100 * PRICE_SCALER);
    assert(book.best_ask() == 101 * PRICE_SCALER);
    assert(book.best_bid_qty() == 500);
    assert(book.best_ask_qty() == 300);
    assert(book.spread() == 1 * PRICE_SCALER);
    assert(book.mid_price() == 1005000); // (1000000 + 1010000) / 2
    
    std::cout << "✓ Basic updates passed\n";
}

void test_level_insertion() {
    OrderBook book;
    
    book.update_bid(100 * PRICE_SCALER, 100);
    book.update_bid(99 * PRICE_SCALER, 200);
    book.update_bid(101 * PRICE_SCALER, 50);
    
    assert(book.best_bid() == 101 * PRICE_SCALER);
    assert(book.best_bid_qty() == 50);
    
    std::cout << "✓ Level insertion passed\n";
}

void test_level_removal() {
    OrderBook book;
    
    book.update_bid(100 * PRICE_SCALER, 100);
    book.update_bid(99 * PRICE_SCALER, 200);
    
    book.update_bid(100 * PRICE_SCALER, 0); // Remove top level
    
    assert(book.best_bid() == 99 * PRICE_SCALER);
    assert(book.best_bid_qty() == 200);
    
    std::cout << "✓ Level removal passed\n";
}

int main() {
    std::cout << "=== Order Book Tests ===\n";
    
    test_basic_updates();
    test_level_insertion();
    test_level_removal();
    
    std::cout << "All tests passed!\n";
    return 0;
}