#include <assert.h>
#include <string.h>

#include "iotbroker.h"
#include "memmanager.h"
#include "session.h"
#include "message.h"
#include "list.h"
#include "uthash.h"
#include "debug.h"

STATIC Client *g_client_session_head = NULL;

STATIC VOID display_session_table()
{
    Client *c_debug, *c_tmp;
    
    printf("\nclient session table as follow:\n");
    printf("=====================================\n");
    printf(" socknum        ip:port        state\n");
    printf("-------------------------------------\n");
    HASH_ITER(hh, g_client_session_head, c_debug, c_tmp)
    {
        printf("%8d %s:%d %6d\n", c_debug->sock_fd, c_debug->address, c_debug->port, c_debug->state);
    }
    printf("=====================================\n");
}

VOID iotbroker_session_get(UINT32 sockfd, Client **client)
{
    assert(client != NULL);
    
    HASH_FIND_INT(g_client_session_head, &sockfd, *client); 
}

VOID iotbroker_session_add(UINT32 sockfd, CONST UINT8 *ip, UINT32 port)
{
    Client *c;
    UINT8 *ip_str, ip_len;
    MessageQueue *mq_head;
    
    assert(ip != NULL);
    
    HASH_FIND_INT(g_client_session_head, &sockfd, c); 
    INVALID_RETURN_NOVALUE(c == NULL);
    
    c = (Client*)iotbroker_malloc(sizeof(Client));
    assert(c != NULL);
    memset(c, 0, sizeof(Client));
    
    c->sock_fd = sockfd;
    
    ip_len = strlen(ip);
    ip_str = (UINT8*)iotbroker_malloc(ip_len + 1);
    assert(ip_str != NULL);
    memset(ip_str, 0, ip_len + 1);
    strncpy(ip_str, ip, ip_len);
    c->address = ip_str;
    
    c->port = port;
    c->state = CS_WAIT_FOR_CONNECT;
    c->packet_id_source = 1;
    
    mq_head = (MessageQueue*)iotbroker_malloc(sizeof(MessageQueue));
    assert(mq_head != NULL);
    memset(mq_head, 0, sizeof(mq_head));
    INIT_LIST_HEAD(&mq_head->list_mount);
    c->mq_head = mq_head;
    
    /*add to hash table*/
    HASH_ADD_INT(g_client_session_head, sock_fd, c);
 
#ifdef DEBUG   
    display_session_table();
#endif

}

UINT32 iotbroker_session_auth(UINT32 sockfd, UINT8 *username, UINT8 *password)
{
    Client *c = NULL;
    
    /*username or password should not be NULL*/
    assert(username != NULL || password != NULL);
    
    HASH_FIND_INT(g_client_session_head, &sockfd, c); 
    INVALID_RETURN_VALUE(c != NULL, FAILED);
    
    c->username = username;
    c->password = password;
    
    return SUCESS;
}

VOID iotbroker_session_setid(UINT32 sockfd, UINT8 *client_id)
{
    Client *c = NULL;
    
    /*username or password should not be NULL*/
    assert(client_id != NULL);
    
    HASH_FIND_INT(g_client_session_head, &sockfd, c); 
    INVALID_RETURN_NOVALUE(c != NULL);
    
    c->client_id = client_id;
}

VOID iotbroker_session_clean(UINT32 sockfd)
{
    Client *c = NULL;
    MessageQueue *mq_head;
    struct list_head *node_pos, *node_tmp;
    
    HASH_FIND_INT(g_client_session_head, &sockfd, c); 
    INVALID_RETURN_NOVALUE(c != NULL);
    
    if(c->client_id != NULL)
    {
        iotbroker_free(c->client_id);
    }
    
    if(c->username != NULL)
    {
        iotbroker_free(c->username);
    }
    
    if(c->password != NULL)
    {
        iotbroker_free(c->password);
    }
    
    if(c->address != NULL)
    {
        iotbroker_free(c->address);
    }
    
    mq_head = c->mq_head;
    list_for_each_safe(node_pos, node_tmp, &mq_head->list_mount)
    {
        MessageQueue *mq_tmp;
        
        list_del(node_pos);
        mq_tmp = container_of(node_pos, MessageQueue, list_mount);
        iotbroker_message_store_deref(mq_tmp->ms);
        iotbroker_free(mq_tmp);
    }
	
    iotbroker_free(mq_head);
    HASH_DEL(g_client_session_head, c);
    iotbroker_free(c);
    
#ifdef DEBUG   
    display_session_table();
#endif

}

VOID iotbroker_session_state_mod(UINT32 sockfd, enum client_sate newstate)
{
    Client *c = NULL;
    
    HASH_FIND_INT(g_client_session_head, &sockfd, c); 
    
    if(NULL == c)
    {
		/*TODO:disconnect*/
        return;
    }
    
    c->state = newstate;
    
#ifdef DEBUG   
    display_session_table();
#endif

}
