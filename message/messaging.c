#include "messaging.h"

#define MAX_MSGS 1000

static int lock() {

    int x = 0;
    pthread_t callingthreadid = pthread_self();
    x = pthread_mutex_lock(&messagelock);

#ifdef QUEUE_DEBUG
    if (x != 0 ) {
        printf("lock(%lld) failed to lock mutex.\n", callingthreadid);
        switch (x) {
            case EINVAL:
                printf("\t\tEINVAL:The mutex was created with the protocol attribute having the value PTHREAD_PRIO_PROTECT and the calling thread's priority is higher than the mutex's current priority ceiling.\n");
                break;
            case EBUSY:
                printf("\t\tEBUSY:The mutex could not be acquired because it was already locked.\n");
                break;
            case EAGAIN:
                printf("\t\tEAGAIN:The mutex could not be acquired because the maximum number of recursive locks for mutex has been exceeded.\n");
                break;
            case EDEADLK:
                printf("\t\tEDEADLK:The current thread already owns the mutex.\n");
                break;
            case EPERM:
                printf("\t\tEPERM:The current thread does not own the mutex.\n");
                break;
            default:
                break;
        }
    }
#endif
    return x;
}

static int unlock() {

    int x = 0;
    pthread_t callingthreadid = pthread_self();
    x = pthread_mutex_unlock(&messagelock);
#ifdef QUEUE_DEBUG
    if (x) {
        printf("\tunlock(&lld) failed to unlock mutex.\n", callingthreadid);
    }
#endif
    return x;
}

static char *getthreadname( pthread_t idnum ) {

    static char retval[] = "";
    JQUEUE *jq = jqueue;
    if (!jq)
        return retval;
    lock();
    do {
        if (jq->owningthreadid == idnum) {
            unlock();
            return jq->threadname;
        }
        jq = jq->next;
    } while (jq);

    unlock();
    return retval;
}

int createmessagequeue( char *description ) {

    int retval = JMSG_QUEUENOTFOUND;
    JQUEUE *jq = jqueue;
    pthread_t callingthreadid = pthread_self();

    pthread_mutexattr_t ptt;

    if (!jq) {

#ifdef QUEUE_DEBUG
        printf("messaging.c > Initializing Messaging System ********************** \n");
#endif
        pthread_mutexattr_init(&ptt);
#ifdef PTHREAD_MUTEX_RECURSIVE_NP
        pthread_mutexattr_settype(&ptt, PTHREAD_MUTEX_RECURSIVE_NP);
#else
        pthread_mutexattr_settype(&ptt, PTHREAD_MUTEX_RECURSIVE);
#endif
        pthread_mutex_init(&messagelock, &ptt);
        lock();
        jqueue = calloc(1, sizeof(JQUEUE));
        if (jqueue) {

            jqueue->jmessagelist = calloc(JMSG_INIT_QUEUE_SIZE, sizeof(JMESSAGE));

            if (jqueue->jmessagelist) {

                jqueue->queuesize = JMSG_INIT_QUEUE_SIZE;
                jqueue->owningthreadid = callingthreadid;
                jqueue->lastfulljmessage = JMSG_EMPTY_QUEUE;

                if (description) {
                    if (strlen(description) < 100) {
                        sprintf(jqueue->threadname, "%s", description);
                    } else {
                        sprintf(jqueue->threadname, "%.*s", 99, description);
                    }
                }

                sprintf(jqueue->semname, "SEM%d", (int)callingthreadid);
                jqueue->messagesignal = sem_open(jqueue->semname, O_CREAT, 0644, 0);

#ifdef QUEUE_DEBUG
                printf("messaging.c > Message queue set up for \"%s\" (%lld).\n ", jqueue->threadname, callingthreadid);
#endif
                retval = JMSG_OKAY;
            } else {
                free(jqueue);
                jqueue = 0;
                retval = JMSG_OUTOFMEMORY;
            }
        } else {
            retval = JMSG_OUTOFMEMORY;
        }
    } else {

        lock();
        do {
            if (jq->owningthreadid == callingthreadid) {
                retval = JMSG_OKAY;
                break;
            }
            if (jq->next == 0) {
                jq->next = calloc(1, sizeof(JQUEUE));
                if (jq->next) {

                    jq->next->jmessagelist = calloc(JMSG_INIT_QUEUE_SIZE, sizeof(JMESSAGE));

                    if (jq->next->jmessagelist) {
                        jq->next->previous = jq;
                        jq->next->queuesize = JMSG_INIT_QUEUE_SIZE;
                        jq->next->owningthreadid = callingthreadid;
                        jq->next->lastfulljmessage = JMSG_EMPTY_QUEUE;
                        sprintf(jq->next->semname, "SEM%d", (int)callingthreadid);
                        if (description) {
                            if (strlen(description) < 100) {
                                sprintf(jq->next->threadname, "%s", description);
                            } else {
                                sprintf(jq->next->threadname, "%.*s", 99, description);
                            }
                        }
#ifdef QUEUE_DEBUG
                        printf("messaging.c > Message queue set up for \"%s\" (%lld).\n ", jq->next->threadname, callingthreadid);
#endif
                        jq->next->messagesignal = sem_open(jq->next->semname, O_CREAT, S_IRWXU, 0);
                        retval = JMSG_OKAY;
                    } else {
                        free(jq);
                        jq = 0;
                        retval = JMSG_OUTOFMEMORY;
                    }
                }
                break;
            } 
            jq = jq->next;
        } while (jq);
    }
    unlock();
    return retval;
}


