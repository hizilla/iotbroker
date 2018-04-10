#ifndef _NET_H_
#define _NET_H_

#include "iotbroker.h"

#define IPADDRESS   "0.0.0.0"
#define PORT        1883
#define MAXSIZE     1024
#define LISTENQ     5
#define FDSIZE      1000
#define EPOLLEVENTS 100

VOID iotbroker_net_init(INT32 *out_listenfd, INT32 *out_epollfd);

VOID iotbroker_handle_events(INT32 epollfd, struct epoll_event *events, INT32 num, INT32 listenfd);

#endif
