


ndc:	node_controller.o msg_util.o
	gcc -o ndc node_controller.o msg_util.o -lpthread
node_controller.o: node_controller.c buf_mgr.h ddonuts.h
	gcc -g -c node_controller.c
msg_util.o: msg_util.c buf_mgr.h ddonuts.h
	gcc -g -c msg_util.c
clean:	
	rm -f ndc *.o
