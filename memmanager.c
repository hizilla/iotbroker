#include <stdlib.h>
#include <assert.h>

#include "iotbroker.h"
#include "memmanager.h"

/*malloc memory*/
VOID* iotbroker_malloc(size_t size)
{
    VOID *p = malloc(size);
#ifdef DEBUG
    if(size > 1024 * 4)
    {
        printf("=====================================\n");
        printf(" malloc may be error, please check!!!\n");
        printf("=====================================\n");
        assert(0);
    }

    printf("\nmalloc mem as follow:\n");
    printf("=====================================\n");
    printf("     address:%p, size:%ld\n", p, size);
    printf("=====================================\n");
#endif
    return p;
}

/*free memory*/
VOID iotbroker_free(VOID* ptr)
{
#ifdef DEBUG
    printf("\nfree mem as follow:\n");
    printf("=====================================\n");
    printf("     address:%p\n", ptr);
    printf("=====================================\n");
#endif
    free(ptr);
}
