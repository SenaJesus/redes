#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H

/**
 * @file    session_manager.h
 * @brief   Módulo responsável pela criação e reativação de sessões SLOW.
 *
 * Fornece funções para iniciar uma nova sessão com o 3-way handshake,
 *  ou para tentar retomar uma sessão anterior usando o 0-way, ou seja
 * mecanismo de revive. As interações seguem o comportamento esperado do
 * protocolo, com troca de pacotes contendo flags apropriadas, controle
 * de sequência e gerenciamento de janelas.
 */

#include "network.h"
#include "session.h"

bool doThreeWayHandshake(Network& net, sockaddr_in& srv, Session& s);
bool tryRevive(Network& net, sockaddr_in& srv, Session& s);

#endif
