CC=clang
CFLAGS=-Wall -Werror -std=gnu99 -D_GNU_SOURCE -pthread -g

BIN = serverThreaded
OBJS = main.o get_listen_socket.o service_client_socket.o service_listen_socket_multithread.o make_printable_address.o

all: ${BIN}

serverThreaded: ${OBJS}
	${CC} -o $@ ${CFLAGS} $+ 

clean: 
	rm -f ${BIN} ${OBJS} *~
	
