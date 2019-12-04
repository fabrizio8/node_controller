/*   msg_util.c  minimal buffer manager helper functions    */

#include "buf_mgr.h"
#include "ddonuts.h"

// fill message struct with network byte order ints

void make_msg(MSG *message_ptr, int type, int id, int donut_num, int node_id)
{
    message_ptr->mid        = htonl(id);
    message_ptr->mtype      = htonl(type);
    message_ptr->mdonut_num = htonl(donut_num);
    message_ptr->mnode_id   = htonl(node_id);
}

// read message struct size bytes into message, byte by byte

void read_msg(int socket, char* buffer)
{
    int i, x;
    
    for (i = 0; i < (4*sizeof(int)); i++) {
        if ((x = read(socket, buffer+i, 1)) == -1) {
            perror("read_msg failed: ");
            exit(3);
        }

        if (!x) {
            printf("\nEOF on socket, goodbye\n");
            exit(4);
        }
    }
}
