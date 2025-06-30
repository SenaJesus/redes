#include "network.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <unistd.h>
using namespace std;
using clk = std::chrono::steady_clock;

Network::Network() = default;
Network::~Network() { closeSocket(); }

bool Network::createSocket() {
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    return sockfd >= 0;
}

static void pushPending(deque<Network::Pending>& q, const uint8_t* buf, size_t len, uint32_t seq, size_t dsz) {
    q.push_back({std::vector<uint8_t>(buf, buf + len), len, seq, dsz, 0, clk::now()});
}

bool Network::sendPacket(const sockaddr_in& addr, const SlowPacket& pkt, uint32_t& lastSeq, Session& sess) {
    tickRetransmit(addr, sess);

    auto tx = [&](const SlowPacket& p, size_t dsz) {
        uint8_t buf[MAX_PACKET]; size_t len;
        p.serialize(buf, len);
        logPacket(p, "TX");

        if (!p.data.empty()) {
            size_t show = min<size_t>(50, p.data.size());
            cout << "✉  DATA (" << p.data.size() << " B): \""
                 << string(p.data.begin(), p.data.begin() + show)
                 << (p.data.size() > show ? "…" : "") << "\"\n\n";
        }

        if (sendto(sockfd, buf, len, 0, (sockaddr*)&addr, sizeof(addr)) != (ssize_t)len)
            return false;

        if (dsz) pushPending(pend, buf, len, p.seqnum, dsz);
        return true;
    };

    if (pkt.data.size() <= MAX_DATA) {
        if (sess.bytesInFlight + pkt.data.size() > sess.remoteWindow) {
            cerr << "[FLOW] janela cheia (" << sess.remoteWindow << " B)\n";
            lastSeq = pkt.seqnum;
            return false;
        }
        if (tx(pkt, pkt.data.size())) {
            sess.bytesInFlight += pkt.data.size();
            lastSeq = pkt.seqnum;
        }
        return lastSeq == pkt.seqnum;
    }

    uint8_t fid = Session::generateUUID()[0];
    uint8_t fo = 0;
    uint32_t seq = pkt.seqnum;
    size_t off = 0;

    while (off < pkt.data.size()) {
        size_t freeWin = sess.remoteWindow - sess.bytesInFlight;
        if (!freeWin) break;

        size_t chunk = min<size_t>({MAX_DATA, pkt.data.size() - off, freeWin});

        SlowPacket f = pkt;
        f.fid = fid;
        f.fo = fo;
        f.seqnum = ++seq;
        f.data.assign(pkt.data.begin() + off, pkt.data.begin() + off + chunk);
        if (off + chunk < pkt.data.size()) f.flags |= MOREBITS;
        else f.flags &= ~MOREBITS;

        if (!tx(f, chunk)) return false;

        sess.bytesInFlight += chunk;
        off += chunk;
        ++fo;
    }

    cout << "[FLOW] " << int(fo) << " fragmentos enviados\n";
    lastSeq = seq;
    return true;
}

bool Network::receivePacket(SlowPacket& pkt, sockaddr_in& from) {
    uint8_t buf[MAX_PACKET]; socklen_t alen = sizeof(from);
    ssize_t n = recvfrom(sockfd, buf, MAX_PACKET, 0, (sockaddr*)&from, &alen);
    if (n < HDR_SIZE) return false;

    pkt.deserialize(buf, n);
    logPacket(pkt, "RX");

    if (pkt.data.size()) {
        size_t show = min<size_t>(50, pkt.data.size());
        cout << "✉  DATA (" << pkt.data.size() << " B): \""
             << string(pkt.data.begin(), pkt.data.begin() + show)
             << (pkt.data.size() > show ? "…" : "") << "\"\n\n";
    }

    if (pkt.flags & ACK) {
        while (!pend.empty() && pend.front().seq <= pkt.acknum) {
            cout << "✓ ACK " << pend.front().seq << " (" << pend.front().data << " B)\n";
            pend.pop_front();
        }
    }

    return true;
}

void Network::tickRetransmit(const sockaddr_in& addr, Session& sess) {
    auto now = clk::now();

    for (auto& p : pend) {
        if (p.tries >= MAX_TRIES) continue;
        if (now - p.t0 < RETRY) continue;

        if (sendto(sockfd, p.buf.data(), p.len, 0, (sockaddr*)&addr, sizeof(addr)) < 0)
            continue;

        ++p.tries;
        p.t0 = now;
        cout << "↻ RETX seq=" << p.seq << " (try " << p.tries << '/' << MAX_TRIES << ")\n";
    }

    while (!pend.empty() && pend.front().tries >= MAX_TRIES) {
        cerr << "[WARN] abortando seq " << pend.front().seq << " (3x falhou)\n";
        sess.bytesInFlight -= pend.front().data;
        pend.pop_front();
    }
}

void Network::closeSocket() {
    if (sockfd >= 0) close(sockfd);
    sockfd = -1;
}