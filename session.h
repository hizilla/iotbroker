#ifndef _SESSION_H_
#define _SESSION_H_

#include "message.h"
#include "uthash.h"

enum client_sate
{
    CS_WAIT_FOR_CONNECT,
    CS_CONNECTING,
    CS_DISCONNECT,
};

typedef struct
{
    INT32 sock_fd; /*client socket fd*/
    
    UINT8 *client_id; /*client id*/
    
    UINT16 packet_id_source; /*packet id gen*/
    
    UINT8 *username; /*client username*/
    UINT8 *password; /*client password*/
    
    UINT8 *address; /*client ip address*/
    UINT16 port; /*client port*/
    
    enum client_sate state; /*client state*/
    
    MessageQueue *mq_head; /*message queue*/
    
    UT_hash_handle hh; /*hashtable handle*/
} Client;

VOID iotbroker_session_add(UINT32 sockfd, CONST UINT8 *ip, UINT32 port);

VOID iotbroker_session_get(UINT32 sockfd, Client **client);

VOID iotbroker_session_setid(UINT32 sockfd, UINT8 *client_id);

VOID iotbroker_session_clean(UINT32 sockfd);

UINT32 iotbroker_session_auth(UINT32 sockfd, UINT8 *username, UINT8 *password);

VOID iotbroker_session_state_mod(UINT32 sockfd, enum client_sate newstate);
#endif
