/** net_consumer.c  -  for distributed donuts process implementations **/

#include "ddonuts.h"
#include "buf_mgr.h"


int	main(int argc, char *argv[])
{

	int		  i,j,k,nsigs;
    int     inet_sock, local_file, donut_num, node_id;
    int     type_val, id_val, read_val, local_size, my_id;
    char    *buffer_ptr, *token_ptr, *last_token_ptr;
    char    full_file_path_name[256];
    MSG     msg;
    MBUF    raw;
    union   type_size;
	int     pro_id_val, pro_node_id;
    struct  sockaddr_in inet_telnum;
    struct  hostent *heptr, *gethostbyname();
    struct  timeval 		randtime;
	unsigned short 		xsub1[3];

	if (argc < 4){
	  printf("\nUSAGE: net_consumer BM_host_name cons_id node_id\n");
	  exit(2);
	}

	my_id   = atoi(argv[2]);
	node_id = atoi(argv[3]);

    if ((heptr = gethostbyname( argv[1] )) == NULL){
        perror("gethostbyname failed: ");
        exit(1);
    }

    bcopy(heptr->h_addr, &inet_telnum.sin_addr, heptr->h_length);
    inet_telnum.sin_family = AF_INET;
    inet_telnum.sin_port = htons( (u_short)PORT );

    gettimeofday(&randtime, NULL);

    xsub1[0] = (ushort)randtime.tv_usec;
    xsub1[1] = (ushort)(randtime.tv_usec >> 16);
    xsub1[2] = (ushort)(getpid());

	donut_num = 0;
	printf("\nstarting consumer %d on node %d\n", my_id, node_id);

// get 10 dozen donuts for testing

	for (i = 0; i < 10; i++) {
	    for (k = 0; k < 12; ++k) {
	        j = nrand48(xsub1) & 3;

        if ((inet_sock=socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            perror("inet_sock allocation failed: ");
            exit(1);
        }

	    if (connect(inet_sock, (struct sockaddr *)&inet_telnum, sizeof(struct sockaddr_in)) == -1) {
            perror("inet_sock connect failed: ");
            exit(2);
        }

	    make_msg(&msg, j+10, my_id, donut_num, node_id);

        if (write(inet_sock, &msg, (4*sizeof(int))) == -1) {
            perror("inet_sock write failed: ");
            exit(3);
        }

        read_msg(inet_sock, &raw.buf);

        type_val    = ntohl(raw.m.mtype);
        pro_id_val  = ntohl(raw.m.mid);
	    donut_num   = ntohl(raw.m.mdonut_num);
	    pro_node_id = ntohl(raw.m.mnode_id);

// write stdout with basic donut info

        switch(j) {
            case JELLY:
                printf("consumer %d on node %d received JELLY donut  %d  from producer %d on node %d\n", my_id, node_id, donut_num, pro_id_val, pro_node_id );
                break;
            case PLAIN:
                printf("consumer %d on node %d received PLAIN donut  %d  from producer %d on node %d\n", my_id, node_id, donut_num, pro_id_val, pro_node_id );
                break;
            case COCO:
                printf("consumer %d on node %d received COCON donut  %d  from producer %d on node %d\n", my_id, node_id, donut_num, pro_id_val, pro_node_id );
                break;
            case CREAM:
                printf("consumer %d on node %d received CREAM donut  %d  from producer %d on node %d\n", my_id, node_id, donut_num, pro_id_val, pro_node_id );
                break;
        } // flavor switch

        if (type_val != C_ACK)
            printf("\nBAD REPLY FROM BUFFER MANAGER\n");
	    close(inet_sock);
    } // each dozen

// sleep to force context switch between dozens
	  usleep(100);
	}
}
