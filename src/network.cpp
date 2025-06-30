#include "network.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <unistd.h>
using namespace std;

Network::Network() = default;
Network::~Network() { closeSocket(); }

bool Network::createSocket() {
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    return sockfd >= 0;
}

bool Network::sendPacket(const sockaddr_in& addr, const SlowPacket& pkt, uint32_t& lastSeq, Session& sess) {
    const auto& d = pkt.data;

    auto tx = [&](const SlowPacket& p) {
        uint8_t buf[MAX_PACKET]; size_t len;
        p.serialize(buf, len);
        logPacket(p, "TX");
        if (!p.data.empty()) {
            size_t show = min<size_t>(50, p.data.size());
            string preview(p.data.begin(), p.data.begin() + show);
            cout << "✉  DATA (" << p.data.size() << " B): \"" << preview << (p.data.size() > show ? "…" : "") << "\"\n\n";
        }
        return sendto(sockfd, buf, len, 0, (sockaddr*)&addr, sizeof(addr)) == (ssize_t)len;
    };

    if (d.size() <= MAX_DATA) {
        if (sess.bytesInFlight + d.size() > sess.remoteWindow) {
            cerr << "[FLOW] janela cheia (" << sess.remoteWindow << " B); aguardando ACK\n";
            lastSeq = pkt.seqnum;
            return false;
        }
        if (tx(pkt)) {
            sess.bytesInFlight += d.size();
            lastSeq = pkt.seqnum;
        }
        return lastSeq == pkt.seqnum;
    }

    uint8_t fid = Session::generateUUID()[0];
    size_t off = 0;
    uint8_t fo = 0;
    uint32_t seq = pkt.seqnum;

    while (off < d.size()) {
        size_t freeWin = sess.remoteWindow - sess.bytesInFlight;
        if (!freeWin) break;

        size_t chunk = min<size_t>({MAX_DATA, d.size() - off, freeWin});

        SlowPacket f = pkt;
        f.fid = fid;
        f.fo = fo;
        f.seqnum = ++seq;
        f.data.assign(d.begin() + off, d.begin() + off + chunk);
        if (off + chunk < d.size()) f.flags |= MOREBITS;
        else f.flags &= ~MOREBITS;

        if (!tx(f)) return false;

        sess.bytesInFlight += chunk;
        off += chunk;
        ++fo;
    }

    cout << "[FLOW] " << int(fo) << " fragmentos enviados, aguardando ACK…\n";
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
        string preview(pkt.data.begin(), pkt.data.begin() + show);
        cout << "✉  DATA (" << pkt.data.size() << " B): \"" << preview << (pkt.data.size() > show ? "…" : "") << "\"\n\n";
    }
    return true;
}

void Network::closeSocket() {
    if (sockfd >= 0) close(sockfd);
    sockfd = -1;
}