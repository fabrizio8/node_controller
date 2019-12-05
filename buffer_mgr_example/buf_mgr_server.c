/*    buf_mgr_server.c - a fork based distributed buffer manager      */

#include "buf_mgr.h"
#include "ddonuts.h"

int     shmid, semid[3];
void    sig_handler(int);  // clean up shm and sem resources

int main(int argc, char *argv[])
{
    MSG                 msg;
    MBUF                raw;
    DONUT               donut;
    int                 inet_sock, new_sock, out_index;
    int                 type_val, id_val, read_val, trigger;
    int                 i, j, k, nsigs, donut_num, node_id;
    socklen_t           fromlen;
    int                 wild_card = INADDR_ANY;
    char                *buffer_ptr;
    struct sockaddr_in  inet_telnum;
    struct hostent      *heptr, *gethostbyname();
    struct sigaction    sigstrc;
    sigset_t            mask;
    struct donut_ring   *shared_ring;
    struct timeval      randtime;
    unsigned short      xsub1[3];

// catch fatal signals to remove shm and sem resources

    int sigs[] = {
                   SIGHUP,  SIGINT, SIGQUIT, SIGPIPE,
                   SIGTERM, SIGBUS, SIGSEGV, SIGFPE
                 };

    nsigs = sizeof(sigs)/sizeof(int);
    sigemptyset(&mask);

    for (i = 0; i < nsigs; i++)
        sigaddset(&mask, sigs[i]);

    for (i = 0; i < nsigs; i++) {
        sigstrc.sa_handler = sig_handler;
        sigstrc.sa_mask    = mask;
        sigstrc.sa_flags   = 0;
        if (sigaction(sigs[i], &sigstrc, NULL) == -1) {
            perror("can't set signals: ");
            exit(1);
        }
    }

    if ((shmid = shmget(SEMKEY, sizeof(struct donut_ring), IPC_CREAT | 0600)) == -1) {
        perror("shared get failed: ");
        exit(1);
    }

    if ((shared_ring = (struct donut_ring *)shmat(shmid, NULL, 0)) == (void *)-1) {
        perror("shared attach failed: ");
        sig_handler(-1);
    }

    for (i = 0; i < NUMSEMIDS; i++)
        if ((semid[i] = semget(SEMKEY+i, NUMFLAVORS, IPC_CREAT | 0600)) == -1) {
            perror("semaphore allocation failed: ");
            sig_handler(-1);
        }

    if (semsetall(semid[PROD], NUMFLAVORS, NUMSLOTS) == -1) {
        perror("semsetall failed: ");
        sig_handler(-1);
    }

    if (semsetall(semid[CONSUMER], NUMFLAVORS, 0) == -1) {
        perror("semsetall failed: ");
        sig_handler(-1);
    }

/***** set up sigaction structure to eliminate zombies *****/

    sigemptyset(&mask);

    sigstrc.sa_handler = SIG_IGN; // ignore this signal to prevent zombie
    sigstrc.sa_mask    = mask;
    sigstrc.sa_flags   = SA_RESTART;

    sigaction(SIGCHLD, &sigstrc, NULL);

/***** allocate a socket to communicate with *****/

    if ((inet_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("inet_sock allocation failed: ");
        sig_handler(-1);
    }

/***** byte copy the wild_card IP address INADDR_ANY into   *****/
/***** IP address structure, along with port and family and *****/
/***** use the structure to give yourself a connect address *****/

    bcopy(&wild_card, &inet_telnum.sin_addr, sizeof(int));
    inet_telnum.sin_family = AF_INET;
    inet_telnum.sin_port   = htons((u_short)PORT);

    if (bind(inet_sock, (struct sockaddr *)&inet_telnum, sizeof(struct sockaddr_in)) == -1)  {
        perror("inet_sock bind failed: ");
        sig_handler(-1);
    }

/***** allow client connect requests to arrive: call-wait 5 *****/

    listen(inet_sock, 5);

    for (i = 0; i < NUMFLAVORS; ++i) {
        for (j = 0; j < NUMSLOTS; ++j) {
            shared_ring->flavor[i][j].node_id   = -1;
            shared_ring->flavor[i][j].prod_id   = -1;
            shared_ring->flavor[i][j].donut_num = -1;
        }
    }
        
    printf("\nTHE BUFFER MANAGER IS UP\n");

/***** forever more, answer the phone and create a child to *****/
/***** handle each client connection (use EINTR check)      *****/

    trigger = 0;
    for (;;) {  /* forever */
        if (trigger++ == 650) {
            write(1, "\n\n*********\n\n", 13);
            if (!fork()) {
                close(1);
                if (open("/tmp/cnt", O_CREAT | O_RDWR, 0600) != 1) {
                    perror("open /tmp/cnt ");
                    exit(3);
                }
                execlp("ps", "ps", "-l", NULL);
                perror("exec ps failed");
                exit(4);
            }
        }
/***** set sizeof(struct sockaddr) into fromlen to specify  *****/
/***** original buffer size for returned address (the       *****/
/***** actual size of the returned address then goes here)  *****/

        fromlen = sizeof(struct sockaddr);

        while ((new_sock = accept(inet_sock, (struct sockaddr *)&inet_telnum, &fromlen)) == -1 && errno == EINTR);

        if (new_sock == -1) {
            perror("accept failed: ");
            sig_handler(-1);
        }
     
        switch (fork()) {
            default: /***** parent takes this case if fork succeeds               *****/
                close(new_sock);
                break;
            case -1: /***** parent takes this case if fork fails                  *****/
                perror("fork failed: ");
                sig_handler(-1);
            case  0: /***** child takes this case if fork succeeds                *****/
                close(inet_sock);

    /***** read message info of message sent by client            *****/
                read_msg(new_sock, &raw.buf);

                type_val  = ntohl(raw.m.mtype);
                id_val    = ntohl(raw.m.mid);
                donut_num = ntohl(raw.m.mdonut_num);
                node_id   = ntohl(raw.m.mnode_id);  

    /***** what type of message has the client sent to us ??     *****/

                switch (type_val) {
                    case PRO_JELLY: 
                        if (p(semid[PROD], JELLY) == -1) {
                            perror("p operation failed: ");
                            exit(9);
                        }
                        shared_ring->flavor[JELLY][shared_ring->in_ptr[JELLY]].node_id   = node_id;
                        shared_ring->flavor[JELLY][shared_ring->in_ptr[JELLY]].prod_id   = id_val;
                        shared_ring->flavor[JELLY][shared_ring->in_ptr[JELLY]].donut_num = donut_num;

                        usleep(100);
                        shared_ring->in_ptr[JELLY] = (shared_ring->in_ptr[JELLY]+1) % NUMSLOTS;

                        if (v(semid[CONSUMER], JELLY) == -1) {
                            perror("v operation failed: ");
                            exit(9);
                        }

                        make_msg(&msg, P_ACK, BUF_MGR, donut_num, 0);
                        if (write(new_sock, &msg, (4*sizeof(int))) == -1) {
                            perror("new_sock write failed: ");
                            exit(3);
                        }
                        close(new_sock);
                        exit(10);

                    case PRO_PLAIN:
                        if ( p(semid[PROD], PLAIN) == -1) {
                            perror("p operation failed: ");
                            exit(9);
                        }
                        shared_ring->flavor[PLAIN][shared_ring->in_ptr[PLAIN]].node_id   = node_id;
                        shared_ring->flavor[PLAIN][shared_ring->in_ptr[PLAIN]].prod_id   = id_val;
                        shared_ring->flavor[PLAIN][shared_ring->in_ptr[PLAIN]].donut_num = donut_num;

                        usleep(100);
                        shared_ring->in_ptr[PLAIN] = (shared_ring->in_ptr[PLAIN]+1) % NUMSLOTS;

                        if ( v(semid[CONSUMER], PLAIN) == -1) {
                            perror("v operation failed: ");
                            exit(9);
                        }
                        make_msg(&msg, P_ACK, BUF_MGR, donut_num, 0);
                        if (write(new_sock, &msg, (4*sizeof(int))) == -1) {
                            perror("new_sock write failed: ");
                            exit(3);
                        }
                        close(new_sock);
                        exit(10);

                    case PRO_COCO:
                        if (p(semid[PROD], COCO) == -1) {
                            perror("p operation failed: ");
                            exit(9);
                        }
                        shared_ring->flavor[COCO][shared_ring->in_ptr[COCO]].node_id   = node_id;
                        shared_ring->flavor[COCO][shared_ring->in_ptr[COCO]].prod_id   = id_val;
                        shared_ring->flavor[COCO][shared_ring->in_ptr[COCO]].donut_num = donut_num;

                        usleep(100);
                        shared_ring->in_ptr[COCO] = (shared_ring->in_ptr[COCO]+1) % NUMSLOTS;

                        if ( v(semid[CONSUMER], COCO) == -1) {
                            perror("v operation failed: ");
                            exit(9);
                        }
                        make_msg(&msg, P_ACK, BUF_MGR, donut_num, 0);
                        if (write(new_sock, &msg, (4*sizeof(int))) == -1) {
                            perror("new_sock write failed: ");
                            exit(3);
                        }
                        close(new_sock);
                        exit(10);

                    case PRO_CREAM:
                        if (p(semid[PROD], CREAM) == -1) {
                            perror("p operation failed: ");
                            exit(9);
                        }
                        shared_ring->flavor[CREAM][shared_ring->in_ptr[CREAM]].node_id   = node_id;
                        shared_ring->flavor[CREAM][shared_ring->in_ptr[CREAM]].prod_id   = id_val;
                        shared_ring->flavor[CREAM][shared_ring->in_ptr[CREAM]].donut_num = donut_num;

                        usleep(100);
                        shared_ring->in_ptr[CREAM] = (shared_ring->in_ptr[CREAM]+1) % NUMSLOTS;

                        if (v(semid[CONSUMER], CREAM) == -1) {
                            perror("v operation failed: ");
                            exit(9);
                        }
                        make_msg(&msg, P_ACK, BUF_MGR, donut_num, 0);
                        if (write(new_sock, &msg, (4*sizeof(int))) == -1) {
                            perror("new_sock write failed: ");
                            exit(3);
                        }
                        close(new_sock);
                        exit(10);

                    case CON_JELLY:
                        if (p(semid[CONSUMER], JELLY) == -1) {
                            perror("p operation failed: ");
                            exit(9);
                        }
                        donut = shared_ring->flavor[JELLY][(out_index = shared_ring->outptr[JELLY])];

                        if (donut.donut_num == -1)
                            printf("\n-1 JELLY donut at %d index\n", out_index);

                        usleep(100);
                        shared_ring->outptr[JELLY] = (shared_ring->outptr[JELLY]+1) % NUMSLOTS;

                        if ( v(semid[PROD], JELLY) == -1) {
                            perror("v operation failed: ");
                            exit(9);
                        }
                        make_msg(&msg, C_ACK, donut.prod_id, donut.donut_num, donut.node_id);

                        if (write(new_sock, &msg, (4*sizeof(int))) == -1) {
                            perror("new_sock write failed: ");
                            exit(3);
                        }
                        close(new_sock);
                        exit(10);

                    case CON_PLAIN:
                        if ( p(semid[CONSUMER], PLAIN) == -1) {
                            perror("p operation failed: ");
                            exit(9);
                        }
                        donut = shared_ring->flavor[PLAIN][(out_index = shared_ring->outptr[PLAIN])];

                        if (donut.donut_num == -1)
                            printf("\n-1 PLAIN donut at %d index\n", out_index);
                        usleep(100);

                        shared_ring->outptr[PLAIN] = (shared_ring->outptr[PLAIN]+1) % NUMSLOTS;

                        if ( v(semid[PROD], PLAIN) == -1) {
                            perror("v operation failed: ");
                            exit(9);
                        }
                        make_msg(&msg, C_ACK, donut.prod_id, donut.donut_num, donut.node_id);

                        if (write(new_sock, &msg, (4*sizeof(int))) == -1) {
                            perror("new_sock write failed: ");
                            exit(3);
                        }

                        close(new_sock);
                        exit(10);


                    case CON_COCO:
                        if (p(semid[CONSUMER], COCO) == -1) {
                            perror("p operation failed: ");
                            exit(9);
                        }

                        donut = shared_ring->flavor[COCO][(out_index = shared_ring->outptr[COCO])];
                        if (donut.donut_num == -1)
                            printf("\n-1 COCON donut at %d index\n", out_index);

                        usleep(100);
                        shared_ring->outptr[COCO] = (shared_ring->outptr[COCO]+1) % NUMSLOTS;

                        if (v(semid[PROD], COCO) == -1) {
                            perror("v operation failed: ");
                            exit(9);
                        }

                        make_msg(&msg, C_ACK, donut.prod_id, donut.donut_num, donut.node_id);

                        if (write(new_sock, &msg, (4*sizeof(int))) == -1) {
                            perror("new_sock write failed: ");
                            exit(3);
                        }

                        close(new_sock);
                        exit(10);

                    case CON_CREAM:
                        if ( p(semid[CONSUMER], CREAM) == -1) {
                            perror("p operation failed: ");
                            exit(9);
                        }
                        donut = shared_ring->flavor[CREAM][(out_index = shared_ring->outptr[CREAM])];
                        if (donut.donut_num == -1)
                            printf("\n-1 CREAM donut at %d index\n", out_index);
                        usleep(100);
                        shared_ring->outptr[CREAM] = (shared_ring->outptr[CREAM]+1) % NUMSLOTS;

                        if (v(semid[PROD], CREAM) == -1) {
                            perror("v operation failed: ");
                            exit(9);
                        }
                        make_msg(&msg, C_ACK, donut.prod_id, donut.donut_num, donut.node_id);
                        if (write(new_sock, &msg, (4*sizeof(int))) == -1) {
                            perror("new_sock write failed: ");
                            exit(3);
                        }
                        close(new_sock);
                        exit(10);

                    default:
                        printf("\nUNRECOGNIZED NETWORK MESSAGE, CHILD TERMINATING\n");
                        make_msg(&msg, M_ERR, 999999, 999999, 999999);
                        if (write(new_sock, &msg, (4*sizeof(int))) == -1) {
                            perror("new_sock write failed: ");
                            exit(3);
                        }
                        close(new_sock);
                        exit(10);
                } // message type switch
        } // fork switch
    } // buffer manager forever loop
} // main

void    sig_handler(int sig)
{
    int i, j, k;

    printf("In signal handler with signal # %d\n", sig);

    if (shmctl(shmid, IPC_RMID,0) == -1) {
        perror("handler failed shm RMID: ");
    }

    for (i = 0; i < NUMSEMIDS; i++) {
        if (semctl(semid[i], 0, IPC_RMID,0) == -1) {
            perror("handler failed sem RMID: ");
        }
    }
    exit(4);
}
