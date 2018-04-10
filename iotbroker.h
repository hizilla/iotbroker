#ifndef _IOTBROKER_H_
#define _IOTBROKER_H_

#include <stdio.h>
#include <unistd.h>

/********************************define ****************************************/
#define TRUE 1

#define FALSE (!TRUE)

#define SUCESS 0

#define FAILED (!SUCESS)

#define STATIC static

#define CONST const

#define DEBUG

#define MIN(a,b) (a)<(b)?(a):(b)

/********************************typedef****************************************/
typedef void VOID;

/*typedef char*/
typedef unsigned char UINT8;
typedef char INT8;

/*typedef short*/
typedef unsigned short int UINT16;
typedef short int INT16;

/*typedef int*/
typedef unsigned int UINT32;
typedef int INT32;

/*typedef long*/
typedef unsigned long ULONG;
typedef long LONG;

/*typedef double*/
typedef double DOUBLE;

/*typedef long long*/
typedef long long U64;

#endif
