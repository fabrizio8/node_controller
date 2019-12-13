#include "messaging.h"

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
    MSG_Q *mq = mqueue;
    if (!mq) return retval;
    lock();
    do {
        if (mq->owningthreadid == idnum) {
            unlock();
            return mq->threadname;
        }
        mq = mq->next;
    } while (mq);

    unlock();
    return retval;
}

int createmessagequeue( char *description ) {
    int retval = MSG_QUEUENOTFOUND;
    MSG_Q *mq = mqueue;
    pthread_t callingthreadid = pthread_self();

    pthread_mutexattr_t ptt;
    if (!mq) {
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
        mqueue = calloc(1, sizeof(MSG_Q));
        if (mqueue) {
            mqueue->jmessagelist = calloc(MSG_INIT_QUEUE_SIZE, sizeof(MESSAGE));
            if (mqueue->jmessagelist) {
                mqueue->queuesize = MSG_INIT_QUEUE_SIZE;
                mqueue->owningthreadid = callingthreadid;
                mqueue->lastfulljmessage = MSG_EMPTY_QUEUE;
                if (description) {
                    if (strlen(description) < 100) {
                        sprintf(mqueue->threadname, "%s", description);
                    } else {
                        sprintf(mqueue->threadname, "%.*s", 99, description);
                    }
                }
                sprintf(mqueue->semname, "SEM%d", (int)callingthreadid);
                mqueue->messagesignal = sem_open(mqueue->semname, O_CREAT, 0644, 0);
#ifdef QUEUE_DEBUG
                printf("messaging.c > Message queue set up for \"%s\" (%lld).\n ", mqueue->threadname, callingthreadid);
#endif
                retval = MSG_OKAY;
            } else {
                free(mqueue);
                mqueue = 0;
                retval = MSG_OUTOFMEMORY;
            }
        } else {
            retval = MSG_OUTOFMEMORY;
        }
    } else {
        lock();
        do {
            if (mq->owningthreadid == callingthreadid) {
                retval = MSG_OKAY;
                break;
            }
            if (mq->next == 0) {
                mq->next = calloc(1, sizeof(MSG_Q));
                if (mq->next) {

                    mq->next->jmessagelist = calloc(MSG_INIT_QUEUE_SIZE, sizeof(MESSAGE));

                    if (mq->next->jmessagelist) {
                        mq->next->previous = mq;
                        mq->next->queuesize = MSG_INIT_QUEUE_SIZE;
                        mq->next->owningthreadid = callingthreadid;
                        mq->next->lastfulljmessage = MSG_EMPTY_QUEUE;
                        sprintf(mq->next->semname, "SEM%d", (int)callingthreadid);
                        if (description) {
                            if (strlen(description) < 100) {
                                sprintf(mq->next->threadname, "%s", description);
                            } else {
                                sprintf(mq->next->threadname, "%.*s", 99, description);
                            }
                        }
#ifdef QUEUE_DEBUG
                        printf("messaging.c > Message queue set up for \"%s\" (%lld).\n ", mq->next->threadname, callingthreadid);
#endif
                        mq->next->messagesignal = sem_open(mq->next->semname, O_CREAT, S_IRWXU, 0);
                        retval = MSG_OKAY;
                    } else {
                        free(mq);
                        mq = 0;
                        retval = MSG_OUTOFMEMORY;
                    }
                }
                break;
            } 
            mq = mq->next;
        } while (mq);
    }
    unlock();
    return retval;
}

int destroymessagequeue() {
    int i = 0, retval = MSG_QUEUENOTFOUND;
    MSG_Q *mq = mqueue;
    MESSAGE *m = 0;
    pthread_t callingthreadid = pthread_self();
    lock();
    if (mq) {
        /* 
         * Search for messages waiting to be delivered that are from THIS
         * QUEUE that we're destroying.  We need to replace the pointer that
         * is pointing to the name of this queue with a generic (const char *)
         * saying "(dead queue)" so we don't get an invalid memory read when
         * it's delivered.
         * 
         */
        do {
            if (mq->owningthreadid != callingthreadid) {
                for (i = 0; i <= mq->lastfulljmessage; i++) {
                    m = &mq->jmessagelist[i];
                    if (m->fromthread == callingthreadid) {
                        m->fromthreadname = "(dead queue)";
                    }
                }
            }
            mq = mq->next;
        } while (mq);

        /* Reset mq, find this queue and destroy it */
        mq = mqueue;
        do {
            if (mq->owningthreadid == callingthreadid) {
                /* Found the queue */
                retval = MSG_OKAY;
#ifdef QUEUE_DEBUG
                printf("messaging.c > Destroying message queue for \"%s\" (%lld)\n", mq->threadname, mq->owningthreadid);
#endif
                /* 
                 Free any internal pointers that were supposed
                 to be freed after retrieval.
                 */
                for (i = 0; i <= mq->lastfulljmessage; i++) {
                    sem_wait(mq->messagesignal);
                    if (mq->jmessagelist[i].needtofreepointer) {
                        free(mq->jmessagelist[i].ptr.charptr);
                    }

                }
                /* Free the queue messages */
                free(mq->jmessagelist);
                sem_unlink(mq->semname);

                /* Unlink the queue */

                if (mq->previous) { /* If this isn't the first element */
                    mq->previous->next = mq->next;
                    if (mq->next) {
                        mq->next->previous = mq->previous;
                    }
                } else {
                    if (mq->next) {
                        mq->next->previous = 0;
                    }
                }
                free(mq);
                break;
            }
            mq = mq->next;
        } while (mq);
    }
    unlock();
    return retval;
}

