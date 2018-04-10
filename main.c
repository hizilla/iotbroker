#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/types.h>

#include "net.h"
#include "message.h"
#include "subtree.h"
#include "iotbroker.h"

int main(int argc, char **argv)
{
	UINT32 ret, listenfd, epollfd;
    struct epoll_event events[EPOLLEVENTS];
    
    iotbroker_message_store_init();
    iotbroker_net_init(&listenfd, &epollfd);    
        
    for ( ; ; )
    {
        ret = epoll_wait(epollfd, events, EPOLLEVENTS, -1);
        iotbroker_handle_events(epollfd, events, ret, listenfd);
    }

    close(epollfd);
	
    return 0;
}
