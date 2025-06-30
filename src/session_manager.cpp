#include "session_manager.h"
#include "packet.h"
#include <iostream>
using namespace std;
/**
 * @file    session_manager.cpp
 * @brief   Implementação da lógica de handshakes para sessões SLOW.
 *
 * Este arquivo define as funções que coordenam o estabelecimento e a
 * reativação de sessões. O formato dos pacotes e o uso das flags CONNECT,
 * ACCEPT, ACK e REVIVE seguem o comportamento especificado do protocolo.
 * Logs de diagnóstico são impressos durante os processos para auxiliar na depuração.
 */

/**
 * @brief Estabelece uma nova sessão usando o 3-way handshake.
 *
 * Envia um pacote com a flag CONNECT e a janela de recepção local.
 * Aguarda resposta com a flag ACCEPT e, se válida, inicializa os campos da sessão
 * com os dados recebidos do servidor. O número de sequência local é ajustado.
 *
 * @param net Interface de rede.
 * @param srv Endereço do servidor.
 * @param s Sessão a ser inicializada.
 * @return true em caso de sucesso, false se qualquer etapa falhar.
 */ 
bool doThreeWayHandshake(Network& net, sockaddr_in& srv, Session& s) {

    // envia CONNECT com a janela local
    SlowPacket syn;
    syn.flags = CONNECT;
    syn.window = s.recvWindow;

    uint32_t last;
    if (!net.sendPacket(srv, syn, last, s)) return false;

    //aguarda pacote com ACCEPT do servidor
    SlowPacket setup;
    sockaddr_in from{};
    if (!net.receivePacket(setup, from, s) || !(setup.flags & ACCEPT)) {
        cerr << "[HANDSHAKE] FAIL – SETUP inválido\n";
        return false;
    }

    //inicializa a sessão local com os dados recebidos
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

/**
 * @brief Tenta retomar uma sessão anterior com o flag REVIVE.
 *
 * Envia um pacote com REVIVE|ACK e payload indicativo.
 * Se o servidor responder com ACK ou ACCEPT, a sessão é reativada.
 *
 * @param net Interface de rede.
 * @param srv Endereço do servidor.
 * @param s Sessão contendo o UUID a ser reativado.
 * @return true se o revive for aceito, false caso contrário.
 */
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
        // sessão reativada com sucesso
        s.acknum = resp.seqnum;
        s.remoteWindow = resp.window;
        s.bytesInFlight = 0;
        s.connected = true;
        s.sttl = resp.sttl;
        return true;
    }

    return false;
}