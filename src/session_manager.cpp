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

    uint32_t dummy;
    net.sendPacket(srv, syn, dummy, s);

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

    SlowPacket ack;
    ack.sid = s.sid;
    ack.flags = ACK;
    ack.seqnum = s.seqnum;
    ack.acknum = s.acknum;
    ack.window = s.recvWindow;
    ack.sttl = s.sttl;

    net.sendPacket(srv, ack, dummy, s);
    return true;
}

/**
 * @brief Tenta retomar uma sessão anterior (zero-way revive).
 *
 * O cliente envia um pacote **REVIVE | ACK** contendo o UUID da sessão
 * e um pequeno payload (“revive”).  A resposta VÁLIDA do servidor deve
 * obrigatoriamente trazer o bit **ACCEPT** (sessão pronta) e, conforme
 * o PDF, também deve carregar **ACK** confirmando o nosso último seq.
 *
 * Se (ACCEPT && ACK) estiverem presentes, os campos da estrutura
 * Session são atualizados e a conexão volta ao estado “connected”.
 *
 * @param net  Interface de rede já inicializada.
 * @param srv  Endereço UDP do servidor SLOW.
 * @param s    Sessão local que se deseja reviver.
 * @return     true  se o revive foi aceito;  
 *             false se rejeitado ou timeout.
 */
bool tryRevive(Network& net, sockaddr_in& srv, Session& s)
{
    /* -------- envia REVIVE ------------------------------------ */
    SlowPacket r;
    r.sid = s.sid;
    r.flags = REVIVE | ACK;
    r.seqnum = ++s.seqnum;
    r.acknum = s.acknum;
    r.data.assign({'r','e','v','i','v','e'});   // payload opcional

    uint32_t dummy;
    net.sendPacket(srv, r, dummy, s);

    /* -------- aguarda resposta -------------------------------- */
    SlowPacket resp;
    sockaddr_in from{};
    if (!net.receivePacket(resp, from, s))
        return false;                      // timeout / erro de rede

    /* -------- valida bits ------------------------------------- */
    bool hasAccept = resp.flags & ACCEPT;
    bool hasAck = resp.flags & ACK;

    if (!hasAccept || !hasAck)             // precisa dos DOIS bits
        return false;

    /* -------- atualiza estado da sessão ----------------------- */
    s.acknum = resp.seqnum;
    s.remoteWindow = resp.window;
    s.bytesInFlight = 0;
    s.connected = true;
    s.sttl = resp.sttl;           // espelha STTL mais recente
    return true;
}
