#ifndef SESSION_H
#define SESSION_H

#include "slow.h"
#include <array>
#include <cstdint>

struct Session {
    std::array<uint8_t, 16> sid{};
    uint32_t seqnum = 0;
    uint32_t acknum = 0;
    uint32_t sttl = 0;
    uint16_t recvWindow = 7200;
    bool connected = false;

    uint32_t remoteWindow = 1024;
    uint32_t bytesInFlight = 0;

    Session() noexcept;
    static std::array<uint8_t, 16> generateUUID();
    static std::array<uint8_t, 16> nilUUID();
};

#endif