int destroymessagequeue() {

    int i = 0, retval = JMSG_QUEUENOTFOUND;
    JQUEUE *jq   = jqueue;
    JMESSAGE *jm = 0;
    pthread_t callingthreadid = pthread_self();
    lock();
    if (jq) {
        /* 
         * Search for messages waiting to be delivered that are from THIS
         * QUEUE that we're destroying.  We need to replace the pointer that
         * is pointing to the name of this queue with a generic (const char *)
         * saying "(dead queue)" so we don't get an invalid memory read when
         * it's delivered.
         * 
         */
        do {
            if (jq->owningthreadid != callingthreadid) {
                for (i = 0; i <= jq->lastfulljmessage; i++) {
                    jm = &jq->jmessagelist[i];
                    if (jm->fromthread == callingthreadid) {
                        jm->fromthreadname = "(dead queue)";
                    }
                }
            }
            jq = jq->next;
        } while (jq);

        /* Reset jq, find this queue and destroy it */
        jq = jqueue;
        do {
            if (jq->owningthreadid == callingthreadid) {
                /* Found the queue */
                retval = JMSG_OKAY;
#ifdef QUEUE_DEBUG
                printf("messaging.c > Destroying message queue for \"%s\" (%lld)\n", jq->threadname, jq->owningthreadid);
#endif
                /* 
                 Free any internal pointers that were supposed
                 to be freed after retrieval.
                 */
                for (i = 0; i <= jq->lastfulljmessage; i++) {
                    sem_wait(jq->messagesignal);
                    if (jq->jmessagelist[i].needtofreepointer) {
                        free(jq->jmessagelist[i].ptr.charptr);
                    }

                }
                /* Free the queue messages */
                free(jq->jmessagelist);

                sem_unlink(jq->semname);

                /* Unlink the queue */
                if (jq->previous) { /* If this isn't the first element */
                    jq->previous->next = jq->next;
                    if (jq->next) {
                        jq->next->previous = jq->previous;
                    }
                } else {
                    if (jq->next) {
                        jq->next->previous = 0;
                    }
                }
                free(jq);
                break;
            }
            jq = jq->next;
        } while (jq);
    }
    unlock();
    return retval;
}

int pushmessage( JMESSAGE *jmsg, pthread_t to_thread_id ) {

    int queuespotsleft = 0, retval = JMSG_QUEUENOTFOUND;
    JQUEUE *jq   = jqueue;
    JMESSAGE *jm = 0, *newjmqueue;
    pthread_t callingthreadid = pthread_self();

    if (!jq) {
        return JMSG_QUEUENOTFOUND;
    }
    lock();
    do {
        if (jq->owningthreadid == to_thread_id) {
            if (jq->lastfulljmessage == JMSG_EMPTY_QUEUE) {
                jq->lastfulljmessage = 0;
                jm = &jq->jmessagelist[0];
            } else {
                if (jq->lastfulljmessage + 1 > MAX_MSGS) {
                    /* We have too many messages backed up */
                    unlock();
                    return JMSG_QUEUEFULL;
                }
                if (jq->lastfulljmessage + 1 > (jq->queuesize - 1)) {
                    /* 
                     * We're getting backed up, we need to allocate more
                     * space.  This is slowly moving the allocated message
                     * queue memory for this thread towards the MAX_MSGS
                     */
                    queuespotsleft = MAX_MSGS - (jq->lastfulljmessage + 1);
                    if (queuespotsleft > 50)
                        queuespotsleft = 50;
                    if (queuespotsleft > 0) {
                        newjmqueue = realloc(jq->jmessagelist, ((jq->lastfulljmessage + 1) * sizeof(JMESSAGE)));
                        if (!newjmqueue) {
                            unlock();
                            return JMSG_OUTOFMEMORY;
                        }
                        jq->jmessagelist = newjmqueue;
                        jq->lastfulljmessage++;
                        jm = &jq->jmessagelist[jq->lastfulljmessage];
                    } else {
                        retval = JMSG_QUEUEFULL;
                    }
                } else {
                    /* It's within the bounds of the allocated message space */
                    jm = &jq->jmessagelist[++jq->lastfulljmessage];
                }
            }
            if (jm) {
                retval = JMSG_OKAY;
                memcpy(jm, jmsg, sizeof(JMESSAGE));
                jm->fromthread = callingthreadid;
                jm->fromthreadname = getthreadname(callingthreadid);
                /* Go ahead and increment the semaphore count in case
                 * they're calling waitmessage()
                 */
                sem_post(jq->messagesignal);
            }
            break;
        }
        jq = jq->next;
    } while (jq);
    unlock();
    return retval;
}

