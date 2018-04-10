#ifndef _PACKET_HANDLE_H_
#define _PACKET_HADNLE_H_

#include "iotbroker.h"
#include "protocol.h"
#include "message.h"
#include "session.h"

/*protocol name length*/
#define PROTOCOL_NAME_LEN 4

/*current max protocol level*/
#define PROTOCOL_MAX_LEVEL 4

/*clietn id max length*/
#define PROTOCOL_MAX_CLIENTID_LEN 30

/*qos level*/
#define QOS0 0
#define QOS1 1
#define QOS2 2

/*==============connection flags start===============*/
#define CONNECT_FLAG_RESERVED 0x01

#define CONNECT_FLAG_CLEAN_SESSION 0x02

#define CONNECT_FLAG_WILL_FLAG 0x04

#define CONNECT_FLAG_WILL_QOS 0x18

#define CONNECT_FLAG_WILL_RETAIN 0x20

#define CONNECT_FLAG_USERNAME 0x40

#define CONNECT_FLAG_PASSWORD 0x80
/*==============connection flags end===============*/

/*==============publish flags start===============*/
#define PUBLISH_FLAG_RETAIN 0x01

#define PUBLISH_FLAG_QOS 0x06

#define PUBLISH_FLAG_DUP 0x08
/*==============publish flags end===============*/

/*==============connection return code start===============*/
#define CONNECT_RET_OK 0x0

#define CONNECT_RET_INVALID_PROTOCOL_LEVEL 0x01

#define CONNECT_RET_INVALID_CLIENTID 0x02

#define CONNECT_RET_SERVER_UNAVAILABLE 0x03

#define CONNECT_RET_INVALID_USERNAME_PASSWD 0x04

#define CONNECT_RET_UNAUTHORIZED 0x05
/*==============connection return code end===============*/

/*==============handle return value start===============*/
#define HANDLE_RET_CLOSE_CLIENT 0x01

#define HANDLE_RET_REMOVE_MSG 0x02

#define HANDLE_RET_KEEP_MSG 0x03
/*==============handle return value end===============*/
struct sub_topic_ret
{
    UINT8 ret;
    struct sub_topic_ret *next;
};

INT32 handle_packet(Client *client, Packet *packet, Packet **out_packet);
INT32 handle_message_queue(Client *client, MessageQueue *mq, Packet **out_packet);

#endif
