#include "network.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <sys/select.h>
using namespace std;

/**
 * @file    network.cpp
 * @brief Implementação da lógica de envio e recepção confiável sobre UDP no protocolo SLOW.
 *
 * Envia pacotes via UDP, recebe respostas, gerencia timeout e
 * retransmissão, e controla a janela de envio segundo os limites
 * da sessão remota. Pacotes com payload são mantidos em buffer
 * até serem confirmados por ACK.
 */

 
/**
 * @brief Adiciona um pacote enviado à fila de pendentes.
 */
void Network::pushPending(const uint8_t* buf, size_t len, uint32_t seq, size_t dsz) {
    pend.emplace_back(buf, len, seq, dsz);
}

/**
 * @brief Remove da fila os pacotes já confirmados via ACK.
 */
void Network::dropAcked(uint32_t ack, Session& sess) {
    while (!pend.empty() && pend.front().seq <= ack) {
        sess.bytesInFlight -= pend.front().dataSz;
        pend.pop_front();
    }
}

/**
 * @brief Reenvia o primeiro pacote pendente, se necessário.
 *
 * Se o tempo decorrido desde o último envio exceder RETRY_MS,
 * tenta retransmitir até MAX_TRIES. Após isso, descarta o pacote.
 *
 * @return true se houve retransmissão; false se nada foi feito.
 */
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

/**
 * @brief Envia um pacote pela rede e gerencia janela de envio.
 */
bool Network::sendPacket(const sockaddr_in& addr, const SlowPacket& pkt, uint32_t& lastSeq, Session& sess) {
    uint8_t buf[MAX_PACKET]; size_t len;
    pkt.serialize(buf, len);
    logPacket(pkt, "TX");
    if (!pkt.data.empty()) {
        size_t show = min<size_t>(50, pkt.data.size());
        string prev(pkt.data.begin(), pkt.data.begin() + show);
        cout << "✉  DATA (" << pkt.data.size() << " B): \"" << prev << (pkt.data.size() > show ? "…" : "") << "\"\n\n";
    }
    // se exceder a janela da outra ponta, aguarda ACK
    if (sess.bytesInFlight + pkt.data.size() > sess.remoteWindow) {
        cerr << "[FLOW] janela cheia, aguardando ACK\n";
        lastSeq = pkt.seqnum;
        return false;
    }
    ssize_t sent = ::sendto(sockfd, buf, len, 0, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    if (sent != static_cast<ssize_t>(len)) return false;
    sess.bytesInFlight += pkt.data.size();
    lastSeq = pkt.seqnum;
    // se for pacote com dados, adiciona à fila para retransmissão
    pushPending(buf, len, pkt.seqnum, pkt.data.size());
    return true;
}

/**
 * @brief Tenta receber um pacote, com timeout e suporte a retransmissão.
 */
bool Network::receivePacket(SlowPacket& pkt, sockaddr_in& from, Session& sess) {
    uint8_t buf[MAX_PACKET]; socklen_t alen = sizeof(from);
    fd_set rf; FD_ZERO(&rf); FD_SET(sockfd, &rf);
    // espera até 500ms por um pacote
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
    // atualiza sttl e controle de janela
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