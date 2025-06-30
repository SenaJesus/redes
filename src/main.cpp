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
#include <limits>
using namespace std;

constexpr auto TL = u8"\u2554", TR = u8"\u2557";
constexpr auto BL = u8"\u255A", BR = u8"\u255D";
constexpr auto HL = u8"\u2550", VL = u8"\u2551";

inline void hLine(const char* l, const char* r, int w, const string& t = "") {
    cout << l; if (!t.empty()) cout << ' ' << t << ' ';
    int f = w - 2 - (t.empty() ? 0 : int(t.size()) + 2); while (f--) cout << HL;
    cout << r << '\n';
}

inline void vLine(const string& b, int w) {
    cout << VL << ' ' << b;
    int p = w - 3 - int(b.size()); while (p--) cout << ' ';
    cout << VL << '\n';
}

inline void printPktBox(const string& tag, const SlowPacket& p) {
    ios old(nullptr); old.copyfmt(cout);
    vector<string> rows; stringstream ss;

    ss << "SID  : "; for (uint8_t b : p.sid) ss << hex << setw(2) << setfill('0') << int(b);
    rows.push_back(ss.str()); ss.str(""); ss.clear(); ss.copyfmt(old);

    ss << "FLG  : " << flagsToString(p.flags) << "  STTL: " << p.sttl;
    rows.push_back(ss.str()); ss.str(""); ss.clear();

    ss << "SEQ  : " << p.seqnum << "   ACK : " << p.acknum;
    rows.push_back(ss.str()); ss.str(""); ss.clear();

    ss << "WIN  : " << p.window << "   FID/FO: " << int(p.fid) << '/' << int(p.fo);
    rows.push_back(ss.str());

    int w = 0; for (auto& r : rows) w = max(w, int(r.size())); w += 3;

    hLine(TL, TR, w, tag);
    for (auto& r : rows) vLine(r, w);
    hLine(BL, BR, w);
    cout << '\n'; cout.copyfmt(old);
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

    string l; size_t w = 0; vector<string> rows; istringstream is(ss.str());
    while (getline(is, l)) { rows.push_back(l); w = max(w, l.size()); }

    string bord(w + 4, '-');
    cout << "┌" << bord << "┐\n";
    for (auto& r : rows) cout << "│  " << left << setw(w) << r << "  │\n";
    cout << "└" << bord << "┘\n\n";
}

static void sendPureAck(Network& net, const sockaddr_in& srv, Session& s) {
    SlowPacket a;
    a.sid = s.sid; a.flags = ACK; a.seqnum = s.seqnum; a.acknum = s.acknum;
    a.window = s.recvWindow; a.sttl = s.sttl;
    uint32_t dummy; net.sendPacket(srv, a, dummy, s);
}

