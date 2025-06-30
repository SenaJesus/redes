#include "session.h"
#include <random>

Session::Session() noexcept { sid = nilUUID(); }

std::array<uint8_t, 16> Session::nilUUID() { return {}; }

std::array<uint8_t, 16> Session::generateUUID() {
    std::array<uint8_t, 16> u;
    std::random_device rd;
    for (auto& b : u) b = static_cast<uint8_t>(rd());
    u[6] = (u[6] & 0x0F) | 0x80;
    u[8] = (u[8] & 0x3F) | 0x80;
    return u;
}