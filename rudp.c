#include <math.h>
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

#define LISTEN 0
#define SYN_SENT 1
#define DATA_TRANSFER 2
#define WAIT_BUFFER 3
#define WAIT_FIN_ACK 4
#define CLOSED 5

struct rudp_packet_t {
    struct rudp_hdr header;
    char data[RUDP_MAXPKTSIZE];
} __attribute__((packed));

struct send_packet {
    struct rudp_packet_t rudp_packet;
    int len;
    int counter;
};


int receivePacketCallback(int fd, void *arg);

// TODO Question : est-ce que avoir des variables globales suffit dans notre cas ? -> on n'est censé ne gérer qu'une seule socket par programme.
//		utililsation d'un flag qui dit si une fonction à déjà été enregistré pour ce programme et du coup refuse d'en changer? 
// ---> mieux : flag pour dire qu'une socket à déjà était ouverte, et du coup on renvoit un NULL
int (*handler_receive)(rudp_socket_t, struct sockaddr_in6 *, void *, int);
int (*handler_event)(rudp_socket_t, rudp_event_t, struct sockaddr_in6 *);
int socket_open = 0;
int receive_set = 0;
int event_set = 0;

int state = 0;
int sequence_number = 0;
int window = RUDP_WINDOW;
struct sockaddr_in6* destination;

/* 
 * rudp_socket: Create a RUDP socket. 
 * May use a random port by setting port to zero. 
 */

rudp_socket_t rudp_socket(int port) {

	if(socket_open != 1){
		int s = socket (AF_INET, SOCK_DGRAM, 0);
		if(s==-1){
			printf("Error while opening the socket. Stop sending.\n");
			return NULL;
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


		if(event_fd(s, receivePacketCallback, s, "receivePacketCallback") < 0) {
			printf("Error while registering the callback function of the socket.\n");
			return NULL;
		}

		// State of the socket set at LISTEN
		state = LISTEN;

		rudp_socket_t rudp_socket = (rudp_socket_t) s;

		socket_open = 1;

		return rudp_socket;
	}
	else{
		printf("A socket has already been opened\n");
		return NULL;
	}
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
	if(receive_set != 1){
		handler_receive = handler;
		receive_set = 1;
		return 0;
	}
	else{
		return 1;
	}
	
}

/* 
 *rudp_event_handler: Register event handler callback function 
 */ 
int rudp_event_handler(rudp_socket_t rsocket, 
		       int (*handler)(rudp_socket_t, rudp_event_t, 
				      struct sockaddr_in6 *)) {
	if(event_set != 1){
		handler_event = handler;
		event_set = 1;
		return 0;
	}
	return 1;
}


/* 
 * rudp_sendto: Send a block of data to the receiver. 
 */

int rudp_sendto(rudp_socket_t rsocket, void* data, int len, struct sockaddr_in6* to) {
	
	if (len > RUDP_MAXPKTSIZE) {
	    printf("Packet size is too big\n");
    	return -1;	
	}

	if (state == CLOSED || state == WAIT_BUFFER || WAIT_FIN_ACK){
	    printf("Try to send packet in a wrong state\n");
    	return -1;			
	} else if (state == LISTEN){
		struct rudp_packet_t syn_packet;
		syn_packet.header.version = RUDP_VERSION;
		syn_packet.header.type = RUDP_SYN;
		//Plus 1 to avoid a 0 sequence number
		sequence_number = (rand() % (int)pow(2,32)) +1;
		syn_packet.header.seqno = sequence_number;
		destination = to;

		window = window - 1;
        if (sendto((int) rsocket, (void *) &syn_packet, sizeof (struct rudp_packet_t), 0, (struct sockaddr_in6*) &to, sizeof (struct sockaddr_in6)) < 0) {
            printf("Failed to send SYN packet\n");
            return -1;
        }
        state = SYN_SENT;
	}

	//We create the packet
	struct rudp_packet_t data_packet;
	data_packet.header.version = RUDP_VERSION;
	data_packet.header.type = RUDP_DATA;
	memcpy(data_packet.data, data, len);

	//We save the packet
	struct send_packet packet;
	packet.rudp_packet = data_packet;
	packet.len = len;
	packet.counter = 0;

	if (window > 0) {
		send_packet(rsocket, packet);
	} else {
		//TODO ajouter le packet à la liste des packets à envoyer
	}

	return 0;
}

int send_packet(rudp_socket_t rsocket, struct send_packet packet){
	window = window -1;
	if (sendto((int) rsocket, (void *) &packet, packet.len, 0, (struct sockaddr_in6*) destination, sizeof (struct sockaddr_in6)) < 0) {
		printf("Failed to send SYN packet\n");
		return -1;
	}
	packet.counter++;
	setTimeOut(packet);
	return 0;	
}

int retransmit(int fd, void *arg){

	return 0;
}

int setTimeOut(struct send_packet packet){
	struct timeval time_out, t1, t2;
	time_out.tv_sec = RUDP_TIMEOUT/1000;
	time_out.tv_usec = (RUDP_TIMEOUT % 1000) * 1000;
	gettimeofday(&t1, NULL);  
	timeradd(&t1, &time_out, &t2);

	if(event_timeout(t2, &retransmit, &packet, "timer_callback") == -1) {
		printf("Error while programming the time out callback\n");
		return -1;
	}

	return 0;
}

int receivePacketCallback(int fd, void *arg) {

	rudp_socket_t *rudp_socket = (rudp_socket_t*) arg;

	struct sockaddr_in sender;
	int addr_size = sizeof(sender);

	struct rudp_packet_t rudp_receive;
	memset(&rudp_receive, 0x0, sizeof(struct rudp_packet_t));

    int bytes = recvfrom(fd, &rudp_receive, sizeof (rudp_receive), 0, (struct sockaddr*) &sender, (socklen_t*) & addr_size);

    //Verifications
    if (bytes <=0){
    	printf("Error while receiving the data\n");
    	return -1;
    }
    if (rudp_receive.header.version != RUDP_VERSION){
    	printf("Wrong RUDP version\n");
    	return -1;    	
    }

    switch(rudp_receive.header.type) {
    	case RUDP_DATA:

    		return;

    	case RUDP_ACK:

    		return;

    	case RUDP_SYN:

    		return;

    	case RUDP_FIN:

    		return;

    }
}