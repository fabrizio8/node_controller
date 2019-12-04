/*    node controller.c - a Lamport algorithm node controller      */
/*    USAGE:  node controller my_role<1-n> node_cnt<n>             */
/*            where role n does all connect and role 1 all accepts */


#include "buf_mgr.h"
#include "ddonuts.h"

#include <unistd.h>
#include <pthread.h>

#define  MAXNODE	16

char    *host_list[] = { "host list",
                         "cs91515-1", "cs91515-2", "cs91515-3",
                         "cs91515-4", "cs91515-5", "cs91515-6"
                       }; 

typedef struct thread_arg {
    int my_chan;
    int my_node_id;
} TH_ARG;

int connected_ch[MAXNODE];

void *chan_monitor(void *);
void *msg_maker(void *);

int main(int argc, char *argv[])
{
    MSG     		    msg;
    MBUF			    raw;
    DONUT			    donut;
    int    	 		    inet_sock, new_sock, out_index, my_role, node_cnt;
    int			        my_chan, type_val, id_val, read_val, trigger;
    int     		    i, j, k, nsigs, donut_num, node_id;
    int			        wild_card = INADDR_ANY;
    int			        connect_cnt = 0, th_index = 0;
    socklen_t           fromlen;
    char    		    *buffer_ptr;
    struct sockaddr_in 	inet_telnum;
    struct hostent 		*heptr, *gethostbyname();
    struct sigaction 	sigstrc;
    sigset_t		    mask;
    struct donut_ring   *shared_ring;
    struct timeval      randtime;
    unsigned short      xsub1[3];
    TH_ARG 			    *th_arg;
    pthread_t		    thread_ids[MAXNODE];
    
    my_role  = atoi(argv[1]);
    node_cnt = atoi(argv[2]);

// Unless I'm node n of n nodes, I need to do at least 1 accept
    if (node_cnt - my_role > 0) {

        if ((inet_sock=socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            perror("inet_sock allocation failed: ");
            exit(2);
        }

/***** byte copy the wild_card IP address INADDR_ANY into   *****/
/***** IP address structure, along with port and family and *****/
/***** use the structure to give yourself a connect address *****/

        bcopy(&wild_card, &inet_telnum.sin_addr, sizeof(int));
        inet_telnum.sin_family = AF_INET;
        inet_telnum.sin_port = htons((u_short)PORT);

        if (bind(inet_sock, (struct sockaddr *)&inet_telnum, sizeof(struct sockaddr_in)) == -1) {
            perror("inet_sock bind failed: ");
            exit(2);
        }

/***** allow client connect requests to arrive: call-wait 5 *****/

        listen(inet_sock, 5);

/***** set sizeof(struct sockaddr) into fromlen to specify  *****/
/***** original buffer size for returned address (the       *****/
/***** actual size of the returned address then goes here)  *****/

        fromlen = sizeof(struct sockaddr);

// Accept a connection based on role ... if role is 3 and node_cnt is 5
// then perform 2 accepts

        for (i = 0; i < (node_cnt - my_role); ++i) {

// Accept connection and spawn listener thread

            while ((connected_ch[i] = accept(inet_sock, (struct sockaddr *)&inet_telnum, 
                                            &fromlen)) == -1 && errno == EINTR);
            if (connected_ch[i] == -1) {
                perror("accept failed: ");
                exit(2);
            }

            ++connect_cnt;

            th_arg = malloc(sizeof(TH_ARG));
            th_arg->my_chan = connected_ch[i];
            th_arg->my_node_id = my_role;

            if ((errno = pthread_create(&thread_ids[th_index++], NULL, chan_monitor, (void *)th_arg)) != 0) {
                perror("pthread_create channel monitor failed ");
                exit(3);
            }
        }
    } // some accepts required

    switch(my_role) {
        case 1:
            break; //all connected
        case 2:
// need 1 connect, access hostname array for connect target
            if ((heptr = gethostbyname( host_list[1] )) == NULL) {
                perror("gethostbyname failed: ");
                exit(1);
            }
            if ((connected_ch[connect_cnt] = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
                perror("connect channel allocation failed: ");
                exit(2);
            }

            bcopy(heptr->h_addr, &inet_telnum.sin_addr, heptr->h_length);
            inet_telnum.sin_family = AF_INET;
            inet_telnum.sin_port = htons( (u_short)PORT );

    // connect to target and spawn listener thread

            if (connect(connected_ch[connect_cnt], (struct sockaddr *)&inet_telnum,
                                sizeof(struct sockaddr_in)) == -1) {
                perror("inet_sock connect failed: ");
                exit(2);
            }
            th_arg = malloc(sizeof(TH_ARG));
            th_arg->my_chan = connected_ch[connect_cnt];
            th_arg->my_node_id = my_chan = my_role;
            printf("\nconnected from %s to %s\n", host_list[my_role], host_list[1]);

            if ((errno = pthread_create(&thread_ids[th_index++], NULL, chan_monitor, (void *)th_arg)) != 0) {
                perror("pthread_create channel monitor failed ");
                exit(3);
            }
            break; // accepts and 1 connect
        case 3:
// need 2 connects, access hostname array for connect targets
            if ((heptr = gethostbyname( host_list[1] )) == NULL) {
                perror("gethostbyname failed: ");
                exit(1);
            }
            if ((connected_ch[connect_cnt] = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
                perror("connect channel allocation failed: ");
                exit(2);
            }
            bcopy(heptr->h_addr, &inet_telnum.sin_addr, heptr->h_length);
            inet_telnum.sin_family = AF_INET;
            inet_telnum.sin_port = htons((u_short)PORT);

            if (connect(connected_ch[connect_cnt], (struct sockaddr *)&inet_telnum,
                        sizeof(struct sockaddr_in)) == -1) {
                perror("inet_sock connect failed: ");
                exit(2);
            }
            th_arg = malloc(sizeof(TH_ARG));
            th_arg->my_chan = connected_ch[connect_cnt];
            th_arg->my_node_id = my_role;
             printf("\nconnected from %s to %s\n", host_list[my_role], host_list[1]);

            if ((errno = pthread_create(&thread_ids[th_index++], NULL, chan_monitor, (void *)th_arg)) != 0) {
                 perror("pthread_create channel monitor failed ");
                 exit(3);
            }
            ++connect_cnt;

            if ((heptr = gethostbyname( host_list[2] )) == NULL) {
                perror("gethostbyname failed: ");
                exit(1);
            }
            if ((connected_ch[connect_cnt] = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
                perror("connect channel allocation failed: ");
                exit(2);
            }

            bcopy(heptr->h_addr, &inet_telnum.sin_addr, heptr->h_length);
            inet_telnum.sin_family = AF_INET;
            inet_telnum.sin_port = htons( (u_short)PORT );

            if (connect(connected_ch[connect_cnt], (struct sockaddr *)&inet_telnum, sizeof(struct sockaddr_in)) == -1) {
                perror("inet_sock connect failed: ");
                exit(2);
            }
            th_arg = malloc(sizeof(TH_ARG));
            th_arg->my_chan = connected_ch[connect_cnt];
            th_arg->my_node_id = my_role;
            printf("\nconnected from %s to %s\n", host_list[my_role], host_list[2]);

            if ((errno = pthread_create(&thread_ids[th_index++], NULL, chan_monitor, (void *)th_arg)) != 0) {
                 perror("pthread_create channel monitor failed ");
                 exit(3);
            }
            break; // accepts and 2 connects
        case 4:

            // need 3 connects, access hostname array for connect targets
            if ((heptr = gethostbyname( host_list[1] )) == NULL) {
                perror("gethostbyname failed: ");
                exit(1);
            }
            if ((connected_ch[connect_cnt] = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
                perror("connect channel allocation failed: ");
                exit(2);
            }

            bcopy(heptr->h_addr, &inet_telnum.sin_addr, heptr->h_length);
            inet_telnum.sin_family = AF_INET;
            inet_telnum.sin_port = htons( (u_short)PORT );

            if (connect(connected_ch[connect_cnt], (struct sockaddr *)&inet_telnum,
                               sizeof(struct sockaddr_in)) == -1) {
                perror("inet_sock connect failed: ");
                exit(2);
            }
            th_arg = malloc(sizeof(TH_ARG));
            th_arg->my_chan = connected_ch[connect_cnt];
            th_arg->my_node_id = my_role;
//	   printf("\nconnected from %s to %s\n", host_list[my_role], host_list[1]);

            if ((errno = pthread_create(&thread_ids[th_index++], NULL,
                          chan_monitor, (void *)th_arg)) != 0) {
                perror("pthread_create channel monitor failed ");
                exit(3);
            }
            ++connect_cnt;

            if ((heptr = gethostbyname( host_list[2] )) == NULL) {
                perror("gethostbyname failed: ");
                exit(1);
            }
            if ((connected_ch[connect_cnt] = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
                perror("connect channel allocation failed: ");
                exit(2);
            }

            bcopy(heptr->h_addr, &inet_telnum.sin_addr, heptr->h_length);
            inet_telnum.sin_family = AF_INET;
            inet_telnum.sin_port = htons( (u_short)PORT );

            if (connect(connected_ch[connect_cnt], (struct sockaddr *)&inet_telnum,
                               sizeof(struct sockaddr_in)) == -1) {
                perror("inet_sock connect failed: ");
                exit(2);
            }
            th_arg = malloc(sizeof(TH_ARG));
            th_arg->my_chan = connected_ch[connect_cnt];
            th_arg->my_node_id = my_role;
//	   printf("\nconnected from %s to %s\n", host_list[my_role], host_list[2]);

            if ((errno = pthread_create(&thread_ids[th_index++], NULL,
                          chan_monitor, (void *)th_arg)) != 0) {
                perror("pthread_create channel monitor failed ");
                exit(3);
            }
            ++connect_cnt;

            if ((heptr = gethostbyname( host_list[3] )) == NULL) {
                perror("gethostbyname failed: ");
                exit(1);
            }
            if ((connected_ch[connect_cnt] = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
              perror("connect channel allocation failed: ");
              exit(2);
            }

            bcopy(heptr->h_addr, &inet_telnum.sin_addr, heptr->h_length);
            inet_telnum.sin_family = AF_INET;
            inet_telnum.sin_port = htons((u_short)PORT);

            if (connect(connected_ch[connect_cnt], (struct sockaddr *)&inet_telnum, sizeof(struct sockaddr_in)) == -1) {
                perror("inet_sock connect failed: ");
                exit(2);
            }
            th_arg = malloc(sizeof(TH_ARG));
            th_arg->my_chan = connected_ch[connect_cnt];
            th_arg->my_node_id = my_role;
//	   printf("\nconnected from %s to %s\n", host_list[my_role], host_list[3]);

            if ((errno = pthread_create(&thread_ids[th_index++], NULL, chan_monitor, (void *)th_arg)) != 0) {
                perror("pthread_create channel monitor failed ");
                exit(3);    
            }
            break; //  3 connects and done
    }

    sleep(5);
    if ((errno = pthread_create(&thread_ids[th_index++], NULL, msg_maker, NULL)) != 0) {
        perror("pthread_create msg_maker failed ");
        exit(3);
    }

    for (i = 0; i < (node_cnt - 1); ++i) {
        pthread_join(thread_ids[i], NULL);
    }
// Assuming 5 node example, main thread can exit here,
// full connection is constructed

    printf("\nnode controller finished, goodbye\n");
}


void*	chan_monitor(void *my_arg) {

    int  my_chan, type_val, timestamp, node_id, my_node_id;
    MSG  msg;
    MBUF raw;

    my_chan    = ((TH_ARG *)my_arg)->my_chan;
    my_node_id = ((TH_ARG *)my_arg)->my_node_id;

//	printf("\nmy_chan is %d, my node_id is %d\n", my_chan, my_node_id);


//	if (my_node_id == 3) {
        make_msg(&msg, CONNECTED, my_node_id, 0, 0);
        if (write(my_chan, &msg, (4*sizeof(int))) == -1) {
            perror("connected channel write failed: ");
            exit(3);
        }
//	}

    while(1) {
        read_msg(my_chan, &raw.buf);
        type_val  = ntohl(raw.m.mtype);
        node_id   = ntohl(raw.m.mid);
        timestamp = ntohl(raw.m.mdonut_num);
/***********************************************
      pro_node_id = ntohl(raw.m.mnode_id);
***********************************************/

/***** what type of message has the client sent to us ??     *****/

        switch(type_val) {
            case CONNECTED:
                printf("\nchannel monitor is connected to node %d\n", node_id);
                make_msg(&msg, CONN_ACK, my_node_id, 0, 0);
                if (write(my_chan, &msg, (4*sizeof(int))) == -1) {
                    perror("connected channel write failed: ");
                    exit(3);
                }

                break;
            case CONN_ACK:
                printf("\nreceived CONN_ACK from node %s\n", host_list[node_id]);
                break;	
            default:
                printf("\nchannel monitor received unknown message type: %d \n", type_val);
        }
    }
}

void*  msg_maker(void * argx) {
    int  my_chan, type_val, timestamp, node_id, my_node_id;
    int	 in_msg, ch_index;
    MSG  msg;
    MBUF raw;

    while (1) {
        printf("\nenter 0 to quit, 1 for REQUEST, 2 for REPLY, 3 for RELEASE: ");
        scanf("%d", &in_msg);

        if (!in_msg)
            exit(0);
            
        make_msg(&msg, (in_msg+20), my_node_id, 0, 0);
        ch_index = 0;
        while (connected_ch[ch_index]) {
            if (write(connected_ch[ch_index++], &msg, (4*sizeof(int))) == -1) {
                perror("connected channel write failed: ");
                exit(3);
            }
        }
    }
}
