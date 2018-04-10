#include <assert.h>
#include <string.h>

#include "iotbroker.h"
#include "memmanager.h"
#include "subtree.h"
#include "protocol.h"
#include "uthash.h"
#include "session.h"
#include "list.h"
#include "debug.h"
#include "message.h"

STATIC TreeNode *g_subtree_head = NULL;

STATIC VOID get_topic_tokens(UINT8 *topic, struct topic_token *tt_head)
{
    INT32 topic_len, pos, start = -1;
    struct topic_token *tmp_tt_head = tt_head;
 
 #ifdef DEBUG
    struct topic_token *debug_tt_head;
 #endif
    
    topic_len = strlen(topic);
    
    /*contain '\0'*/
    for(pos = 0; pos <= topic_len; pos++)
    {
        /*level separator*/
        if('/' == topic[pos] || '\0' == topic[pos])
        {
            UINT8 *token_name;
            UINT32 token_len = pos - start - 1;
            struct topic_token *tt;
            
            token_name = (UINT8*)iotbroker_malloc(token_len + 1);
            assert(token_name != NULL);
            memset(token_name, 0, token_len + 1);
            memcpy(token_name, topic + start + 1, token_len);
            
            tt = (struct topic_token*)iotbroker_malloc(sizeof(struct topic_token));
            assert(tt != NULL);
            tt->name = token_name;
            tt->next = NULL;
            
            tmp_tt_head->next = tt;
            tmp_tt_head = tt;
            
            start = pos;
        }
    }
#ifdef DEBUG
    printf("\ntopic %s is separated as follow:\n", topic);
    printf("=====================================\n");
    
    debug_tt_head = tt_head->next;
    while(debug_tt_head != NULL)
    {
        printf("[%s] ", debug_tt_head->name); 
        debug_tt_head = debug_tt_head->next;
    }
    
    printf("\n");
    printf("=====================================\n");    
#endif
}

STATIC VOID free_topic_tokens(struct topic_token *tt_head)
{
    struct topic_token *tt_tmp;
    
    tt_head = tt_head->next;
    
    while(tt_head != NULL)
    {
        tt_tmp = tt_head->next;
        iotbroker_free(tt_head->name);
        iotbroker_free(tt_head);
        tt_head = tt_tmp;
    }
}

STATIC VOID display_subtree()
{
    TreeNode *tn, *tmp;
       
    printf("\nsubscribe table as follow:\n");
    printf("=====================================\n");
    HASH_ITER(hh, g_subtree_head, tn, tmp)
    {
        SubNode *sub_list;
        struct list_head *pos;
        
        printf("%s\n", tn->topic);
        
        sub_list = &tn->sublist;
        list_for_each(pos, &sub_list->list_mount)
        {
            SubNode *sub_node = container_of(pos, SubNode, list_mount);
            Client *client = sub_node->client;
            
            printf("          %s:%d QoS%d\n", client->address, client->port, sub_node->qos);
        }      
    }
    printf("=====================================\n");
}

STATIC INT32 insert_message_to_subtree(UINT8 *topic_name, MessageStore *ms)
{
    TreeNode *tn;
    SubNode *sublist;
    struct list_head *pos;
    
    HASH_FIND_STR(g_subtree_head, topic_name, tn);
    INVALID_RETURN_VALUE(tn != NULL, FAILED);
    
    sublist = &tn->sublist;
    
    if(list_empty(&sublist->list_mount))
    {
        return FAILED;
    }
    
    list_for_each(pos, &sublist->list_mount)
    {
        SubNode *sn;
        Client *client;
        MessageQueue *mq, *new_msg;
        
        sn = container_of(pos, SubNode, list_mount);
        client = sn->client;
        mq = client->mq_head;
        
        new_msg = (MessageQueue*)iotbroker_malloc(sizeof(MessageQueue));
        assert(new_msg != NULL);
        new_msg->qs = QS_INFLIGHT;
        new_msg->ps = PS_WAIT_TO_PUBLISH;
        new_msg->dir = MD_OUT;
        new_msg->resend_count = 0;
        new_msg->ms = ms;
        new_msg->qos = MIN(ms->packet->qos, sn->qos);
        list_add(&new_msg->list_mount, &mq->list_mount);
        
        ms->refer_count++;
    }
    
    return SUCESS;
}

