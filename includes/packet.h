#ifndef PACKET_H
#define PACKET_H

#include "slow.h"
#include <array>
#include <cstdint>
#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

enum SLOWFlags : uint8_t {
    CONNECT = 1 << 4,
    REVIVE = 1 << 3,
    ACK = 1 << 2,
    ACCEPT = 1 << 1,
    MOREBITS = 1 << 0
};

inline std::string flagsToString(uint8_t f, bool longNames = true)
{
    std::vector<std::string> v;
    auto push = [&](uint8_t m, const char* shortN, const char* longN) { if (f & m) v.emplace_back(longNames ? longN : shortN); };

    push(CONNECT, "C", "CONNECT");
    push(REVIVE, "R", "REVIVE");
    push(ACK, "A", "ACK");
    push(ACCEPT, "P", "ACCEPT");
    push(MOREBITS, "M", "MOREBITS");

    if (v.empty()) return "-";

    std::string out = v[0];
    for (size_t i = 1; i < v.size(); ++i) out += (longNames ? "|" : "") + v[i];

    return out;
}

struct SlowPacket {
    std::array<uint8_t, UUID_SIZE> sid{};
    uint8_t flags = 0;
    uint32_t sttl = 0;
    uint32_t seqnum = 0;
    uint32_t acknum = 0;
    uint16_t window = 0;
    uint8_t fid = 0;
    uint8_t fo = 0;
    std::vector<uint8_t> data;

    SlowPacket() noexcept;
    void printVerbose() const;
    void serialize(uint8_t* dst, size_t& len) const;
    void deserialize(const uint8_t* src, size_t len);
};

void packLE(uint8_t* dst, uint32_t v, int nbytes);
uint32_t unpackLE(const uint8_t* src, int nbytes);

namespace detail {

inline void hLine(const char* left, const char* right, int boxW, const std::string& title = "") {
    std::cout << left;
    if (!title.empty()) std::cout << ' ' << title << ' ';
    int fill = boxW - 2 - (title.empty() ? 0 : int(title.size()) + 2);
    while (fill-- > 0) std::cout << u8"═";
    std::cout << right << '\n';
}

inline void vLine(const std::string& body, int boxW) {
    std::cout << u8"║ " << body;
    int pad = boxW - 3 - int(body.size());
    while (pad-- > 0) std::cout << ' ';
    std::cout << u8"║\n";
}

}

inline void logPacket(const SlowPacket& p, const std::string& tag) {
    std::ios old(nullptr); old.copyfmt(std::cout);

    std::ostringstream oss;
    oss << "SID  : ";
    for (uint8_t b : p.sid) oss << std::hex << std::setw(2) << std::setfill('0') << int(b);
    std::string l_sid = oss.str(); oss.str(""); oss.clear();
    oss.copyfmt(old);

    oss << "FLG  : " << std::setw(3) << std::left << flagsToString(p.flags) << "  STTL: " << p.sttl;
    std::string l_flg = oss.str(); oss.str(""); oss.clear();

    oss << "SEQ  : " << p.seqnum << "   ACK : " << p.acknum;
    std::string l_seq = oss.str(); oss.str(""); oss.clear();

    oss << "WIN  : " << p.window << "   FID/FO: " << int(p.fid) << '/' << int(p.fo);
    std::string l_win = oss.str();

    int inner = std::max({ int(l_sid.size()), int(l_flg.size()), int(l_seq.size()), int(l_win.size()) }) + 2;
    int boxW = inner + 2;

    using namespace detail;
    hLine(u8"╔", u8"╗", boxW, tag);
    vLine(l_sid, boxW);
    vLine(l_flg, boxW);
    vLine(l_seq, boxW);
    vLine(l_win, boxW);
    hLine(u8"╚", u8"╝", boxW);
    std::cout << '\n';

    std::cout.copyfmt(old);
}

inline void printHeaderMini(const SlowPacket& p, const std::string& lbl) {
    logPacket(p, lbl);
}

#endif