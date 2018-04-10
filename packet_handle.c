#include <string.h>
#include <assert.h>

#include "iotbroker.h"
#include "protocol.h"
#include "debug.h"
#include "packet_handle.h"
#include "memmanager.h"
#include "session.h"
#include "message.h"
#include "subtree.h"

STATIC CONST INT8* PROTOCOL_NAME = "MQTT";

STATIC INT32 handle_connect(Client *client, Packet *packet, Packet **out_packet);
STATIC INT32 handle_disconnect(Client *client);
STATIC INT32 handle_publish(Client *client, Packet *packet, Packet **out_packet);
STATIC INT32 handle_pingreq(Packet **out_packet);
STATIC INT32 handle_subscribe(Client *client, Packet *packet, Packet **out_packet);
STATIC INT32 handle_unsubscribe(Client *client, Packet *packet, Packet **out_packet);
STATIC INT32 handle_puback(Client *client, Packet *packet);
STATIC INT32 handle_pubrel(Client *client, Packet *packet, Packet **out_packet);
STATIC INT32 handle_pubrec(Client *client, Packet *packet, Packet **out_packet);
STATIC INT32 handle_pubcomp(Client *client, Packet *packet);

STATIC VOID send_connack(Packet **packet, UINT8 sp, UINT8 con_ret);
STATIC VOID send_pingresp(Packet **out_packet);
STATIC VOID send_puback(Packet **packet, UINT16 packet_id);
STATIC VOID send_pubrec(Packet ** packet, UINT16 packet_id);
STATIC VOID send_pubrel(Packet ** packet, UINT16 packet_id);
STATIC VOID send_pubcomp(Packet **packet, UINT16 packet_id);
STATIC VOID send_suback(Packet **packet, UINT16 packet_id, struct sub_topic_ret *sub_ret_head, UINT16 size);
STATIC VOID send_unsuback(Packet **packet, UINT16 packet_id);

/*read 16 bit data from load data*/
STATIC UINT16 read_uint16(Packet *packet)
{
    UINT16 msb, lsb;
    UINT16 data;
    UINT8 *load = packet->load;
    
    msb = load[packet->load_pos++];
    lsb = load[packet->load_pos++];
    
    data = msb * 256 + lsb;
    
    return data;
}

/*read 8 bit data from load data*/
STATIC UINT8 read_uint8(Packet *packet)
{
    UINT8 data;
    
    data = packet->load[packet->load_pos++];
    
    return data;
}

/*read string from load data*/
STATIC VOID read_str(Packet *packet, UINT8 **dst)
{
    UINT16 len;
    UINT8 *str;
    
    len = read_uint16(packet);
    str = (UINT8*)iotbroker_malloc(len + 1);
    assert(str != NULL);
    memset(str, 0, len + 1);
    strncpy(str, packet->load + packet->load_pos, len);
    packet->load_pos += len;
    
    *dst = str;
}

/*read the remain byte as a string*/
STATIC VOID read_remain_str(Packet *packet, UINT8 **dst)
{
    UINT16 len;
    UINT8 *str;
    
    len = packet->remain_len - packet->load_pos;
    str = (UINT8*)iotbroker_malloc(len + 1);
    assert(str != NULL);
    memset(str, 0, len + 1);
    strncpy(str, packet->load + packet->load_pos, len);
    packet->load_pos += len;
    
    *dst = str;
}

/*write the uint16 data into packet load*/
STATIC VOID write_uint16(Packet *packet, UINT16 data)
{
    UINT8 h = data/256;
    UINT8 l = data%256;
    
    packet->load[packet->load_pos++] = h;
    packet->load[packet->load_pos++] = l;
}

/*write the uint8 data into packet load*/
STATIC VOID write_uint8(Packet *packet, UINT8 data)
{
    packet->load[packet->load_pos++] = data;
}

STATIC VOID write_str(Packet *packet, UINT8 *str)
{
    UINT16 len;
    
    len = strlen(str);
    write_uint16(packet, len);
    
    memcpy(packet->load + packet->load_pos, str, len);
    packet->load_pos += len; 
}