int main() {
    const char* HOST = "142.93.184.175"; const int PORT = SLOW_PORT;

    Network net;
    if (!net.createSocket()) { cerr << "socket() erro\n"; return 1; }

    sockaddr_in srv{}; srv.sin_family = AF_INET; srv.sin_port = htons(PORT);
    inet_pton(AF_INET, HOST, &srv.sin_addr);

    Session sess; sess.recvWindow = 7200; bool connected = false;

    if (!doThreeWayHandshake(net, srv, sess)) return 1;
    connected = true; cout << "[sucesso] Conectado.\n";

    while (true) {
        banner(); string line; if (!getline(cin, line)) break;

        auto trim = [](string& s) {
            auto b = s.find_first_not_of(" \t\r\n");
            auto e = s.find_last_not_of(" \t\r\n");
            s = (b == string::npos ? "" : s.substr(b, e - b + 1));
        };
        trim(line); if (line.empty()) continue;

        char cmd = tolower(line[0]);

        if (cmd == 'd') {
            if (!connected) {
                cout << "[erro] sem sessão (use r)\n";
                continue;
            }

            cout << "# Mensagem: ";
            string msg; getline(cin, msg);
            if (msg.empty()) continue;

            bool willFrag = msg.size() > MAX_DATA;
            if (willFrag) {
                cout << "Mensagem será fragmentada: " << msg.size()
                    << " bytes (DATA_MAX=" << MAX_DATA << ")\n";
                cout << "Preview: \"" << msg.substr(0, 100)
                    << (msg.size() > 100 ? "…" : "") << "\"\n";
            } else {
                cout << "Enviando mensagem sem fragmentar (" << msg.size()
                    << " bytes): \"" << msg.substr(0, 100)
                    << (msg.size() > 100 ? "…" : "") << "\"\n";
            }

            uint8_t fid = willFrag ? Session::generateUUID()[0] : 0;
            uint8_t fo  = 0;
            size_t  off = 0;

            while (off < msg.size()) {
                size_t freeWin = sess.remoteWindow - sess.bytesInFlight;
                if (!freeWin) {
                    sendPureAck(net, srv, sess);

                    SlowPacket ack; sockaddr_in f{};
                    if (net.receivePacket(ack, f) && (ack.flags & ACK)) {
                        sess.remoteWindow = ack.window;
                        sess.bytesInFlight = 0;
                        sess.acknum = ack.seqnum;
                        cout << "[debug] Janela atualizada: "
                            << ack.window << '\n';
                    }
                    continue;
                }

                size_t chunk = min<size_t>({ MAX_DATA, msg.size() - off, freeWin });

                SlowPacket p;
                p.sid = sess.sid;
                p.flags = ACK | ((off + chunk < msg.size()) ? MOREBITS : 0);
                p.seqnum = ++sess.seqnum;
                p.acknum = sess.acknum;
                p.window = sess.recvWindow;
                p.sttl = sess.sttl;
                p.fid = willFrag ? fid : 0;
                p.fo = willFrag ? fo++ : 0;
                p.data.assign(msg.begin() + off, msg.begin() + off + chunk);

                uint32_t lastSeq;
                net.sendPacket(srv, p, lastSeq, sess);
                sess.bytesInFlight += chunk;
                off += chunk;

                SlowPacket a; sockaddr_in from{};
                while (net.receivePacket(a, from)) {
                    if ((a.flags & ACK) && a.acknum == lastSeq) {
                        sess.remoteWindow  = a.window;
                        sess.bytesInFlight = 0;
                        sess.acknum        = a.seqnum;
                        cout << "[debug] Janela atualizada: "
                            << a.window << '\n';
                        break;
                    }
                }
            }
            cout << "[sucesso] Mensagem enviada (" << msg.size() << " B)\n";
        }

        else if (cmd == 'x') {
            if (!connected) { cout << "[já desconectado]\n"; continue; }

            SlowPacket disc;
            disc.sid = sess.sid;
            disc.flags = CONNECT | REVIVE | ACK;
            disc.seqnum = ++sess.seqnum;
            disc.acknum = sess.acknum;
            uint32_t last;
            net.sendPacket(srv, disc, last, sess);

            SlowPacket resp; sockaddr_in from{};
            if (net.receivePacket(resp, from) && (resp.flags & ACK)) {
                connected = false;
                sess.bytesInFlight = 0;
                cout << "[sucesso] Desconectado.\n";
            } else cout << "[erro] sem ACK do disconnect.\n";
        }

        else if (cmd == 'r') {
            if (connected) { cout << "[aviso] Já conectado. Use x.\n"; continue; }

            cout << "\nMensagem para revive? (ENTER = padrão) : ";
            cout.flush();
            string payload; getline(cin, payload);
            if (payload.empty()) payload = "revive";
            cout << '\n';

            SlowPacket r;
            r.sid = sess.sid;
            r.flags = REVIVE | ACK;
            r.seqnum = ++sess.seqnum;
            r.acknum = sess.acknum;
            r.window = sess.recvWindow;
            r.sttl = sess.sttl;
            r.data.assign(payload.begin(), payload.end());

            uint32_t lastSeq;
            net.sendPacket(srv, r, lastSeq, sess);

            SlowPacket resp; sockaddr_in from{};
            if (net.receivePacket(resp, from) && (resp.flags & (ACK | ACCEPT))) {
                sess.acknum = resp.seqnum;
                sess.remoteWindow = resp.window;
                sess.bytesInFlight = 0;
                connected = true;
                cout << "[revive OK]\n";
            } else cout << "[revive rejeitado]\n";
        }

        else if (cmd == '?') showStatus(sess, connected, HOST, PORT);
        else if (cmd == 'h') help();
        else if (cmd == 'q') { cout << "[tchau] sessão encerrada!\n"; break; }
        else cout << "[erro] comando inválido. 'h' ajuda.\n";
    }

    net.closeSocket();
    return 0;
}