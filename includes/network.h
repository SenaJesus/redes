#ifndef NETWORK_H
#define NETWORK_H

#include "packet.h"
#include "session.h"
#include <netinet/in.h>

class Network {
public:
    Network(); ~Network();
    bool createSocket();
    bool sendPacket(const sockaddr_in& addr, const SlowPacket& pkt, uint32_t& lastSeq, Session& sess);
    bool receivePacket(SlowPacket& pkt, sockaddr_in& from);
    void closeSocket();
private:
    int sockfd = -1;
};

#endif