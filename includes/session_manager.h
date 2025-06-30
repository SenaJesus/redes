#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H
#include "network.h"
#include "session.h"

bool doThreeWayHandshake(Network& net, sockaddr_in& srv, Session& s);
bool tryRevive(Network& net, sockaddr_in& srv, Session& s);

#endif
