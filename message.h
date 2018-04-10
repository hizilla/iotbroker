#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#include "protocol.h"
#include "list.h"

enum queue_state
{
    QS_WAIT,
    QS_INFLIGHT,
};

enum publish_state
{
    PS_WAIT_TO_PUBLISH,
    PS_WAIT_FOR_PUBACK,
    PS_WAIT_FOR_PUBREL,
    PS_WAIT_FOR_PUBREC,
    PS_WAIT_FOR_PUBCOMP,
};

enum message_dir
{
    MD_IN,
    MD_OUT,
};

typedef struct
{
    TopicPacket *packet; /*packet reference*/
    UINT32 refer_count; /*reference count*/
    struct list_head list_mount; /*mount point in the message list*/
}MessageStore;

typedef struct
{
    enum queue_state qs; /*message state in the queue*/
    enum publish_state ps; /*message publish state*/
    UINT8 resend_count; /*resend count*/
    UINT8 qos; /*qos*/
    enum message_dir dir;
    UINT16 packet_id; /*packet id*/
    struct list_head list_mount; /*mount point in the queue*/
    MessageStore *ms; /*point to the message store*/
}MessageQueue;

VOID iotbroker_message_store_init();

VOID iotbroker_message_store_deref(MessageStore *ms);

VOID iotbroker_message_store_insert(TopicPacket *tp, MessageStore **ms);

#endif