STATIC VOID write_remain_str(Packet *packet, UINT8 *str)
{
    UINT16 len;
    
    len = strlen(str);
    memcpy(packet->load + packet->load_pos, str, len);
    packet->load_pos += len; 
}

STATIC INT32 handle_connect(Client *client, Packet *packet, Packet **out_packet)
{
    UINT16 protocol_name_len, client_id_len;
    UINT8 *protocol_name = NULL;
    UINT8 *client_id = NULL;
    INT8 protocol_level;
    UINT8 connect_flags;
    UINT16 keepalive;
    UINT8 *will_topic = NULL, *will_msg = NULL;
    UINT8 *username = NULL, *password = NULL;
    UINT8 session_present, connection_ret;
        
    read_str(packet, &protocol_name);
    
#ifdef DEBUG
    printf("\n%s %d \n\
        \tprotocol name: %s\n\
        \tcurrent load pos: %d\n",
         __FILE__, __LINE__, protocol_name, packet->load_pos);
#endif
    
    /*check protocol name*/
    if(strlen(protocol_name) > PROTOCOL_NAME_LEN || strcmp(protocol_name, PROTOCOL_NAME) != 0)
    {
        printf("invalid protocol name!\n");
        iotbroker_free(protocol_name);
        protocol_name = NULL;
        return HANDLE_RET_CLOSE_CLIENT;
    }
    iotbroker_free(protocol_name);
    protocol_name = NULL;
    
    /*check protocol level*/
    protocol_level = read_uint8(packet);
#ifdef DEBUG
    printf("\n%s %d\n\
    \tprotocol level: %d\n",
    __FILE__, __LINE__, protocol_level);
#endif 

    if(protocol_level > PROTOCOL_MAX_LEVEL)
    {
        printf("invalid protocol level!\n");
        connection_ret = CONNECT_RET_INVALID_PROTOCOL_LEVEL;
        goto handle_connect_ack;
    }
    
    /*connect flags*/
    connect_flags = read_uint8(packet);
    if(CONNECT_FLAG_RESERVED & connect_flags)
    {
        printf("invalid control flags in pos 0!\n");
        return HANDLE_RET_CLOSE_CLIENT;
    }
    
    /*keep alive time*/
    keepalive = read_uint16(packet);
#ifdef DEBUG
    printf("\n%s %d\n\
    \tkeepalive: %d\n",
    __FILE__, __LINE__, keepalive);
#endif    
    
    /*client id*/
    read_str(packet, &client_id);
#ifdef DEBUG
    printf("\n%s %d \n\
        \tclient id : %s\n\
        \tcurrent load pos: %d\n",
         __FILE__, __LINE__, client_id, packet->load_pos);
#endif   

    /*will*/
    if(connect_flags & CONNECT_FLAG_WILL_FLAG)
    {
        read_str(packet, &will_topic);
        read_str(packet, &will_msg);
#ifdef DEBUG
        printf("\n%s %d \n\
        \twill topic : %s\n\
        \twill msg : %s\n",
         __FILE__, __LINE__, will_topic, will_msg);
#endif
    }
    
    /*username*/
    if(connect_flags & CONNECT_FLAG_USERNAME)
    {
        read_str(packet, &username);
#ifdef DEBUG
        printf("\n%s %d \n\
        \tusername: %s\n",
         __FILE__, __LINE__, username);
#endif
    }
    
    /*password*/
    if(connect_flags & CONNECT_FLAG_USERNAME)
    {
        read_str(packet, &password);
#ifdef DEBUG
        printf("\n%s %d \n\
        \tpassword: %s\n",
         __FILE__, __LINE__, password);
#endif
    }
    
    if(client_id != NULL)
    {
        iotbroker_session_setid(client->sock_fd, client_id);
    }
    
    if(username != NULL || password != NULL)
    {
        iotbroker_session_auth(client->sock_fd, username, password);
    }

	/*TODO:check auth*/
    connection_ret = CONNECT_RET_OK;
    session_present = FALSE;
    
    iotbroker_session_state_mod(client->sock_fd, CS_CONNECTING);
    
handle_connect_ack:
    send_connack(out_packet, session_present, connection_ret);
    
    return SUCESS;
}

