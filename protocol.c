#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "iotbroker.h"
#include "memmanager.h"
#include "protocol.h"
#include "debug.h"
#include "packet_handle.h"
#include "list.h"
#include "session.h"
#include "message.h"

CONST INT8 *g_control_type_str[] = {
    "INVALID",
    "CONNECT",
    "CONNACK",
    "PUBLISH",
    "PUBACK",
    "PUBREC",
    "PUBREL",
    "PUBCOMP",
    "SUBSCRIBE",
    "SUBACK",
    "UNSUBSCRIBE",
    "UNSUBACK",
    "PINGREQ",
    "PINGRESP",
    "DISCONNECT",
    "MAX_CONTROL_TYPE",
};

/*get the packet type*/
STATIC INT32 get_packet_type(INT8 buf, Packet *packet)
{
    INT8 packet_type;
    
    packet_type = (buf >> 4) & 0x0F;
    if(packet_type <= MIN_CONTROL_TYPE || packet_type >= MAX_CONTROL_TYPE)
    {
        return FAILED;
    }
    
    packet->type = packet_type;
    
    return SUCESS;
}

/*get the packet flag*/
STATIC VOID get_packet_flag(INT8 buf, Packet *packet)
{
    INT8 packet_flags;
    
    packet_flags = (buf & 0x0F);
    
    packet->current_pos++;
    packet->flags = packet_flags;
}

/*set the remain length to buffer*/
STATIC VOID set_packet_remainlength(INT8 *buf, Packet *packet)
{
    UINT32 remain_len = packet->remain_len;
    
    do
    {
        UINT8 encoded_byte = remain_len % 128;
        remain_len /= 128;
        
        if(remain_len > 0)
        {
            encoded_byte |= 128;
        } 
        
        buf[packet->current_pos++] = encoded_byte;     
    }while(remain_len > 0);
}

/*convert packet to buf*/
STATIC VOID write_packet(Packet *packet, INT8 **buf, INT32 *buf_len)
{
    INT8 *tmp_buf;
    
    tmp_buf = (INT8*)iotbroker_malloc(5 + packet->remain_len);
    assert(tmp_buf != NULL);
    memset(tmp_buf, 0, 5 + packet->remain_len);
    
    packet->current_pos = 0;
    
    /*type and flags*/
    tmp_buf[packet->current_pos++] = packet->type << 4 | packet->flags;
    
    /*remain length*/
    set_packet_remainlength(tmp_buf, packet);
    
    /*buffer length*/
    *buf_len = packet->current_pos + packet->load_pos;

#ifdef DEBUG
    printf("\n%s:%d\n\
        \ttype: %d\n\
        \tflags: %d\n\
        \tremain length: %d\n",__FILE__, __LINE__, packet->type, packet->flags,packet->remain_len);    
#endif

#ifdef DEBUG    
    printf("\n%s:%d\n\
        \tbuf_len: %d = %d(current_pos) + %d(load_pos)\n", __FILE__, __LINE__,
            *buf_len, packet->current_pos, packet->load_pos);
#endif 
   
    memcpy(tmp_buf + packet->current_pos, packet->load, packet->load_pos);
    
    if(packet->load != NULL)
    {
        iotbroker_free(packet->load);
    }
    
    if(packet != NULL)
    {
        iotbroker_free(packet);
    }
    
    *buf = tmp_buf;
}

