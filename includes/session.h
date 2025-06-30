#ifndef SESSION_H
#define SESSION_H

/**
 * @file    session.h
 * @brief   Estrutura e utilitários para o gerenciamento de uma sessão SLOW.
 *
 * Cada sessão mantém seu UUID (16 bytes), seqnum e acknum
 * e controle de janela, exatamente como especificado para o protocolo.
 */

#include "slow.h"
#include <array>
#include <cstdint>

/**
 * @struct Session
 * @brief  Representa o estado de uma única sessão de transporte SLOW.
 *
 * Campos:
 *  • sid           – UUID da sessão (16 B)  
 *  • seqnum/acknum – controle de fluxo  
 *  • sttl          – _Session-TTL_ (valor imposto pelo central)  
 *  • recvWindow    – janela de recepção local  
 *  • remoteWindow  – janela anunciada pelo peer  
 *  • bytesInFlight – bytes enviados ainda não confirmados
 */
struct Session {
    std::array<uint8_t, 16> sid{};
    uint32_t seqnum = 0;
    uint32_t acknum = 0;
    uint32_t sttl = 0;
    uint16_t recvWindow = 7200;
    bool connected = false;

    uint32_t remoteWindow = 1024;
    uint32_t bytesInFlight = 0;

    Session() noexcept;
    static std::array<uint8_t, 16> generateUUID();
    static std::array<uint8_t, 16> nilUUID();
};

#endif