STATIC VOID send_connack(Packet **packet, UINT8 sp, UINT8 con_ret)
{   
    Packet *p;
    INT8 *load;
    
    p = (Packet*)iotbroker_malloc(sizeof(Packet));
    assert(p != NULL);
    memset(p, 0, sizeof(Packet));
    
    p->type = CONNACK;
    p->remain_len = 2;
    
    load = (INT8*)iotbroker_malloc(p->remain_len);
    assert(load != NULL);
    load[p->load_pos++] = sp & 0x01;
    load[p->load_pos++] = con_ret;
    
    p->load = load;
    
    *packet = p;
}

STATIC INT32 handle_disconnect(Client *client)
{
#ifdef DEBUG
    printf("\n%s %d \n\
        \tdisconnect\n",
         __FILE__, __LINE__);
#endif     
    return HANDLE_RET_CLOSE_CLIENT;
}

STATIC INT32 handle_publish(Client *client, Packet *packet, Packet **out_packet)
{
    UINT8 dup, qos, retain;
    UINT8 *topic_name, *topic_content;
    TopicPacket *tp;
    MessageStore *ms;
    UINT16 packet_id = 0;
    
    dup = (packet->flags & PUBLISH_FLAG_DUP) >> 3;
    qos = (packet->flags & PUBLISH_FLAG_QOS) >> 1;
    retain = packet->flags & PUBLISH_FLAG_RETAIN;
    
    /*check qos*/
    if(qos > QOS2)
    {
        printf("invalid qos!\n");
        return HANDLE_RET_CLOSE_CLIENT;
    }
    
    read_str(packet, &topic_name);
    
    if(qos == QOS1 || qos == QOS2)
    {
        packet_id = read_uint16(packet);
    }
    
    read_remain_str(packet, &topic_content);
    
#ifdef DEBUG
        printf("\n%s %d \n\
        \tdup: %d\n\
        \tqos: %d\n\
        \tretain: %d\n\
        \ttopic name: %s\n\
        \tpacket id: %d\n\
        \ttopic content: %s\n",
         __FILE__, __LINE__, dup, qos, retain, topic_name, packet_id, topic_content);
#endif
    
    tp = (TopicPacket*)iotbroker_malloc(sizeof(TopicPacket));
    assert(tp != NULL);

    tp->packet_id = packet_id;
    tp->topic = topic_name;
    tp->content = topic_content;
    tp->dup = dup;
    tp->qos = qos;
    tp->retain = retain;
    
    iotbroker_message_store_insert(tp, &ms);
    
    if(QOS0 == qos)
    {
        /*qos0, insert into subtree*/
        iotbroker_subtree_pub(ms);
    }
    else if(QOS1 == qos)
    {
        /*qos1: send puback*/
        send_puback(out_packet, packet_id);
        
        /*insert into subtree*/
        iotbroker_subtree_pub(ms);
    }  
    else if(QOS2 == qos)
    {
        MessageQueue *mq, *new_msg;
        
        /*qos2: send pubrec and add the msg into msg wait queue*/
        new_msg = (MessageQueue*)iotbroker_malloc(sizeof(MessageQueue));
        assert(new_msg != NULL);
        new_msg->qs = QS_INFLIGHT;
        new_msg->ps = PS_WAIT_FOR_PUBREL;
        new_msg->dir = MD_IN;
        new_msg->resend_count = 0;
        new_msg->ms = ms;
        new_msg->qos = qos;
        new_msg->packet_id = packet_id;
        
        mq = client->mq_head;
        list_add(&new_msg->list_mount, &mq->list_mount);
        
        ms->refer_count++;
        
        send_pubrec(out_packet, packet_id);
    }  
    
    return SUCESS;
}

