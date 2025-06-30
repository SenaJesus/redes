#include "session_manager.h"
#include "packet.h"
#include <iostream>
using namespace std;

bool doThreeWayHandshake(Network& net, sockaddr_in& srv, Session& s) {
    SlowPacket syn;
    syn.flags = CONNECT;
    syn.window = s.recvWindow;

    uint32_t dummy;
    net.sendPacket(srv, syn, dummy, s);

    SlowPacket setup;
    sockaddr_in from{};
    if (!net.receivePacket(setup, from, s) || !(setup.flags & ACCEPT)) {
        cerr << "[HANDSHAKE] FAIL – SETUP inválido\n";
        return false;
    }

    s.sid = setup.sid;
    s.seqnum = setup.seqnum + 1;
    s.acknum = setup.seqnum;
    s.remoteWindow = setup.window;
    s.bytesInFlight = 0;
    s.connected = true;
    s.sttl = setup.sttl;

    cout << "[HANDSHAKE] concluído! janela=" << s.remoteWindow << " B\n";

    SlowPacket ack;
    ack.sid = s.sid;
    ack.flags = ACK;
    ack.seqnum = s.seqnum;
    ack.acknum = s.acknum;
    ack.window = s.recvWindow;
    ack.sttl = s.sttl;

    net.sendPacket(srv, ack, dummy, s);
    return true;
}

bool tryRevive(Network& net, sockaddr_in& srv, Session& s) {
    SlowPacket r;
    r.sid = s.sid;
    r.flags = REVIVE | ACK;
    r.seqnum = ++s.seqnum;
    r.acknum = s.acknum;
    r.data.assign({'r','e','v','i','v','e'});

    uint32_t dummy;
    net.sendPacket(srv, r, dummy, s);

    SlowPacket resp;
    sockaddr_in from{};
    if (!net.receivePacket(resp, from, s)) return false;

    if (resp.flags & (ACK | ACCEPT)) {
        s.acknum = resp.seqnum;
        s.remoteWindow = resp.window;
        s.bytesInFlight = 0;
        s.connected = true;
        s.sttl = resp.sttl;
        return true;
    }
    return false;
}
