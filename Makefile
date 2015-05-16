CC = gcc
CFLAGS = -g -Wall

all: vs_send vs_recv

vs_send: vs_send.o rudp.o event.o getaddr.o sockaddr6.o
	$(CC) $(CFLAGS) $^ -o $@

vs_recv: vs_recv.o rudp.o event.o getaddr.o sockaddr6.o
	$(CC) $(CFLAGS) $^ -o $@

vs_send.o vs_recv.o rudp.o: rudp.h rudp_api.h event.h getaddr.h sockaddr6.h

getaddr.o: getaddr.h

sockaddr6.o: sockaddr6.h

event.o: event.h

rudp.tar: vs_send.c vs_recv.c vsftp.h Makefile rudp_api.h rudp.h event.h \
	event.c rudp.c getaddr.c getaddr.h sockaddr6.c sockaddr6.h
	tar cf rudp.tar $^

clean:
	/bin/rm -f vs_send vs_recv *.o rudp.tar