STATIC INT32 handle_pubrec(Client *client, Packet *packet, Packet **out_packet)
{
    UINT16 packet_id;
    MessageQueue *mq_head;
    struct list_head *pos, *tmp;
    
    packet_id = read_uint16(packet);
    mq_head = client->mq_head;
    
    list_for_each_safe(pos, tmp, &mq_head->list_mount)
    {
        MessageQueue *mq = container_of(pos, MessageQueue, list_mount);
        
        if(mq->packet_id == packet_id && mq->ps == PS_WAIT_FOR_PUBREC)
        {
            mq->dir = MD_IN;
            mq->ps = PS_WAIT_FOR_PUBCOMP;
            send_pubrel(out_packet, packet_id);
            break;
        }
    }
    
    return SUCESS; 
}

STATIC INT32 handle_pubcomp(Client *client, Packet *packet)
{
    UINT16 packet_id;
    MessageQueue *mq_head;
    struct list_head *pos, *tmp;
    
    packet_id = read_uint16(packet);
    mq_head = client->mq_head;
    
    list_for_each_safe(pos, tmp, &mq_head->list_mount)
    {
        MessageQueue *mq = container_of(pos, MessageQueue, list_mount);
        
        if(mq->packet_id == packet_id && mq->ps == PS_WAIT_FOR_PUBCOMP)
        {
            iotbroker_message_store_deref(mq->ms);
            list_del(&mq->list_mount);
            break;
        }
    }
    
    return SUCESS; 
}

STATIC INT32 handle_pubrel(Client *client, Packet *packet, Packet **out_packet)
{
    UINT16 packet_id;
    MessageQueue *mq_head;
    struct list_head *pos, *tmp;
    
    packet_id = read_uint16(packet);
    mq_head = client->mq_head;
    
    list_for_each_safe(pos, tmp, &mq_head->list_mount)
    {
        MessageQueue *mq = container_of(pos, MessageQueue, list_mount);
        
        if(mq->packet_id == packet_id && mq->ps == PS_WAIT_FOR_PUBREL)
        {
            iotbroker_subtree_pub(mq->ms);
            iotbroker_message_store_deref(mq->ms);
            list_del(&mq->list_mount);
            send_pubcomp(out_packet, packet_id);
            break;
        }
    }
    
    return SUCESS;    
}

STATIC INT32 handle_subscribe(Client *client, Packet *packet, Packet **out_packet)
{
    UINT16 packet_id, topic_counter = 0;
    struct sub_topic_ret *sub_ret, *tmp_sub_ret;
    INT32 ret = SUCESS;
    
    packet_id = read_uint16(packet);
    
    sub_ret = (struct sub_topic_ret*)iotbroker_malloc(sizeof(struct sub_topic_ret));
    assert(sub_ret != NULL);
    tmp_sub_ret = sub_ret;
    
    /*read all topics in load*/
    while(packet->load_pos < packet->remain_len)
    {
        UINT8 *topic;
        UINT8 qos;
        TopicPacket *tp;
        struct sub_topic_ret *sub_ret_node;
        
        read_str(packet, &topic);
        qos = read_uint8(packet);
#ifdef DEBUG
        printf("\n%s %d \n\
        \ttopic: %s\n\
        \tqos: %d\n",
         __FILE__, __LINE__, topic, qos);
#endif   
        if(qos > QOS2)
        {
            ret = HANDLE_RET_CLOSE_CLIENT;
            goto handle_error;
        }
        
        tp = (TopicPacket*)iotbroker_malloc(sizeof(TopicPacket));
        assert(tp != NULL);
        tp->topic = topic;
        tp->qos = qos;
        
        iotbroker_subtree_sub(tp, client);
        
        iotbroker_free(topic);
        topic = NULL;
        
        iotbroker_free(tp);
        tp = NULL;
        
        sub_ret_node = (struct sub_topic_ret*)iotbroker_malloc(sizeof(struct sub_topic_ret));
        assert(sub_ret_node != NULL);
        
        sub_ret_node->ret = qos;
        sub_ret_node->next = NULL;
        
        tmp_sub_ret->next = sub_ret_node;
        tmp_sub_ret = sub_ret_node;
        
        topic_counter++;
    }
    
    send_suback(out_packet, packet_id, sub_ret, topic_counter);

handle_error:
    
    tmp_sub_ret = sub_ret;
    while(tmp_sub_ret != NULL)
    {
        struct sub_topic_ret *tmp = tmp_sub_ret->next;
        iotbroker_free(tmp_sub_ret);
        tmp_sub_ret = NULL;
        tmp_sub_ret = tmp;
    }
    return ret;
}

