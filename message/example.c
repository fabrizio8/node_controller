#include <signal.h>
#include "messaging.h"

#define THREAD_STARTED      MSG_CUSTOM1
#define THREAD_FAILED       MSG_CUSTOM2
#define THREAD_EXITING      MSG_CUSTOM3
#define THREAD_ACKNOWLEDGE  MSG_CUSTOM4

void sighand( int signum ) {
    MESSAGE msg;
    printf("\nSignal Received! Exiting...\n");
    msg.message = MSG_QUIT;

/* Put this MSG_QUIT message on the main thread's message queue 
* to let the main code know to quit.*/
    pushmessage(&msg, pthread_self());
    return;
}

void *mythread( void *_mainthreadid ) {
    pthread_t mainthreadid = (pthread_t)_mainthreadid;
    MESSAGE msg;

    signal(SIGINT, &sighand); /* CTRL-C */

    if (!createmessagequeue("mythread")) {
        printf("main.c > mythread() > createmessagequeue(): Failed.\n");
        return 0;
    }

    /* 
     * Send a message to the main thread so it can do something when it 
     * knows we're ready.
     */
    msg.message = THREAD_ACKNOWLEDGE;
    pushmessage(&msg, mainthreadid);

    printf("main.c > mythread(): Launched successfully, using blocking message loop!\n");
    do {
        waitmessage(&msg); /* 
                            * Wait indefinitely.  You can, however, use a 
                            * signal to send a message to this queue to get
                            * it to move along, or signal it from another thread
                            * to get it to move along.  
                            * 
                            */
        switch (msg.message) {
            case THREAD_ACKNOWLEDGE:
                printf("main.c > mythread(): THREAD_ACKNOWLEDGE received from thread \"%s\" (0x%x).\n", msg.fromthreadname, msg.fromthread);
                fflush(stdout);
                break;
            default:
                break;
        }
    } while (msg.message != MSG_QUIT);
    printf("main.c > mythread(): Got JMSG_QUIT.\n");
    msg.message = THREAD_EXITING;
    pushmessage(&msg, mainthreadid);
    printf("main.c > mythread(): Calling destroymessagequeue()\n");
    destroymessagequeue();
    printf("main.c > mythread(): Exiting.\n");
    return 0;
}

int main( void ) {
    MESSAGE msg;
    pthread_t mythreadid;
    int ret;

    ret = createmessagequeue("Main Thread");
    if (!ret) {
        printf("main.c > createmessagequeue(): Failed with %d.\n", ret);
        return 0;
    }

    pthread_create(&mythreadid, 0, &mythread, (void *)pthread_self());

    printf("main.c > main(): Press [CTRL-C] to terminate program.\n");
    do {
        /* NON Blocking message queue */
        if (popmessage(&msg)) {
            switch (msg.message) {
                case MSG_QUIT:
                    /* Forward the message on to any other queues */
                    if (pushmessage(&msg, mythreadid)) {
                        printf("main.c > main(): Received JMSG_QUIT. Forwarded message to mythreadid\n");
                        fflush(stdout);
                        pthread_join(mythreadid, 0);
                    }
                    break;
                case THREAD_ACKNOWLEDGE:
                    printf("main.c > main(): Received a THREAD_ACKNOWLEDGE from thread \"%s\" (0x%x)\n", msg.fromthreadname, msg.fromthread);
                    /* Bounce message back for the heck of it */
                    pushmessage(&msg, mythreadid);
                    fflush(stdout);
                   break;
                case THREAD_EXITING:
                    printf("main.c > main(): Received a THREAD_EXITING from thread \"%s\" (0x%x)\n", msg.fromthreadname, msg.fromthread);
                    fflush(stdout);
                    break;
                default:
                    break;
            }
        } else { /* No messages do some important stuff */
            usleep(20000); /* Take a breather */
        }
    } while (msg.message != MSG_QUIT);
    printf("main.c > main(): Calling destroymessagequeue()!\n");
    destroymessagequeue();
    printf("main.c > main(): Exiting program!\n");
    fflush(stdout);
   return 0;
}