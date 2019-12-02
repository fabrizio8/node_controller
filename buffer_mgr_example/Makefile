all:    bm np nc
	gcc -o bm buf_mgr_server.o msg_util.o utilities.o
	gcc -o np net_producer.o msg_util.o utilities.o
	gcc -o nc net_consumer.o msg_util.o utilities.o
bm:     buf_mgr_server.o msg_util.o utilities.o
	gcc -o bm buf_mgr_server.o msg_util.o utilities.o
np:     net_producer.o msg_util.o utilities.o
	gcc -o bm buf_mgr_server.o msg_util.o utilities.o
nc:     net_consumer.o msg_util.o utilities.o
	gcc -o nc net_consumer.o msg_util.o utilities.o
buf_mgr_server.o: buf_mgr_server.c buf_mgr.h ddonuts.h
	gcc -g -c buf_mgr_server.c
net_producer.o: net_producer.c buf_mgr.h ddonuts.h
	gcc -g -c net_producer.c
net_consumer.o: net_consumer.c buf_mgr.h ddonuts.h
	gcc -g -c net_consumer.c
msg_util.o: msg_util.c buf_mgr.h ddonuts.h
	gcc -g -c msg_util.c
utilities.o: utilities.c buf_mgr.h ddonuts.h
	gcc -g -c utilities.c
clean:	
	rm -f bm np nc *.o