int pushmessage( MESSAGE *msg, pthread_t to_thread_id ) {
    int remaining_space = 0, retval = MSG_QUEUENOTFOUND;
    MSG_Q *mq = mqueue;
    MESSAGE *m = 0, *mqueue_new;
    pthread_t callingthreadid = pthread_self();
    if (!mq) {
        return MSG_QUEUENOTFOUND;
    }
    lock();
    do {
        if (mq->owningthreadid == to_thread_id) {
            if (mq->lastfulljmessage == MSG_EMPTY_QUEUE) {
                mq->lastfulljmessage = 0;
                m = &mq->jmessagelist[0];
            } else {
                if (mq->lastfulljmessage + 1 > MAX_MESSAGES) {
                    /* We have too many messages backed up */
                    unlock();
                    return MSG_QUEUEFULL;
                }
                if (mq->lastfulljmessage + 1 > (mq->queuesize - 1)) {
                    /* 
                     * We're getting backed up, we need to allocate more
                     * space.  This is slowly moving the allocated message
                     * queue memory for this thread towards the MAX_JMESSAGES
                     * 
                     */
                    remaining_space = MAX_JMESSAGES - (mq->lastfulljmessage + 1);
                    if (remaining_space > 50)
                        remaining_space = 50;
                    if (remaining_space > 0) {
                        mqueue_new = realloc(mq->jmessagelist, ((mq->lastfulljmessage + 1) * sizeof(MESSAGE)));
                        if (!mqueue_new) {
                            unlock();
                            return MSG_OUTOFMEMORY;
                        }
                        mq->jmessagelist = mqueue_new;
                        mq->lastfulljmessage++;
                        m = &mq->jmessagelist[mq->lastfulljmessage];
                    } else {
                        retval = MSG_QUEUEFULL;
                    }
                } else {
                    /* It's withing the bounds of the allocated message space */
                    m = &mq->jmessagelist[++mq->lastfulljmessage];
                }
            }
            if (m) {
                retval = MSG_OKAY;
                memcpy(m, msg, sizeof(MESSAGE));
                m->fromthread = callingthreadid;
                m->fromthreadname = getthreadname(callingthreadid);
                /* Go ahead and increment the semaphore count in case
                 * they're calling waitmessage()
                 * 
                 */
                sem_post(mq->messagesignal);
            }
            break;
        }
        mq = mq->next;
    } while (mq);
    unlock();
    return retval;
}


int popmessage( MESSAGE *msg ) {
    int retval = MSG_QUEUENOTFOUND;
    MSG_Q *mq = mqueue;
    pthread_t callingthreadid = pthread_self();
    if (!mq) {
        return MSG_QUEUENOTFOUND;
    }
    memset(msg, 0, sizeof(MESSAGE));
    lock();
    do {
        if (mq->owningthreadid == callingthreadid) {
            if (mq->lastfulljmessage > MSG_EMPTY_QUEUE) {
                memcpy(msg, &mq->jmessagelist[mq->lastfulljmessage], sizeof(MESSAGE));
                memset(&mq->jmessagelist[mq->lastfulljmessage], 0, sizeof(MESSAGE));
                mq->lastfulljmessage--;
                retval = ((mq->lastfulljmessage == MSG_EMPTY_QUEUE) ? MSG_LASTMESSAGE : MSG_MOREMESSAGES);
                /* Decrease the semaphore count because they're NOT calling waitmessage() but may later... */
                sem_wait(mq->messagesignal);
            } else {
                retval = MSG_NOMESSAGE;
            }
            break;
        }
        mq = mq->next;
    } while (mq);
    unlock();
    return retval;
}

/* 
 * This function is only called by waitmessage()  It doesn't decrease the
 * semaphore count, because we've already waited for it to be signaled 
 * in waitmessage().  This just pulls the message off of the stack.
 * 
 */
static int popmessagenosem( MESSAGE *msg ) {
    int retval = MSG_QUEUENOTFOUND;
    MSG_Q *mq = mqueue;
    pthread_t callingthreadid = pthread_self();
    if (!mq) {
        return MSG_QUEUENOTFOUND;
    }
    lock();
    do {
        if (mq->owningthreadid == callingthreadid) {
            if (mq->lastfulljmessage > MSG_EMPTY_QUEUE) {
                memmove(msg, &mq->jmessagelist[mq->lastfulljmessage], sizeof(MESSAGE));
                memset(&mq->jmessagelist[mq->lastfulljmessage], 0, sizeof(MESSAGE));
                mq->lastfulljmessage--;
                retval = ((mq->lastfulljmessage == MSG_EMPTY_QUEUE) ? MSG_NOMESSAGE :
                ((mq->lastfulljmessage == 0) ? MSG_LASTMESSAGE : MSG_MOREMESSAGES));
            } else {
                retval = MSG_NOMESSAGE;
            }
            break;
        }
        mq = mq->next;
    } while (mq);
    unlock();
    return retval;
}

int waitmessage( MESSAGE *msg ) {
    MSG_Q *mq = mqueue;
    sem_t *waitingon = 0;
    pthread_t callingthreadid = pthread_self();
    if (!mq) {
        return MSG_QUEUENOTFOUND;
    }
    lock();
    do {
        if (mq->owningthreadid == callingthreadid) {
            //printf("Waiting on semaphore %s to be signalled...\n", mq->semname);
            waitingon = mq->messagesignal;
            break;
        }
        mq = mq->next;
    } while (mq);
    unlock();
    if (!waitingon) return MSG_QUEUENOTFOUND;
    //printf("waiting on semaphore!\n");
    sem_wait(waitingon);
    popmessagenosem(msg);
    //printf("semaphore signalled! continuing...\n");
    return MSG_OKAY;
}