#ifndef NETWORK_H
#define NETWORK_H

/**
 * @file    network.h
 * @brief   Camada de envio e recepção de pacotes SLOW via UDP.
 *
 * Esta classe encapsula a lógica de envio, recepção, controle
 * de janela e retransmissão confiável sobre UDP. Mantém uma
 * fila de pacotes pendentes aguardando ACK, com timeout e limite
 * de tentativas.
 */

#include "packet.h"
#include "session.h"
#include <array>
#include <deque>
#include <cstring>
#include <chrono>
#include <netinet/in.h>

/**
 * @brief Retorna timestamp atual em milissegundos.
 */
static inline uint64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

/**
 * @class Network
 * @brief Interface de envio e recepção para o protocolo SLOW.
 *
 * Gerencia socket UDP, controle de fluxo, envio com retransmissão
 * confiável, e integração com a estrutura de sessão.
 */
class Network {
public:
    Network(); ~Network();
     /**
     * @brief Cria o socket UDP.
     * @return true se o socket foi criado com sucesso.
     */
    bool createSocket();
    /**
     * @brief Envia um pacote via UDP.
     *
     * Se houver payload, o pacote é colocado na fila de retransmissão.
     * Respeita a janela de envio da sessão remota.
     *
     * @param addr     Destino.
     * @param pkt      Pacote SLOW.
     * @param lastSeq  Último seqnum transmitido (retorno).
     * @param sess     Sessão associada.
     * @return true se o envio foi realizado.
     */
    bool sendPacket(const sockaddr_in& addr, const SlowPacket& pkt, uint32_t& lastSeq, Session& sess);
    /**
     * @brief Tenta receber um pacote do socket.
     *
     * Se não houver dados após o timeout, chama retransmissão.
     * Atualiza o estado da sessão com base no pacote recebido.
     *
     * @param pkt   Pacote recebido.
     * @param from  Endereço de origem.
     * @param sess  Sessão a ser atualizada.
     * @return true se algo foi recebido com sucesso.
     */
    bool receivePacket(SlowPacket& pkt, sockaddr_in& from, Session& sess);
    void closeSocket();

private:
/**
     * @brief Representa um pacote aguardando ACK.
     */
    struct Pending {
        std::array<uint8_t, MAX_PACKET> buf;
        size_t len;
        uint32_t seq;
        size_t dataSz;
        uint64_t sentAt;
        int tries;
        Pending(const uint8_t* b, size_t l, uint32_t s, size_t d)
        : len(l), seq(s), dataSz(d), sentAt(nowMs()), tries(0) {
            std::memcpy(buf.data(), b, l);
        }
    };

    static constexpr uint64_t RETRY_MS = 500; // Timeout de retransmissão (ms)
    static constexpr int MAX_TRIES = 5; // Máximo de tentativas por pacote

    std::deque<Pending> pend; // Fila de pacotes aguardando ACK
    void pushPending(const uint8_t* buf, size_t len, uint32_t seq, size_t dsz);
    void dropAcked(uint32_t ack, Session& sess);
    bool retransmit(const sockaddr_in& addr, Session& sess);
    int sockfd = -1;
};

#endif