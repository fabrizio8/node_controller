typedef enum jmsg_errors {
    JMSG_QUEUENOTFOUND = -1,
    JMSG_OKAY, // 0
    JMSG_QUEUEFULL, // 1
    JMSG_OUTOFMEMORY // 2
} JMSG_ERROR;

typedef struct jmessage {
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
} JMESSAGE;

typedef struct jqueue {
    pthread_t owningthreadid;
    char semname[20];
    char threadname[100];
    sem_t *messagesignal;
    int queuesize;
    JMESSAGE *jmessagelist;
    int lastfulljmessage;
    struct jqueue *next;
    struct jqueue *previous;
} JQUEUE;

static JQUEUE *jqueue = 0;


/* Creates a messagequeue for the current thread */
int createmessagequeue( char *description );

/* Destroys the message queue on this thread */
int destroymessagequeue();

/* Sends the message in JMESSAGE *jmsg to another thread */
int pushmessage( JMESSAGE *jmsg, pthread_t to_thread_id );

/* Checks for a message waiting for this queue.  If found,
   it populates the message in the JMESSAGE * parameter passed.

   Returns:
   --------
   JMSG_QUEUENOTFOUND (No call to createmessagequeue()
   JMSG_EMPTY_QUEUE   (The message queue was empty)
   JMSG_MOREMESSAGES  (You should loop while you get this)
   JMSG_LASTMESSAGE   (The last message on the stack)
 */
int popmessage( JMESSAGE *jmsg );

/* A blocking version of popmessage() */
int waitmessage( JMESSAGE *jmsg );

/* Internal Functions, public use not needed */
static int lockit();
static int unlockit();
static int popmessagenosem( JMESSAGE *jmsg );

#endif /* messaging_h */