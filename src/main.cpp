#include "network.h"
#include "session_manager.h"
#include "packet.h"
#include "slow.h"

#include <arpa/inet.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

constexpr auto TL = u8"\u2554";
constexpr auto TR = u8"\u2557";
constexpr auto BL = u8"\u255A";
constexpr auto BR = u8"\u255D";
constexpr auto HL = u8"\u2550";
constexpr auto VL = u8"\u2551";

inline void hLine(const char* left, const char* right, int width, const string& title = "") {
    cout << left;
    if (!title.empty()) cout << ' ' << title << ' ';
    int fill = width - 2 - (title.empty() ? 0 : int(title.size()) + 2);
    while (fill-- > 0) cout << HL;
    cout << right << '\n';
}

inline void vLine(const string& body, int width) {
    cout << VL << ' ' << body;
    int pad = width - 2 - 1 - int(body.size());
    if (pad < 0) pad = 0;
    while (pad-- > 0) cout << ' ';
    cout << VL << '\n';
}

inline void printPktBox(const string& tag, const SlowPacket& p) {
    ios old(nullptr); old.copyfmt(cout);
    vector<string> rows; rows.reserve(4);
    stringstream ss;

    ss << "SID  : ";
    for (uint8_t b : p.sid) ss << hex << setw(2) << setfill('0') << int(b);
    rows.push_back(ss.str()); ss.str(""); ss.clear();

    ss.copyfmt(old);
    ss << "FLG  : " << setw(3) << left << flagsToString(p.flags) << "  STTL: " << p.sttl;
    rows.push_back(ss.str()); ss.str(""); ss.clear();

    ss << "SEQ  : " << p.seqnum << "   ACK : " << p.acknum;
    rows.push_back(ss.str()); ss.str(""); ss.clear();

    ss << "WIN  : " << p.window << "   FID/FO: " << int(p.fid) << '/' << int(p.fo);
    rows.push_back(ss.str());

    int width = 0;
    for (auto& r : rows) width = max(width, int(r.size()));
    width += 3;

    hLine(TL, TR, width, tag);
    for (auto& r : rows) vLine(r, width);
    hLine(BL, BR, width);
    cout << '\n';

    cout.copyfmt(old);
}

inline void banner() {
    cout << "\n================= S L O W   C L I E N T =================\n"
            "  d) data     x) disconnect     r) revive     ? ) status\n"
            "  h) help     q) quit\n"
            "=========================================================\n> ";
}

inline void help() {
    cout << "\nd) enviar mensagem   x) disconnect   r) revive\n"
            "?) status            h) ajuda        q) sair\n";
}

static void showStatus(const Session& s, bool conn, const char* h, int p) {
    ostringstream ss;
    ss << "Servidor : " << h << ':' << p << '\n'
       << "Conexão  : " << (conn ? "[CONECTADO]" : "[DESCONECTADO]") << '\n'
       << "Janela   : " << s.remoteWindow << " B\n"
       << "Em voo   : " << s.bytesInFlight << " B\n"
       << "SEQ/ACK  : " << s.seqnum << " / " << s.acknum;
    string txt = ss.str(); istringstream is(txt);
    string l; size_t w = 0; vector<string> rows;
    while (getline(is, l)) { rows.push_back(l); w = max(w, l.size()); }
    string bord(w + 4, '-');
    cout << "┌" << bord << "┐\n";
    for (auto& r : rows) cout << "│  " << left << setw(w) << r << "  │\n";
    cout << "└" << bord << "┘\n\n";
}

int main() {
    const char* HOST = "142.93.184.175";
    const int PORT = SLOW_PORT;

    Network net;
    if (!net.createSocket()) { cerr << "socket() erro\n"; return 1; }

    sockaddr_in srv{}; srv.sin_family = AF_INET;
    srv.sin_port = htons(PORT);
    inet_pton(AF_INET, HOST, &srv.sin_addr);

    Session sess; sess.recvWindow = 7200;
    bool connected = false;

    if (!doThreeWayHandshake(net, srv, sess)) return 1;
    connected = true; cout << "[sucesso] Conectado.\n";

    string cmd;
    while (true) {
        banner(); if (!(cin >> cmd)) break;

        if (cmd == "d") {
            if (!connected) { cout << "[erro] sem sessão (use r)\n"; continue; }
            cin.ignore();
            cout << "# Mensagem: "; string msg; getline(cin, msg);
            if (msg.empty()) continue;

            size_t off = 0; uint8_t fid = Session::generateUUID()[0]; uint8_t fo = 0;

            while (off < msg.size()) {
                size_t freeWin = sess.remoteWindow - sess.bytesInFlight;
                if (!freeWin) {
                    SlowPacket ack; sockaddr_in f{};
                    if (net.receivePacket(ack, f) && (ack.flags & ACK)) {
                        sess.remoteWindow = ack.window;
                        sess.bytesInFlight = 0;
                        sess.acknum = ack.seqnum;
                        cout << "[DEBUG] Janela atualizada: " << ack.window << "\n";
                    }
                    continue;
                }

                size_t chunk = min<size_t>({MAX_DATA, msg.size() - off, freeWin});

                SlowPacket p;
                p.sid = sess.sid;
                p.flags = ACK; if (off + chunk < msg.size()) p.flags |= MOREBITS;
                p.seqnum = ++sess.seqnum;
                p.acknum = sess.acknum;
                p.window = sess.recvWindow;
                p.sttl = sess.sttl;
                p.fid = fid;
                p.fo = fo++;
                p.data.assign(msg.begin() + off, msg.begin() + off + chunk);

                uint32_t lastSeq;
                net.sendPacket(srv, p, lastSeq, sess);
                sess.bytesInFlight += chunk;
                off += chunk;

                SlowPacket a; sockaddr_in from{};
                while (net.receivePacket(a, from)) {
                    if ((a.flags & ACK) && a.acknum == lastSeq) {
                        sess.remoteWindow = a.window;
                        sess.bytesInFlight = 0;
                        sess.acknum = a.seqnum;
                        cout << "[DEBUG] Janela atualizada: " << a.window << "\n";
                        break;
                    }
                }
            }
            cout << "[sucesso] Mensagem enviada (" << msg.size() << " B)\n";
        }

        else if (cmd == "x") {
            if (!connected) { cout << "[já desconectado]\n"; continue; }

            SlowPacket disc;
            disc.sid = sess.sid; disc.flags = CONNECT | REVIVE | ACK;
            disc.seqnum = ++sess.seqnum; disc.acknum = sess.acknum;

            uint32_t last; net.sendPacket(srv, disc, last, sess);

            SlowPacket resp; sockaddr_in from{};
            if (net.receivePacket(resp, from) && (resp.flags & ACK)) {
                connected = false; sess.bytesInFlight = 0;
                cout << "[sucesso] Desconectado.\n";
            } else cout << "[erro] sem ACK do disconnect.\n";
        }

        else if (cmd == "r") {
            if (connected) { cout << "[AVISO] Já conectado. Use x.\n"; continue; }
            if (!tryRevive(net, srv, sess)) cout << "[revive rejeitado]\n";
            else { connected = true; cout << "[revive OK]\n"; }
        }

        else if (cmd == "?") showStatus(sess, connected, HOST, PORT);
        else if (cmd == "h") help();
        else if (cmd == "q") { cout << "[tchau] sessão encerrada!\n"; break; }
        else cout << "[erro] comando inválido. 'h' ajuda.\n";
    }
    net.closeSocket();
    return 0;
}