#ifndef _SUBTREE_H_
#define _SUBTREE_H_

#include "iotbroker.h"
#include "list.h"
#include "session.h"
#include "uthash.h"

typedef struct
{
    Client *client; /*the subscriber*/
    UINT8 qos;
    struct list_head list_mount;
}SubNode;

typedef struct
{
    UINT8 *topic; /*topic name*/
    SubNode sublist; /*subscribe list*/
    UT_hash_handle hh;
}TreeNode;

struct topic_token
{
    UINT8 *name;
    struct topic_token *next;
};

VOID iotbroker_subtree_sub(TopicPacket *tp, Client *client);

VOID iotbroker_subtree_unsub(TopicPacket *tp, Client *client);

VOID iotbroker_subtree_pub(MessageStore *ms);

#endif