INT32 iotbroker_read_packet(UINT32 sock_fd)
{
    UINT8 byte_type_flags;
    UINT32 multiplier = 1, remainlen_value = 0;
    UINT8 remain_start, remain_counter = 0;
    INT32 ret;
    UINT8 *load = NULL;
    Packet *packet = NULL, *out_packet = NULL;
    INT8 *write_buf;
    INT32 write_buf_len;
    Client *client = NULL;

    iotbroker_session_get(sock_fd, &client);
    if(NULL == client)
    {
        return ERROR_SOCK_CLIENT_NOEXIST;
    }
    
    /*read first byte as type and flags*/
    ret = read(sock_fd, &byte_type_flags, 1);
    if(ret <= 0)
    {
        goto handle_read_error;
    }
    
    /*read remain length, invalid when bytes more than MAX_REMAIN_BYTE_LEN*/
    do
    {
        ret = read(sock_fd, &remain_start, 1);
        if(ret <= 0)
        {
            goto handle_read_error;
        } 
        remain_counter++;
             
        remainlen_value += (remain_start & 127) * multiplier;
        multiplier *= 128;
        if(multiplier > 128*128*128)
        {
            goto handle_read_error;
        }
    }while((UINT8)(remain_start & 128) != 0);
    
    /*read load*/
    if(remainlen_value != 0)
    {
        load = (UINT8*)iotbroker_malloc(remainlen_value);
        assert(load != NULL);
        memset(load, 0, remainlen_value);
        ret = read(sock_fd, load, remainlen_value);
        if(ret <= 0)
        {
            goto handle_read_error;
        }
    }
    
    packet = (Packet*)iotbroker_malloc(sizeof(Packet));
    assert(packet != NULL);
    memset(packet, 0, sizeof(Packet));
    
    ret = get_packet_type(byte_type_flags, packet);
    if(ret != SUCESS)
    {
        goto handle_read_error;
    }
    
    get_packet_flag(byte_type_flags, packet);
    if(ret != SUCESS)
    {
        goto handle_read_error;
    }
    
    packet->current_pos += remain_counter;
    packet->remain_len = remainlen_value;
    packet->load = load;
        
#ifdef DEBUG
    printf("\n%s %d \n\
        \t type: %s\n\
        \t flags: %d\n\
        \t remain length: %d\n\
        \t load address: %p\n\
        \t current read pos: %d\n",
        __FILE__, __LINE__, g_control_type_str[packet->type], packet->flags, packet->remain_len, 
        packet->load, packet->current_pos);
        
    if(packet->remain_len > 0)
    {
        print_hex2num(packet->load, packet->remain_len);
    }
#endif    
    
    out_packet = NULL;
    if(handle_packet(client, packet, &out_packet) != SUCESS)
    {
        goto handle_read_error;
    }
    INVALID_RETURN_VALUE(out_packet != NULL, SUCESS);
    
    write_packet(out_packet, &write_buf, &write_buf_len);
    ret = write(sock_fd, write_buf,  write_buf_len);
    
#ifdef DEBUG
    print_hex2num(write_buf, write_buf_len);
#endif   
 
    iotbroker_free(write_buf);
    write_buf = NULL;
    
    if(ret <= 0)
    {
        goto handle_read_error;
    }
    
    if(load != NULL)
    {
        iotbroker_free(load);
        load = NULL;
    }
    
    if(packet != NULL)
    {
        iotbroker_free(packet);
        packet = NULL;
    }
    
    return SUCESS;
    
handle_read_error:
    
    if(load != NULL)
    {
        iotbroker_free(load);
        load = NULL;
    }
    
    if(packet != NULL)
    {
        iotbroker_free(packet);
        packet = NULL;
    }
    
    if(0 == ret)
    {
        return ERROR_SOCK_CLIENT_CLOSE;
    }
    else if(-1 == ret)
    {
        return ERROR_SOCK_READ_WRITE;
    }
    else
    {
        return ERROR_SOCK_PACKET_ERROR;
    }
}

/*write packet to buffer*/
INT32 iotbroker_write_packet(UINT32 sock_fd)
{
    Client *client = NULL;
    struct list_head *pos;
    MessageQueue *mq_head;
    Packet *packet;
    INT32 ret = SUCESS;
    
    iotbroker_session_get(sock_fd, &client);
    if(NULL == client)
    {
        return ERROR_SOCK_CLIENT_NOEXIST;
    }
    
    mq_head = client->mq_head;
    list_for_each(pos, &mq_head->list_mount)
    {
        INT8 *out_buf;
        INT32 write_buf_len;
        INT32 ret;
        
        MessageQueue *mq = container_of(pos, MessageQueue, list_mount);
        
        if(MD_IN == mq->dir)
        {
            continue;
        }
        
        if(HANDLE_RET_REMOVE_MSG == handle_message_queue(client, mq, &packet))
        {
            list_del(pos);         
        }
        
        write_packet(packet, &out_buf, &write_buf_len);
#ifdef DEBUG
        print_hex2num(out_buf, write_buf_len);
#endif            
        ret = write(sock_fd, out_buf, write_buf_len);
            
        iotbroker_free(out_buf);
        out_buf = NULL;
            
        if(ret <= 0)
        {
            goto handle_write_error;
        }  
    }
    
    return SUCESS;
    
handle_write_error:

    if(0 == ret)
    {
        return ERROR_SOCK_CLIENT_CLOSE;
    }
    else if(-1 == ret)
    {
        return ERROR_SOCK_READ_WRITE;
    }
    else
    {
        return ERROR_SOCK_PACKET_ERROR;
    }
}
