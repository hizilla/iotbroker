#include <stdio.h>
#include <assert.h>

#include "debug.h"
#include "iotbroker.h" 

VOID print_hex2num(INT8 *data, UINT32 len)
{
    UINT32 i, j;
    
    assert(data != NULL);
    assert(len > 0);
    
    printf("\nmessage content as follow:\n");
    printf("=====================================\n");
    
    for(i = 0; i < len; i++)
    {
        UINT8 tmp = (UINT8)data[i];
        
        printf("%5d: %3d   ", i, tmp);
        
        for(j = 0; j < UINT8_LEN; j++)
        {
            printf("%d ", (tmp >> (UINT8_LEN - j -1)) & 0x01);
            
            /*make a blank*/
            if(UINT8_LEN == 2 * (j + 1))
            {
                printf(" ");            
            }
        }
        printf("\n");
    }
    printf("=====================================\n");
}

