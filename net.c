#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/types.h>

#include "net.h"
#include "iotbroker.h"
#include "protocol.h"
#include "debug.h"
#include "memmanager.h"
#include "session.h"

/*accept the connect*/
STATIC VOID handle_accept(INT32 epollfd, INT32 listenfd);

/*handle read event*/
STATIC VOID handle_read(INT32 epollfd, INT32 fd);

/*handle write event*/
STATIC VOID handle_write(INT32 epollfd, INT32 fd);

/*handle disconnect event*/
STATIC VOID handle_disconnect(INT32 epollfd, INT32 fd);

/*add epoll event*/
STATIC VOID add_event(INT32 epollfd,INT32 fd,INT32 state);

/*modify epoll event*/
STATIC VOID modify_event(INT32 epollfd,INT32 fd,INT32 state);

/*delete epoll event*/
STATIC VOID delete_event(INT32 epollfd,INT32 fd,INT32 state);


VOID iotbroker_net_init(INT32 *out_listenfd, INT32 *out_epollfd)
{
    INT32 listenfd, epollfd;
    struct sockaddr_in servaddr;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == listenfd)
    {
        perror("socket error:");
        exit(FAILED);
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, IPADDRESS, &servaddr.sin_addr);
    servaddr.sin_port = htons(PORT);

    if (bind(listenfd,(struct sockaddr*)&servaddr,sizeof(servaddr)) != SUCESS)
    {
        perror("bind error: ");
        exit(FAILED);
    }

    listen(listenfd, LISTENQ);

    epollfd = epoll_create(FDSIZE);
    add_event(epollfd, listenfd, EPOLLIN);
    
    *out_listenfd = listenfd;
    *out_epollfd = epollfd;
}

VOID iotbroker_handle_events(INT32 epollfd, struct epoll_event *events, INT32 num, INT32 listenfd)
{
    INT32 i;
    INT32 fd;

    for(i = 0; i < num; i++)
    {
        fd = events[i].data.fd;

        if((fd == listenfd) && (events[i].events & EPOLLIN))
        {
            /*the event from listen sock*/
            handle_accept(epollfd, listenfd);
        }
        else if(events[i].events & EPOLLIN)
        {   
            /*read event*/
            handle_read(epollfd, fd);
        }
        else if(events[i].events & EPOLLOUT)
        {
            /*write event*/
            handle_write(epollfd, fd);
        }
    }
}

STATIC VOID handle_accept(INT32 epollfd, INT32 listenfd)
{
    INT32 clifd;
    struct sockaddr_in cliaddr;
    socklen_t cliaddrlen = sizeof(struct sockaddr);
    
    clifd = accept(listenfd, (struct sockaddr*)&cliaddr, &cliaddrlen);
    if (clifd < 0)
    {
        perror("accpet error:");
    }
    else
    {   
#ifdef DEBUG    
        printf("accept a new client: %s:%d\n", inet_ntoa(cliaddr.sin_addr), cliaddr.sin_port);
#endif  
        iotbroker_session_add(clifd, inet_ntoa(cliaddr.sin_addr), cliaddr.sin_port);
        add_event(epollfd, clifd, EPOLLIN | EPOLLOUT);
    }
}

STATIC VOID handle_disconnect(INT32 epollfd, INT32 fd)
{
    delete_event(epollfd,fd,EPOLLIN | EPOLLOUT);
    close(fd); 
    iotbroker_session_clean(fd);  
}

STATIC VOID handle_read(INT32 epollfd, INT32 fd)
{
    UINT32 ret;
    
    ret = iotbroker_read_packet(fd);
    if(ret != SUCESS)
    {
#ifdef DEBUG
        printf("%s %d\n\
            \tfail: %d\n", __FILE__, __LINE__, ret);
        perror("read error:");
#endif
        handle_disconnect(epollfd, fd);
    }

}

STATIC VOID handle_write(INT32 epollfd, INT32 fd)
{
    UINT32 ret;
    
    ret = iotbroker_write_packet(fd);
    
    if(ret != SUCESS)
    {
#ifdef DEBUG
        printf("%s %d\n\
            \tfail: %d\n", __FILE__, __LINE__, ret);
        perror("read error:");
#endif
        handle_disconnect(epollfd, fd);
    }
}

STATIC VOID add_event(INT32 epollfd, INT32 fd, INT32 state)
{
    struct epoll_event ev;
    UINT32 ret;
    
    ev.events = state;
    ev.data.fd = fd;
    ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
    
    if(ret != SUCESS)
    {
        perror("epoll_ctl error:");
    }
}

STATIC VOID delete_event(INT32 epollfd, INT32 fd, INT32 state)
{
    struct epoll_event ev;
    UINT32 ret;
    
    ev.events = state;
    ev.data.fd = fd;
    ret = epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &ev);
    
    if(ret != SUCESS)
    {
        perror("epoll_ctl error:");
    }
}

STATIC VOID modify_event(INT32 epollfd, INT32 fd, INT32 state)
{
    struct epoll_event ev;
    UINT32 ret;
    
    ev.events = state;
    ev.data.fd = fd;
    ret = epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
    
    if(ret != SUCESS)
    {
        perror("epoll_ctl error:");
    }
}
