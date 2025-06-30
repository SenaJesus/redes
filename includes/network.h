#ifndef NETWORK_H
#define NETWORK_H

#include "packet.h"
#include "session.h"
#include <netinet/in.h>
#include <deque>
#include <chrono>

class Network {
public:
    Network();
    ~Network();

    bool createSocket();

    bool sendPacket(const sockaddr_in& addr,
                    const SlowPacket& pkt,
                    uint32_t& lastSeq,
                    Session& sess);

    bool receivePacket(SlowPacket& pkt, sockaddr_in& from);

    void tickRetransmit(const sockaddr_in& addr, Session& sess);
    void closeSocket();

    struct Pending {
        std::vector<uint8_t> buf;
        size_t len = 0;
        uint32_t seq = 0;
        size_t data = 0;
        int tries = 0;
        std::chrono::steady_clock::time_point t0;
    };

private:
    int sockfd = -1;
    std::deque<Pending> pend;

    static constexpr int MAX_TRIES = 3;
    static constexpr std::chrono::seconds RETRY{2};
};

#endif