STATIC INT32 handle_unsubscribe(Client *client, Packet *packet, Packet **out_packet)
{
    UINT16 packet_id;

    packet_id = read_uint16(packet);
    
    /*read all topics in load*/
    while(packet->load_pos < packet->remain_len)
    {
        UINT8 *topic;
        TopicPacket *tp;
        
        read_str(packet, &topic);
#ifdef DEBUG
        printf("\n%s %d \n\
        \ttopic: %s\n",
         __FILE__, __LINE__, topic);
#endif   
        tp = (TopicPacket*)iotbroker_malloc(sizeof(TopicPacket));
        assert(tp != NULL);
        tp->topic = topic;
        
        iotbroker_subtree_unsub(tp, client);
        
        iotbroker_free(topic);
        topic = NULL;
        
        iotbroker_free(tp);
        tp = NULL;
    }
    
    send_unsuback(out_packet, packet_id);
    
    return SUCESS;
}

STATIC INT32 handle_pingreq(Packet **out_packet)
{
    send_pingresp(out_packet);
    
    return SUCESS;
}

STATIC INT32 handle_puback(Client *client, Packet *packet)
{
    UINT16 packet_id;
    MessageQueue *mq_head;
    struct list_head *pos, *tmp;
    
    packet_id = read_uint16(packet);
    mq_head = client->mq_head;
    
    list_for_each_safe(pos, tmp, &mq_head->list_mount)
    {
        MessageQueue *mq = container_of(pos, MessageQueue, list_mount);
        
        if(mq->packet_id == packet_id && mq->ps == PS_WAIT_FOR_PUBACK)
        {
            iotbroker_message_store_deref(mq->ms);
            list_del(&mq->list_mount);
            break;
        }
    }
    
    return SUCESS;
}

STATIC VOID build_packet_with_packetid(Packet **out_packet, UINT16 packet_id, UINT8 type)
{
    Packet *p;
    INT8 *load;
    
    p = (Packet*)iotbroker_malloc(sizeof(Packet));
    assert(p != NULL);
    memset(p, 0, sizeof(Packet));
    
    /*type*/
    p->type = type;
    
    /*remain length*/
    p->remain_len = 2; 
    
    /*load*/
    load = (INT8*)iotbroker_malloc(p->remain_len);
    assert(load != NULL);
    p->load = load;
    
    write_uint16(p, packet_id);

#ifdef DEBUG
        printf("\n%s %d \n\
        \tpacket id: %d\n",
         __FILE__, __LINE__, packet_id);
#endif   
    
    *out_packet = p;
}

STATIC VOID send_pingresp(Packet **out_packet)
{
    Packet *p;
    
    p = (Packet*)iotbroker_malloc(sizeof(Packet));
    assert(p != NULL);
    memset(p, 0, sizeof(Packet));
    
    p->type = PINGRESP;
    p->remain_len = 0;
    
    *out_packet = p;
}

STATIC VOID send_puback(Packet **packet, UINT16 packet_id)
{
    build_packet_with_packetid(packet, packet_id, PUBACK);
}

STATIC VOID send_pubrec(Packet ** packet, UINT16 packet_id)
{
    build_packet_with_packetid(packet, packet_id, PUBREC);
}

STATIC VOID send_pubrel(Packet ** packet, UINT16 packet_id)
{
    build_packet_with_packetid(packet, packet_id, PUBREL);
}

STATIC VOID send_pubcomp(Packet **packet, UINT16 packet_id)
{
    build_packet_with_packetid(packet, packet_id, PUBCOMP);
}

