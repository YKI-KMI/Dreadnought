#pragma once

#include "dreadnought/core/types.hpp"
#include "dreadnought/core/compiler.hpp"

namespace dreadnought {

enum class PacketType : uint8_t {
    MARKET_DATA_UPDATE = 1,
    TRADE = 2,
    ORDER_ACK = 3,
    ORDER_REJECT = 4
};

struct alignas(16) PacketHeader {
    uint64_t sequence_num;
    Timestamp exchange_ts;
    PacketType type;
    uint8_t symbol_id;
    uint16_t payload_len;
    uint32_t _pad;
};

static_assert(sizeof(PacketHeader) == 16, "PacketHeader must be 16 bytes");

// Market data update payload
struct alignas(16) MarketDataPayload {
    Side side;
    uint8_t _pad1;
    uint16_t level_idx;
    Price price;
    Quantity qty;
    uint32_t _pad2;
};

static_assert(sizeof(MarketDataPayload) == 32, "MarketDataPayload must be 32 bytes");


struct alignas(64) Packet {
    PacketHeader header;
    union {
        MarketDataPayload market_data;
        uint8_t raw[48];
    } payload;
    
    FORCE_INLINE bool is_market_data() const noexcept {
        return header.type == PacketType::MARKET_DATA_UPDATE;
    }
};

static_assert(sizeof(Packet) == 64, "Packet must be 64 bytes (one cache line)");

struct alignas(64) OrderPacket {
    PacketHeader header;
    Side side;
    OrderType order_type;
    uint16_t _pad1;
    Price price;
    Quantity qty;
    uint64_t client_order_id;
    uint8_t symbol_id;
    uint8_t _pad2[23];
};

static_assert(sizeof(OrderPacket) == 64, "OrderPacket must be 64 bytes");

} 