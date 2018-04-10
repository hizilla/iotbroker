objs = debug.o memmanager.o message.o subtree.o  session.o packet_handle.o  protocol.o net.o  main.o
CC = gcc
CFLAGS = -rdynamic -g 
LDFLAGS = -ldd

all: $(objs)
	$(CC) -o main $(objs)

$(objs): %.o:%.c
	$(CC) -c $(CFLAGS) $< -o $@

.PHONY:
clean:
	-rm ./*.o
	-rm main
