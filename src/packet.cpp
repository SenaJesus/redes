#include "packet.h"
#include <cstring>
#include <iostream>

/**
 * @file    packet.cpp
 * @brief   Rotinas de serialização/desserialização do pacote SLOW.
 *
 * Todo campo é codificado em formato little-endian (LE)
 *
 * Funções auxiliares `packLE`/`unpackLE` cuidam da conversão LE
 * de inteiros de até 32 bits, enquanto `SlowPacket::{serialize,deserialize}`
 * convertem entre a representação em memória e esse formato de rede.
 */

using namespace std;

SlowPacket::SlowPacket() noexcept {}

/**
 * @brief Exibe o cabeçalho do pacote em formato “caixa” (debug).
 */
void SlowPacket::printVerbose() const {
    logPacket(*this, "PACKET VERBOSE");
}

/**
 * @brief Escreve um inteiro de até 32 bits em ordem little-endian.
 *
 * @param p  Buffer destino.
 * @param v  Valor a gravar.
 * @param n  Quantidade de bytes (1-4) a escrever.
 *
 */
void packLE(uint8_t* p, uint32_t v, int n) {
    for (int i = 0; i < n; ++i) {
        p[i] = v & 0xFF;
        v >>= 8;
    }
}


/**
 * @brief Lê um inteiro em ordem little-endian.
 *
 * @param p  Ponteiro para o primeiro byte.
 * @param n  Quantidade de bytes (1-4) a ler.
 * @return   Valor reconstruído.
 */
uint32_t unpackLE(const uint8_t* p, int n) {
    uint32_t v = 0;
    for (int i = 0; i < n; ++i) v |= uint32_t(p[i]) << (8 * i);
    return v;
}

/**
 * @brief Constrói o buffer on-wire a partir da estrutura em memória.
 *
 * @param buf Buffer destino (≥ 1472 B).
 * @param len Devolve o total de bytes gravados.
 *
 * A função escreve cada campo na ordem definida pelo protocolo.
 * O par `(sttl, flags)` é compactado em um único word LE:
 * `sf = (sttl << 5) | flags`.
 */
void SlowPacket::serialize(uint8_t* buf, size_t& len) const {
    size_t off = 0;
    memcpy(buf + off, sid.data(), UUID_SIZE); off += UUID_SIZE;

    uint32_t sf = ((sttl & 0x07FFFFFFu) << 5) | (flags & 0x1Fu);
    packLE(buf + off, sf, 4); off += 4;
    packLE(buf + off, seqnum, 4); off += 4;
    packLE(buf + off, acknum, 4); off += 4;
    packLE(buf + off, window, 2); off += 2;

    buf[off++] = fid;
    buf[off++] = fo;

    if (!data.empty()) {
        memcpy(buf + off, data.data(), data.size());
        off += data.size();
    }
    len = off;
}

/**
 * @brief Lê um buffer on-wire e preenche a estrutura em memória.
 *
 * @param buf Buffer de entrada.
 * @param len Número de bytes válidos em `buf`.
 */
void SlowPacket::deserialize(const uint8_t* buf, size_t len) {
    size_t off = 0;
    memcpy(sid.data(), buf + off, UUID_SIZE); off += UUID_SIZE;

    uint32_t sf = unpackLE(buf + off, 4); off += 4;
    flags = sf & 0x1Fu;
    sttl = sf >> 5;

    seqnum = unpackLE(buf + off, 4); off += 4;
    acknum = unpackLE(buf + off, 4); off += 4;
    window = static_cast<uint16_t>(unpackLE(buf + off, 2)); off += 2;

    fid = buf[off++];
    fo = buf[off++];

    data.clear();
    if (len > off) data.assign(buf + off, buf + len);
}