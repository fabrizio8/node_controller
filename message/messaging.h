#ifndef messaging_h
#define messaging_h

#include <stdio.h>
#include <pthread.h>
#include <memory.h>
#include <stdlib.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#define MAX_MESSAGES 2000
#define MSG_INIT_QUEUE_SIZE 100
#define MSG_EMPTY_QUEUE -1

//#define QUEUE_DEBUG

static pthread_mutex_t messagelock = PTHREAD_MUTEX_INITIALIZER;

typedef enum _messages {
    MSG_QUIT = 1, // 1 
    MSG_CHANGE,   // 2
    MSG_FAILURE,  // 3
    MSG_CONTINUE, // 4
    MSG_DRAW,     // 5
    MSG_CUSTOM1,  // 6
    MSG_CUSTOM2,  // 7
    MSG_CUSTOM3,  // 8
    MSG_CUSTOM4,  // 9
    MSG_CUSTOM5,  // 10
    MSG_CUSTOM6,  // 11
    MSG_CUSTOM7,  // 12
    MSG_CUSTOM8,  // 13
    MSG_CUSTOM9,  // 14
    MSG_CUSTOM10  // 15
} MESSAGES;

/* Returned by popmessage() */
typedef enum msg_returns {
    MSG_NOMESSAGE,   // 0
    MSG_MOREMESSAGES,// 1
    MSG_LASTMESSAGE, // 2
    MSG_TIMEOUT      // 3
} MSG_RETURN;

typedef enum msg_errors {
    MSG_QUEUENOTFOUND = -1,
    MSG_OKAY, // 0
    MSG_QUEUEFULL, // 1
    MSG_OUTOFMEMORY // 2
} MSG_ERROR;

typedef struct message {
   unsigned long long message;
    unsigned long long value1;
    unsigned long long value2;    
    union _mptr {
        void *voidptr;
        char *charptr;
        unsigned int *uintptr;
        int *intptr;
        double *doubleptr;
        float *floatptr;
        unsigned long long *lluptr;
        long long *llptr;
        short *shortptr;
        unsigned short *ushortptr;
    } ptr;
    int pointerlength;
    int needtofreepointer;
    pthread_t fromthread;
    const char *fromthreadname;
} MESSAGE;

typedef struct msg_queue {
    pthread_t owningthreadid;
    char semname[20];
    char threadname[100];
    sem_t *messagesignal;
    int queuesize;
    MESSAGE *jmessagelist;
    int lastfulljmessage;
    struct mqueue *next;
    struct mqueue *previous;
} MSG_Q;

static MSG_Q *mqueue = NULL;


/* Creates a messagequeue for the current thread */
int createmessagequeue( char *description );

/* Destroys the message queue on this thread */
int destroymessagequeue();

/* Sends the message in MESSAGE *msg to another thread */
int pushmessage( MESSAGE *msg, pthread_t to_thread_id );

/* Checks for a message waiting for this queue.  If found,
   it populates the message in the MESSAGE * parameter passed.

   Returns:
   --------
   MSG_QUEUENOTFOUND (No call to createmessagequeue()
   MSG_EMPTY_QUEUE   (The message queue was empty)
   MSG_MOREMESSAGES  (You should loop while you get this)
   MSG_LASTMESSAGE   (The last message on the stack)
 */
int popmessage( MESSAGE *msg );

/* A blocking version of popmessage() */
int waitmessage( MESSAGE *msg );

/* Internal Functions, public use not needed */
static int lock();
static int unlock();
static int popmessagenosem( MESSAGE *msg );

#endif /* messaging_h */