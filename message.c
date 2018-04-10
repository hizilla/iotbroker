#include <assert.h>

#include "protocol.h"
#include "message.h"
#include "memmanager.h"
#include "list.h"

STATIC MessageStore *g_message_store_head;

STATIC VOID display_message_store()
{
    struct list_head *pos;
    
    printf("\nmessage store table as follow:\n");
    printf("=====================================\n");
    
    list_for_each(pos, &g_message_store_head->list_mount)
    {
        TopicPacket *tp;
        MessageStore *ms = container_of(pos, MessageStore, list_mount);
        
        tp = ms->packet;
        
        printf("%s: %s | %d\n", tp->topic, tp->content, ms->refer_count);
    }
    printf("=====================================\n");
}

VOID iotbroker_message_store_init()
{
    g_message_store_head = (MessageStore*)iotbroker_malloc(sizeof(MessageStore));
    assert(g_message_store_head != NULL);
    
    INIT_LIST_HEAD(&g_message_store_head->list_mount);
}

VOID iotbroker_message_store_insert(TopicPacket *tp, MessageStore **ms)
{
    MessageStore *tmp_ms;
    
    assert(tp != NULL && ms != NULL);
    
    tmp_ms = (MessageStore*)iotbroker_malloc(sizeof(MessageStore));
    assert(tmp_ms != NULL);
    tmp_ms->refer_count = 0;
    tmp_ms->packet = tp;
    
    list_add(&tmp_ms->list_mount, &g_message_store_head->list_mount);
    *ms = tmp_ms;

#ifdef DEBUG
    display_message_store();
#endif
}

VOID iotbroker_message_store_deref(MessageStore *ms)
{
    assert(ms != NULL);
    
    ms->refer_count--;
    
    /*no refrence, delete it*/
    if(ms->refer_count == 0)
    {
        if(ms->packet != NULL)
        {
            TopicPacket *tp = ms->packet;
            
            if(tp->topic != NULL)
            {
                iotbroker_free(tp->topic);
            }
            
            if(tp->content != NULL)
            {
                iotbroker_free(tp->content);
            }
            
            iotbroker_free(tp);
        }
        
        list_del(&ms->list_mount);
        iotbroker_free(ms);
    }
#ifdef DEBUG
    display_message_store();
#endif
}
