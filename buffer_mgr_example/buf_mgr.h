/*    buf_mgr.h  a support for remote buffer manager      */

#ifndef __bmgr_h
#define __bmgr_h

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>


#define PORT      27439 // use a value from 1001 - (64K-1)

#define JELLY     0
#define PLAIN     1
#define COCO      2
#define CREAM     3

#define PRO_JELLY 0
#define PRO_PLAIN 1
#define PRO_COCO  2
#define PRO_CREAM 3

#define CON_JELLY 10
#define CON_PLAIN 11
#define CON_COCO  12
#define CON_CREAM 13

#define REQUEST   21
#define REPLY     22
#define RELEASE   23

#define BUF_MGR   100
#define P_ACK     101
#define C_ACK     102
#define M_ERR     103

#define CONNECTED 200
#define CONN_ACK  210

#define NUM_NODES 3


typedef  struct {
    int  mtype;      // what operation ?
    int  mid;        // what producer-ID
    int  mdonut_num; // what donut number for this producer
    int  mnode_id;   // what node-ID producer lives on
} MSG;

typedef union{
    MSG m;                  // structured message
    char buf[sizeof(MSG)];  // message as a raw byte buffer
} MBUF;

// typedef struct msgbuf {
//     long    mtype;
//     char    mtext[MSGSZ];
// } message_buf;

extern int errno;

#endif /* #ifndef __bmgr_h */