STATIC VOID send_suback(Packet **packet, UINT16 packet_id, struct sub_topic_ret *sub_ret_head, UINT16 size)
{
    Packet *p;
    INT8 *load;
    struct sub_topic_ret *tmp_sub_ret;
    
    p = (Packet*)iotbroker_malloc(sizeof(Packet));
    assert(p != NULL);
    memset(p, 0, sizeof(Packet));
    
    p->type = SUBACK;
    p->remain_len = 2 + size;
    
    load = (INT8*)iotbroker_malloc(p->remain_len);
    assert(load != NULL);
    p->load = load;
    
    write_uint16(p, packet_id);
    
    tmp_sub_ret = sub_ret_head->next;
    while(tmp_sub_ret != NULL)
    {
        UINT8 ret = tmp_sub_ret->ret;
        write_uint8(p, ret);
        tmp_sub_ret = tmp_sub_ret->next;
    }
    
    *packet = p;
}

STATIC VOID send_unsuback(Packet **packet, UINT16 packet_id)
{
    build_packet_with_packetid(packet, packet_id, UNSUBACK);
}

STATIC INT32 send_publish(Client *client, MessageQueue *mq, Packet **out_packet)
{
    Packet *p;
    MessageStore *ms;
    TopicPacket *tp;
    UINT8 *load;
    INT32 ret = HANDLE_RET_REMOVE_MSG;
    
    ms = mq->ms;
    tp = ms->packet;
    
    p = (Packet*)iotbroker_malloc(sizeof(Packet));
    assert(p != NULL);
    memset(p, 0, sizeof(Packet));
    
    p->type = PUBLISH;
    p->flags = mq->qos << 1;
    p->remain_len = 2  + strlen(tp->topic) + strlen(tp->content);
    
    if(QOS1 == mq->qos || QOS2 == mq->qos)
    {
        p->remain_len += 2;
    }
    
    load = (UINT8*)iotbroker_malloc(p->remain_len);
    assert(load != NULL);
    p->load = load;

    write_str(p, tp->topic);    
    if(QOS1 == mq->qos || QOS2 == mq->qos)
    {
        mq->packet_id = client->packet_id_source;
        write_uint16(p, client->packet_id_source++);
    }
    write_remain_str(p, tp->content);
    
    if(QOS0 == mq->qos)
    {
        /*the routine end*/
        iotbroker_message_store_deref(ms);
    }
    else if(QOS1 == mq->qos)
    {   
        /*wait for puback*/
        mq->ps = PS_WAIT_FOR_PUBACK;
        mq->dir = MD_IN;
        ret = HANDLE_RET_KEEP_MSG;        
    }
    else if(QOS2 == mq->qos)
    {
        /*wait for pubrec*/
        mq->ps = PS_WAIT_FOR_PUBREC;
        mq->dir = MD_IN;
        ret = HANDLE_RET_KEEP_MSG;
    }
    
    *out_packet = p;
    
    return ret;
}

INT32 handle_message_queue(Client *client, MessageQueue *mq, Packet **out_packet)
{
    INT32 ret = SUCESS;    
    
    assert(client != NULL && mq != NULL && out_packet != NULL);
    
    switch(mq->ps)
    {
        case PS_WAIT_TO_PUBLISH:
            ret = send_publish(client, mq, out_packet);
            break;

        default:
            break;
    }        
    
    return ret;
}

INT32 handle_packet(Client *client, Packet *packet, Packet **out_packet)
{
    INT32 ret = SUCESS;    
    
    assert(packet != NULL);
    assert(out_packet != NULL);
    
    switch(packet->type)
    {
        case CONNECT:
            ret = handle_connect(client, packet, out_packet);
            break;
            
        case PUBLISH:
            ret = handle_publish(client, packet, out_packet);
            break;
            
        case PUBACK:
            ret = handle_puback(client, packet);
            break;
            
        case PUBREC:
            ret = handle_pubrec(client, packet, out_packet);
            break;
            
        case PUBREL:
            ret = handle_pubrel(client, packet, out_packet);
            break;
            
        case PUBCOMP:
            ret = handle_pubcomp(client, packet);
            break;
            
        case SUBSCRIBE:
            ret = handle_subscribe(client, packet, out_packet);
            break;
            
        case UNSUBSCRIBE:
            ret = handle_unsubscribe(client, packet, out_packet);
            break;
            
        case PINGREQ:
            ret = handle_pingreq(out_packet);
            break;
            
        case DISCONNECT:
            ret = handle_disconnect(client);
            break;
           
        default:
            break;
    }
    
    return ret;
}
