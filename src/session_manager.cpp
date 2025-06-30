#include "session_manager.h"
#include "packet.h"
#include <iostream>
using namespace std;

bool doThreeWayHandshake(Network& net, sockaddr_in& srv, Session& s) {
    SlowPacket syn;
    syn.flags = CONNECT;
    syn.window = s.recvWindow;

    uint32_t last;
    if (!net.sendPacket(srv, syn, last, s)) return false;

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
    return true;
}

bool tryRevive(Network& net, sockaddr_in& srv, Session& s) {
    SlowPacket r;
    r.sid = s.sid;
    r.flags = REVIVE | ACK;
    r.seqnum = ++s.seqnum;
    r.acknum = s.acknum;
    r.data.assign({'r', 'e', 'v', 'i', 'v', 'e'});

    uint32_t last;
    net.sendPacket(srv, r, last, s);

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