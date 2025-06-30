#include "session.h"
#include <random>

/**
 * @file    session.cpp
 * @brief   Implementação da estrutura Session.
 *
 * Inclui rotina para geração de UUID aleatório conforme especificado.
 */


    
/** Constrói sessão vazia (UUID nil). */
Session::Session() noexcept { sid = nilUUID(); }

/* UUID todo zero (16 bytes) */
std::array<uint8_t, 16> Session::nilUUID() { return {}; }

/**
 * @brief Gera um UUID pseudo-aleatório.
 *
 * O algoritmo usa std::random_device para preencher 16 bytes
 * e ajusta bits das posições para construir UUIDv8 conforme especificado no RFC9562
 */
std::array<uint8_t, 16> Session::generateUUID() {
    std::array<uint8_t, 16> u;
    std::random_device rd;
    for (auto& b : u) b = static_cast<uint8_t>(rd());
    u[6] = (u[6] & 0x0F) | 0x80;
    u[8] = (u[8] & 0x3F) | 0x80;
    return u;
}