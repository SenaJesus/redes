#include "network.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <sys/select.h>
using namespace std;

void Network::pushPending(const uint8_t* buf, size_t len, uint32_t seq, size_t dsz) {
    pend.emplace_back(buf, len, seq, dsz);
}

void Network::dropAcked(uint32_t ack, Session& sess) {
    while (!pend.empty() && pend.front().seq <= ack) {
        sess.bytesInFlight -= pend.front().dataSz;
        pend.pop_front();
    }
}

bool Network::retransmit(const sockaddr_in& addr, Session& sess) {
    if (pend.empty()) return false;
    Pending& p = pend.front();
    if (nowMs() - p.sentAt < RETRY_MS) return false;
    if (p.tries >= MAX_TRIES) {
        cerr << "[timeout] seq " << p.seq << " excedeu MAX_TRIES, descartando\n";
        sess.bytesInFlight -= p.dataSz;
        pend.pop_front();
        return false;
    }
    ++p.tries;
    p.sentAt = nowMs();
    ::sendto(sockfd, p.buf.data(), p.buf.size(), 0, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    cout << "↻ RETX seq=" << p.seq << " (try " << p.tries << '/' << MAX_TRIES << ")\n";
    return true;
}

Network::Network() = default;
Network::~Network() { closeSocket(); }

bool Network::createSocket() {
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    return sockfd >= 0;
}

bool Network::sendPacket(const sockaddr_in& addr, const SlowPacket& pkt, uint32_t& lastSeq, Session& sess) {
    uint8_t buf[MAX_PACKET]; size_t len;
    pkt.serialize(buf, len);
    logPacket(pkt, "TX");
    if (!pkt.data.empty()) {
        size_t show = min<size_t>(50, pkt.data.size());
        string prev(pkt.data.begin(), pkt.data.begin() + show);
        cout << "✉  DATA (" << pkt.data.size() << " B): \"" << prev << (pkt.data.size() > show ? "…" : "") << "\"\n\n";
    }
    if (sess.bytesInFlight + pkt.data.size() > sess.remoteWindow) {
        cerr << "[FLOW] janela cheia, aguardando ACK\n";
        lastSeq = pkt.seqnum;
        return false;
    }
    ssize_t sent = ::sendto(sockfd, buf, len, 0, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    if (sent != static_cast<ssize_t>(len)) return false;
    sess.bytesInFlight += pkt.data.size();
    lastSeq = pkt.seqnum;
    pushPending(buf, len, pkt.seqnum, pkt.data.size());
    return true;
}

bool Network::receivePacket(SlowPacket& pkt, sockaddr_in& from, Session& sess) {
    uint8_t buf[MAX_PACKET]; socklen_t alen = sizeof(from);
    fd_set rf; FD_ZERO(&rf); FD_SET(sockfd, &rf);
    timeval tv{0, 500000};
    int ready = select(sockfd + 1, &rf, nullptr, nullptr, &tv);
    if (ready == 0) {
        retransmit(from, sess);
        return false;
    }
    ssize_t n = recvfrom(sockfd, buf, MAX_PACKET, 0, reinterpret_cast<sockaddr*>(&from), &alen);
    if (n < HDR_SIZE) return false;
    pkt.deserialize(buf, n);
    logPacket(pkt, "RX");
    if (!pkt.data.empty()) {
        size_t show = min<size_t>(50, pkt.data.size());
        string prev(pkt.data.begin(), pkt.data.begin() + show);
        cout << "✉  DATA (" << pkt.data.size() << " B): \"" << prev << (pkt.data.size() > show ? "…" : "") << "\"\n\n";
    }
    sess.sttl = pkt.sttl;
    if (pkt.flags & ACK) {
        dropAcked(pkt.acknum, sess);
        sess.remoteWindow = pkt.window;
    }
    return true;
}

void Network::closeSocket() {
    if (sockfd >= 0) close(sockfd);
    sockfd = -1;
}