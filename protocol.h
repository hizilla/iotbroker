#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#include "iotbroker.h"
#include "list.h"

#define MAX_REMAIN_BYTE_LEN 4

#define ERROR_SOCK_READ_WRITE 0x01

#define ERROR_SOCK_CLIENT_CLOSE 0x02

#define ERROR_SOCK_PACKET_ERROR 0x03

#define ERROR_SOCK_CLIENT_NOEXIST 0x04

enum control_type
{
    MIN_CONTROL_TYPE = 0,
    CONNECT,
    CONNACK,
    PUBLISH,
    PUBACK,
    PUBREC,
    PUBREL,
    PUBCOMP,
    SUBSCRIBE,
    SUBACK,
    UNSUBSCRIBE,
    UNSUBACK,
    PINGREQ,
    PINGRESP,
    DISCONNECT,
    MAX_CONTROL_TYPE,
};

typedef struct
{
    UINT16 packet_id;
    UINT8 *topic;
    UINT8 *content;
    UINT8 qos;
    UINT8 dup;
    UINT8 retain;
}TopicPacket;

typedef struct 
{
    enum control_type type; /*packet type*/
    UINT8 flags; /*packet control flags*/
    INT32 remain_len; /*packet remain length*/
    UINT8 *load; /*load*/
    UINT32 current_pos; /*read buffer current pos*/
    UINT32 load_pos;
} Packet;

/*read packet from buffer*/
INT32 iotbroker_read_packet(UINT32 sock_fd);

/*write packet to buffer*/
INT32 iotbroker_write_packet(UINT32 sock_fd);

#endif