int popmessage( JMESSAGE *jmsg ) {
    int retval = JMSG_QUEUENOTFOUND;
    JQUEUE *jq = jqueue;
    pthread_t callingthreadid = pthread_self();
    if (!jq) {
        return JMSG_QUEUENOTFOUND;
    }
    memset(jmsg, 0, sizeof(JMESSAGE));
    lock();
    do {
        if (jq->owningthreadid == callingthreadid) {
            if (jq->lastfulljmessage > JMSG_EMPTY_QUEUE) {
                memcpy(jmsg, &jq->jmessagelist[jq->lastfulljmessage], sizeof(JMESSAGE));
                memset(&jq->jmessagelist[jq->lastfulljmessage], 0, sizeof(JMESSAGE));
                jq->lastfulljmessage--;
                retval = ((jq->lastfulljmessage == JMSG_EMPTY_QUEUE) ? JMSG_LASTMESSAGE : JMSG_MOREMESSAGES);
                /* Decrease the semaphore count because they're NOT calling waitmessage() but may later... */
                sem_wait(jq->messagesignal);
            } else {
                retval = JMSG_NOMESSAGE;
            }
            break;
        }
        jq = jq->next;
    } while (jq);
    unlock();
    return retval;
}

/* 
 * This function is only called by waitmessage()  It doesn't decrease the
 * semaphore count, because we've already waited for it to be signaled 
 * in waitmessage().  This just pulls the message off of the stack.
 * 
 */
static int popmessagenosem( JMESSAGE *jmsg ) {
    int retval = JMSG_QUEUENOTFOUND;
    JQUEUE *jq = jqueue;
    pthread_t callingthreadid = pthread_self();

    if (!jq) {
        return JMSG_QUEUENOTFOUND;
    }

    lock();
    do {
        if (jq->owningthreadid == callingthreadid) {
            if (jq->lastfulljmessage > JMSG_EMPTY_QUEUE) {
                memmove(jmsg, &jq->jmessagelist[jq->lastfulljmessage], sizeof(JMESSAGE));
                memset(&jq->jmessagelist[jq->lastfulljmessage], 0, sizeof(JMESSAGE));
                jq->lastfulljmessage--;
                retval = ((jq->lastfulljmessage == JMSG_EMPTY_QUEUE) ? JMSG_NOMESSAGE :
                ((jq->lastfulljmessage == 0) ? JMSG_LASTMESSAGE : JMSG_MOREMESSAGES));
            } else {
                retval = JMSG_NOMESSAGE;
            }
            break;
        }
        jq = jq->next;
    } while (jq);
    unlock();
    return retval;
}

int waitmessage( JMESSAGE *jmsg ) {

    JQUEUE *jq = jqueue;
    sem_t *waitingon = 0;
    pthread_t callingthreadid = pthread_self();
    if (!jq) {
        return JMSG_QUEUENOTFOUND;
    }
    lock();
    do {
        if (jq->owningthreadid == callingthreadid) {
            //printf("Waiting on semaphore %s to be signalled...\n", jq->semname);
            waitingon = jq->messagesignal;
            break;
        }
        jq = jq->next;
    } while (jq);
    unlock();
    if (!waitingon)
        return JMSG_QUEUENOTFOUND;
    //printf("waiting on semaphore!\n");
    sem_wait(waitingon);
    popmessagenosem(jmsg);
    //printf("semaphore signalled! continuing...\n");
    return JMSG_OKAY;
}