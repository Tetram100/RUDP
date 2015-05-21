#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "event.h"
#include "rudp.h"
#include "rudp_api.h"
#include "getaddr.h"
#include "sockaddr6.h"


int receiveDataCallback(int fd, void *arg);

/* 
 * rudp_socket: Create a RUDP socket. 
 * May use a random port by setting port to zero. 
 */

rudp_socket_t rudp_socket(int port) {


	int s = socket (AF_INET, SOCK_DGRAM, 0);
	if(s==-1){
		printf("Error while pening the socket. Stop sending.\n");
		return 0;
	}

	struct sockaddr_in s_receiver;
	s_receiver.sin_family = AF_INET;
	s_receiver.sin_addr.s_addr = htonl(INADDR_ANY);
	s_receiver.sin_port = htons(port);

	if(bind(s, (struct sockaddr *) &s_receiver, sizeof(s_receiver)) == -1)
	{
		printf("Error while binding the socket\n");
		return NULL;
	}


	if(event_fd(s,receiveDataCallback, s, "receiveDataCallback") < 0) {
		printf("Error while registering the callback function of the socket.");
		return NULL;
	}

	rudp_socket_t rudp_socket = (rudp_socket_t)s;

	return rudp_socket;
}

/* 
 *rudp_close: Close socket 
 */ 

int rudp_close(rudp_socket_t rsocket) {
	return 0;
}

/* 
 *rudp_recvfrom_handler: Register receive callback function 
 */ 

int rudp_recvfrom_handler(rudp_socket_t rsocket, 
			  int (*handler)(rudp_socket_t, struct sockaddr_in6 *, 
					 void *, int)) {
	return 0;
}

/* 
 *rudp_event_handler: Register event handler callback function 
 */ 
int rudp_event_handler(rudp_socket_t rsocket, 
		       int (*handler)(rudp_socket_t, rudp_event_t, 
				      struct sockaddr_in6 *)) {
	return 0;
}


/* 
 * rudp_sendto: Send a block of data to the receiver. 
 */

int rudp_sendto(rudp_socket_t rsocket, void* data, int len, struct sockaddr_in6* to) {
	return 0;
}

int receiveDataCallback(int fd, void *arg) {

}