STATIC VOID insert_pub_message(struct topic_token *tt, UINT8 *topic_buf, MessageStore *ms)
{
    UINT8 *token_cur;
    UINT32 token_cur_len, topic_buf_len;
    
    /*end of calling*/
    if(NULL == tt)
    {
        return;
    }    
    
    topic_buf_len = strlen(topic_buf);
    
    /*1.find current token*/
    token_cur = tt->name;
    token_cur_len = strlen(token_cur);
    memcpy(topic_buf+topic_buf_len, token_cur, token_cur_len);
#ifdef DEBUG
    printf("%s\n", topic_buf);
#endif

    if(SUCESS == insert_message_to_subtree(topic_buf, ms))
    {
        topic_buf[topic_buf_len + token_cur_len] = '/';
        insert_pub_message(tt->next, topic_buf, ms);
    }    
    memset(topic_buf+topic_buf_len, 0, token_cur_len);
    
    /*2.find +*/
    topic_buf[topic_buf_len] = '+';
#ifdef DEBUG
    printf("%s\n", topic_buf);
#endif 

    if(SUCESS == insert_message_to_subtree(topic_buf, ms))
    {
        topic_buf[topic_buf_len + 1] = '/';
        insert_pub_message(tt->next, topic_buf, ms);
        topic_buf[topic_buf_len + 1] = 0;
    }
    topic_buf[topic_buf_len] = 0;
    
    /*3.find #*/
    topic_buf[topic_buf_len] = '#';
    insert_message_to_subtree(topic_buf, ms);
#ifdef DEBUG
    printf("%s\n", topic_buf);
#endif    
    topic_buf[topic_buf_len] = 0;
}

VOID iotbroker_subtree_pub(MessageStore *ms)
{
    TopicPacket *tp;
    struct topic_token tt;
    UINT32 topic_len;
    UINT8 *topic_buf;
    
    assert(ms != NULL);
    
    tp = ms->packet;
    assert(tp != NULL);
    
    get_topic_tokens(tp->topic, &tt);
    
    topic_len = strlen(tp->topic);
    topic_buf = (UINT8*)iotbroker_malloc(topic_len+1); /*1 for '+' or '#'*/
    assert(topic_buf);
    memset(topic_buf, 0, topic_len+1);
    
    insert_pub_message(tt.next, topic_buf, ms);
    
    iotbroker_free(topic_buf);
    free_topic_tokens(&tt);
}

VOID iotbroker_subtree_sub(TopicPacket *tp, Client *client)
{
    TreeNode *tn;
    SubNode *sn;
    
    assert(tp != NULL && client != NULL);

    sn = (SubNode*)iotbroker_malloc(sizeof(SubNode));
    assert(sn != NULL);
    sn->client = client;  
    sn->qos = tp->qos;  
        
    HASH_FIND_STR(g_subtree_head, tp->topic, tn);
    
    if(NULL == tn)
    {
        UINT8 *topic_name;
        UINT16 topic_len;
        SubNode *sn_head;
        
        tn = (TreeNode*)iotbroker_malloc(sizeof(TreeNode));
        assert(tn != NULL);
        
        /*copy topic name*/
        topic_len = strlen(tp->topic);
        topic_name = (UINT8*)iotbroker_malloc(topic_len + 1);
        assert(topic_name != NULL);
        memset(topic_name, 0, topic_len + 1);
        strncpy(topic_name, tp->topic, topic_len);
        tn->topic = topic_name;
        
        sn_head = &tn->sublist;
        INIT_LIST_HEAD(&sn_head->list_mount);
        list_add(&sn->list_mount, &sn_head->list_mount);
        
        HASH_ADD_STR(g_subtree_head, topic, tn);
    }
    else
    {
        SubNode *sn_head = &tn->sublist;
        struct list_head *pos;
        
        list_for_each(pos, &sn_head->list_mount)
        {
            SubNode *tmp = container_of(pos, SubNode, list_mount);
            
            /*simple replace*/
            if(0 == strcmp(tmp->client->client_id, client->client_id))
            {
                iotbroker_free(sn);
                sn = NULL;
                tmp->qos = tp->qos;
                break;
            }
        }
        
        if(pos == &sn_head->list_mount)
        {   
            list_add(&sn->list_mount, &sn_head->list_mount);
        }
    }
#ifdef DEBUG
    display_subtree();
#endif
}

VOID iotbroker_subtree_unsub(TopicPacket *tp, Client *client)
{
    TreeNode *tn;
    SubNode *sub_head;
    struct list_head *pos, *tmp;
    
    assert(tp != NULL);
    
    HASH_FIND_STR(g_subtree_head, tp->topic, tn);
    INVALID_RETURN_NOVALUE(tn != NULL);
    
    sub_head = &tn->sublist;
    list_for_each_safe(pos, tmp, &sub_head->list_mount)
    {
        SubNode *sub_node;
        
        sub_node= container_of(pos, SubNode, list_mount);
        if(0 == strcmp(sub_node->client->client_id, client->client_id))
        {
            list_del(pos);
            iotbroker_free(sub_node);
            sub_node = NULL;
            break;
        }
    }
    
    if(list_empty(&sub_head->list_mount))
    {
        iotbroker_free(tn->topic);
        tn->topic = NULL;
        HASH_DEL(g_subtree_head, tn);
    }
#ifdef DEBUG
    display_subtree();
#endif
}

