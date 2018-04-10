#ifndef _DEBUG_H_
#define _DEBUG_H_

#include "iotbroker.h"

#define UINT8_LEN 8

#define INVALID_RETURN_NOVALUE(exp) \
    {\
        if(!(exp))return;\
    }while(0)
    
#define INVALID_RETURN_VALUE(exp, value) \
    {\
        if(!(exp))return (value);\
    }while(0)

VOID print_hex2num(INT8 *data, UINT32 len);

#endif
