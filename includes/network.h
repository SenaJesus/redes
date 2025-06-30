#ifndef NETWORK_H
#define NETWORK_H

#include "packet.h"
#include "session.h"
#include <array>
#include <deque>
#include <cstring>
#include <chrono>
#include <netinet/in.h>

static inline uint64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

class Network {
public:
    Network(); ~Network();
    bool createSocket();
    bool sendPacket(const sockaddr_in& addr, const SlowPacket& pkt, uint32_t& lastSeq, Session& sess);
    bool receivePacket(SlowPacket& pkt, sockaddr_in& from, Session& sess);
    void closeSocket();

private:
    struct Pending {
        std::array<uint8_t, MAX_PACKET> buf;
        size_t len;
        uint32_t seq;
        size_t dataSz;
        uint64_t sentAt;
        int tries;
        Pending(const uint8_t* b, size_t l, uint32_t s, size_t d)
        : len(l), seq(s), dataSz(d), sentAt(nowMs()), tries(0) {
            std::memcpy(buf.data(), b, l);
        }
    };

    static constexpr uint64_t RETRY_MS = 500;
    static constexpr int MAX_TRIES = 5;

    std::deque<Pending> pend;
    void pushPending(const uint8_t* buf, size_t len, uint32_t seq, size_t dsz);
    void dropAcked(uint32_t ack, Session& sess);
    bool retransmit(const sockaddr_in& addr, Session& sess);
    int sockfd = -1;
};

#endif