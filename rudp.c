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
#include <time.h>

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

#define NUM_SEQ_MAX 4294967295

struct rudp_packet_t {
	struct rudp_hdr header;
	char data[RUDP_MAXPKTSIZE];
} __attribute__((packed));

struct send_packet {
	struct rudp_packet_t rudp_packet;
	rudp_socket_t rudp_socket;
	int len;
	int counter;
};

struct list_packet{
	struct send_packet packet;
	struct list_packet *next_packet;
};

/*
 * Prototypes
 */

int receivePacketCallback(int fd, void *arg);
int send_packet(rudp_socket_t rsocket, struct send_packet* packet);

// We use a different function to send ACK packets (no timeout triggered).
int send_ack(rudp_socket_t rsocket, struct send_packet* packet_ack);

int setTimeOut(struct send_packet* packet);
int retransmit(void *arg);
int receive_DATA(rudp_socket_t rudp_socket, struct rudp_packet_t rudp_receive, int bytes);
int receive_ACK(rudp_socket_t rudp_socket, struct rudp_packet_t rudp_receive);
int receive_SYN(rudp_socket_t rudp_socket, struct rudp_packet_t rudp_receive, struct sockaddr_in6* sender);
int receive_FIN(rudp_socket_t rudp_socket, struct rudp_packet_t rudp_receive);
int send_buffer(rudp_socket_t rsocket);
int send_fin(rudp_socket_t rsocket);
int reset_parameter();

struct list_packet* add_list(struct list_packet *list, struct send_packet packet_to_send);
struct list_packet* remove_head_list(struct list_packet *list);
int get_number_packets_acked(u_int32_t ack_numb_received);
struct list_packet* insert_list_seq(struct list_packet *list, struct send_packet packet_received);
struct send_packet* point_end_list(struct list_packet *list);

u_int32_t get_actual_seq(u_int32_t relative_seq);
u_int32_t get_actual_ack(u_int32_t relative_ack);
u_int32_t get_relative_seq(u_int32_t actual_seq);
u_int32_t get_relative_ack(u_int32_t actual_ack);

/*
 * Global variables
 */

int (*handler_receive)(rudp_socket_t, struct sockaddr_in6 *, void *, int);
int (*handler_event)(rudp_socket_t, rudp_event_t, struct sockaddr_in6 *);
int socket_open = 0;
int receive_set = 0;
int event_set = 0;
int s = -1;
int close_ask = 0; 

int state = LISTEN;
u_int32_t sequence_number = 0;
u_int32_t ack_number =0;
int window = RUDP_WINDOW;
struct sockaddr_in6* destination = NULL;

u_int32_t initial_seq_number = 0;
u_int32_t initial_ack_number = 0;

// Packets waiting to be send to the receiver.
struct list_packet *list_to_send = NULL;
int numb_packet_to_send = 0;

// Packets sent, waiting to be acked by the receiver.
struct list_packet *list_waiting_ack = NULL;

// Packets received, but with a too big seq number, waiting for the missing packet to arrive.
struct list_packet *list_buffer_to_app = NULL;


int reset_parameter() {

	handler_receive = NULL;
	handler_event = NULL;
	socket_open = 0;
	receive_set = 0;
	event_set = 0;
	
	close_ask = 0;

	state = 0;
	sequence_number = 0;
	ack_number =0;
	window = RUDP_WINDOW;
	destination = NULL;

	initial_seq_number = 0;
	initial_ack_number = 0;

	// Packets waiting to be send to the receiver.
	list_to_send = NULL;
	numb_packet_to_send = 0;

	// Packets sent, waiting to be acked by the receiver.
	list_waiting_ack = NULL;

	// Packets received, but with a too big seq number, waiting for the missing packet to arrive.
	list_buffer_to_app = NULL;

	return 0;
}

/* 
 * rudp_socket: Create a RUDP socket. 
 * May use a random port by setting port to zero. 
 */
rudp_socket_t rudp_socket(int port) {

	reset_parameter();

 	if(socket_open != 1){
 		s = socket(AF_INET6, SOCK_DGRAM, 0);
 		if(s==-1){
 			printf("Error while opening the socket. Stop sending.\n");
 			return NULL;
 		}

 		struct sockaddr_in6 s_receiver;

 		s_receiver.sin6_family = AF_INET6;
 		s_receiver.sin6_flowinfo = 0;
 		// s_receiver.sin6_addr.s6_addr = htonl(INADDR_ANY);
 		s_receiver.sin6_addr = in6addr_any;
 		s_receiver.sin6_port = htons(port);

 		if(bind(s, (struct sockaddr *) &s_receiver, sizeof(s_receiver)) == -1)
 		{
 			printf("Error while binding the socket\n");
 			return NULL;
 		}


 		if(event_fd(s, receivePacketCallback, (void*) &s, "receivePacketCallback") < 0) {
 			printf("Error while registering the callback function of the socket.\n");
 			return NULL;
 		}

		// State of the socket set at LISTEN
 		state = LISTEN;

 		socket_open = 1;
 		printf("Socket is opened\n");
 		return &s;
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
	printf("close called from the program\n");
 	if ( (state == DATA_TRANSFER) &&  (list_waiting_ack == NULL) && (list_to_send == NULL)){
 		send_fin(rsocket);
 	} else if (state == DATA_TRANSFER){
 		state = WAIT_BUFFER;
 	} else {
 		close_ask = 1;
 	}
 	return 0;
}

int send_fin(rudp_socket_t rsocket) {
	
	struct rudp_packet_t fin_packet;
	struct send_packet packet;;
	fin_packet.header.version = RUDP_VERSION;
	fin_packet.header.type = RUDP_FIN;
	sequence_number = (sequence_number + 1)%NUM_SEQ_MAX;
	fin_packet.header.seqno = get_actual_seq(sequence_number);

	packet.rudp_packet = fin_packet;
	packet.rudp_socket = rsocket;
	packet.len = sizeof (struct rudp_hdr);
	packet.counter = 0;

	list_waiting_ack = add_list(list_waiting_ack, packet);
		
	send_packet(rsocket, &(list_waiting_ack->packet));
	
	printf("FIN sent\n");	
	state = WAIT_FIN_ACK;

	return 0;
}


/* 
 *rudp_recvfrom_handler: Register receive callback function 
 */ 

int rudp_recvfrom_handler(rudp_socket_t rsocket, 
 	int (*handler)(rudp_socket_t, struct sockaddr_in6 *, 
 		void *, int)) {
	printf("Recvfrom Handler set\n");
 	if(receive_set != 1){
 		handler_receive = handler;
 		receive_set = 1;
 		return 0;
 	}
 	else{
 		printf("Recvfrom Handler already set\n");	
 	}
 	return 0;
}

/* 
 *rudp_event_handler: Register event handler callback function 
 */

int rudp_event_handler(rudp_socket_t rsocket, 
 	int (*handler)(rudp_socket_t, rudp_event_t, 
 		struct sockaddr_in6 *)) {
	printf("Event Handler set\n");
 	if(event_set != 1){
 		handler_event = handler;
 		event_set = 1;
 		return 0;
 	}
 	else{
 		printf("Event Handler already set\n");
 	}
 	return 0;
}

/* 
 * rudp_sendto: Send a block of data to the receiver. 
 */

int rudp_sendto(rudp_socket_t rsocket, void* data, int len, struct sockaddr_in6* to) {

 	if (len > RUDP_MAXPKTSIZE) {
 		printf("Packet size is too big\n");
 		return -1;	
 	}

 	if (state == CLOSED || state == WAIT_BUFFER || state == WAIT_FIN_ACK){
 		printf("Try to send packet in a wrong state\n");
 		return -1;			
 	}

 	// The first time sendto is called we send a SYN packet.
 	if (state == LISTEN){
 		struct rudp_packet_t syn_packet;
 		syn_packet.header.version = RUDP_VERSION;
 		syn_packet.header.type = RUDP_SYN;
		// Plus 1 to avoid a 0 sequence number
 		initial_seq_number = (u_int32_t) (rand() % NUM_SEQ_MAX) + 1;
 		sequence_number = 0;
 		syn_packet.header.seqno = initial_seq_number;
 		destination = to;

 		struct send_packet syn_packet_send;
  		syn_packet_send.rudp_packet = syn_packet;
 		syn_packet_send.rudp_socket = rsocket;
 		syn_packet_send.len = sizeof (struct rudp_hdr);
 		syn_packet_send.counter = 0;

 		printf("Packet added to the list_waiting_ack\n");
 		list_waiting_ack = add_list(list_waiting_ack, syn_packet_send);	

 		send_packet(rsocket, &(list_waiting_ack->packet));
 		printf("SYN sent\n");
 		state = SYN_SENT;
 	}

	// We create the packet
 	struct rudp_packet_t data_packet;
 	data_packet.header.version = (RUDP_VERSION);
 	data_packet.header.type = (RUDP_DATA);

 	sequence_number = (sequence_number + 1)%NUM_SEQ_MAX;
 	data_packet.header.seqno = (get_actual_seq(sequence_number));

 	memcpy(data_packet.data, data, len);

	// We save the packet
 	struct send_packet packet;
 	packet.rudp_packet = data_packet;
 	packet.rudp_socket = rsocket;
 	packet.len = len + sizeof (struct rudp_hdr);
 	packet.counter = 0;

 	printf("Packed added to the list_to_send\n");
 	// We store the packet in the sending buffer. The function which sends the packet will be called each time we receive an ACK.
 	list_to_send = add_list(list_to_send, packet);

 	// If in DATA_TRANSFER state, we use send_buffer, in case send_to is called after all ack have already been received.
 	if(state == DATA_TRANSFER){
 		send_buffer(rsocket);
 	}

 	return 0;
}

int send_packet(rudp_socket_t rsocket, struct send_packet* packet){
 	if (sendto(*((int*)rsocket), (void *) &packet->rudp_packet, packet->len, 0, (struct sockaddr*) destination, sizeof(struct sockaddr_in6)) < 0) {
 		perror("sendto failed");
 		printf("Failed to send packet\n");
 		return -1;
 	}
 	packet->counter++;
 	if (setTimeOut(packet) != 0){
 		return -1;
 	}
 	return 0;	
}

int send_ack(rudp_socket_t rsocket, struct send_packet* packet_ack){
 	if (sendto(*((int*)rsocket), (void *) &packet_ack->rudp_packet, packet_ack->len, 0, (struct sockaddr*) destination, sizeof(struct sockaddr_in6)) < 0) {
 		printf("Failed to send packet\n");
 		return -1;
 	}
 	printf("Ack sent\n");
 	return 0;
}

// This function is called when an ack is received. It checks if there are packets in the sending buffer, and sends as many packets as possible,
// until the sending window is empty or there are no more packet in the buffer.
int send_buffer(rudp_socket_t rsocket){
	printf("Sending of the buffer\n");
 	while(list_to_send != NULL && window > 0){
 		list_waiting_ack = add_list(list_waiting_ack, list_to_send->packet);
 		send_packet(rsocket, point_end_list(list_waiting_ack));
 		printf("Packet sent\n");
 		printf("Packet added to the list_waiting_ack\n");
 		list_to_send = remove_head_list(list_to_send);
 		window = window - 1;
 	}
 	return 0;
}

int retransmit(void *arg){

 	struct send_packet* packet = (struct send_packet*) arg;

 	if (packet->counter >= RUDP_MAXRETRANS){
 		handler_event(packet->rudp_socket, RUDP_EVENT_TIMEOUT, destination);
 	} else {
 		send_packet(packet->rudp_socket, packet);
 		printf("Timeout expired. Packet retransmit\n");
 	}

 	return 0;
}

int setTimeOut(struct send_packet* packet){

	if(event_timeout(RUDP_TIMEOUT, &retransmit, packet, "timer_callback") == -1) {
		printf("Error while programming the time out callback\n");
		return -1;
	}
	printf("Timeout set\n");
	return 0;
}

int receivePacketCallback(int fd, void *arg) {

	rudp_socket_t rudp_socket = (rudp_socket_t) arg;

	struct sockaddr_in6 sender;
	int addr_size = sizeof(sender);

	struct rudp_packet_t rudp_receive;
	memset(&rudp_receive, 0x0, sizeof(struct rudp_packet_t));

	int bytes = recvfrom((int) fd, (void*)&rudp_receive, sizeof (rudp_receive), 0, (struct sockaddr*) &sender, (socklen_t*) &addr_size);

    //Verifications

    if(state != LISTEN){ 		    
    	if(getnameinfohost(destination) != getnameinfohost(&sender) || destination->sin6_port != sender.sin6_port){
    		printf("%d\n", sockaddr6_cmp(&sender, destination));
    		printf("Receive a packet from an unexpected source\n");
    		return 0;
    	}
    }
	if (bytes <=0){
		printf("Error while receiving the data\n");
		return 0;
	}
	if (rudp_receive.header.version != RUDP_VERSION){
		printf("Wrong RUDP version\n");
		return 0;    	
	}

	switch(rudp_receive.header.type) {
		case RUDP_DATA:
			printf("DATA received\n");
    		receive_DATA(rudp_socket, rudp_receive, bytes);
		return 0;

		case RUDP_ACK:
			printf("ACK received\n");
    		receive_ACK(rudp_socket, rudp_receive);
		return 0;

		case RUDP_SYN:
			printf("SYN received\n");
			receive_SYN(rudp_socket, rudp_receive, &sender);
		return 0;

		case RUDP_FIN:
			printf("FIN received\n");
			receive_FIN(rudp_socket, rudp_receive);
		return 0;

		default:
			printf("Wrong Type packet\n");
			return 0;
	}
}

/*
 * Functions to call depending on the type of the received packet.
 */

int receive_SYN(rudp_socket_t rudp_socket, struct rudp_packet_t rudp_receive, struct sockaddr_in6* sender){

 	struct rudp_packet_t ack_packet;
 	struct send_packet packet;

 	switch(state){
 		case LISTEN:
 			destination = sender;

			// send ACK
 			ack_packet.header.version = RUDP_VERSION;
 			ack_packet.header.type = RUDP_ACK;

 			ack_number = 0;
 			sequence_number = 0; // in case we need to send data as well.

 			initial_seq_number = ((rudp_receive.header.seqno) + 1)%NUM_SEQ_MAX;
 			initial_ack_number = (u_int32_t) (rand() % NUM_SEQ_MAX) + 1;

 			ack_packet.header.seqno = get_actual_ack(ack_number);

 			packet.rudp_packet = ack_packet;
 			packet.rudp_socket = rudp_socket;
 			packet.len = sizeof (struct rudp_hdr);
 			packet.counter = 0;

 			send_ack(rudp_socket, &packet);

 			state = DATA_TRANSFER;

 			return 0;

 		case DATA_TRANSFER:
 			ack_packet.header.version = RUDP_VERSION;
 			ack_packet.header.type = RUDP_ACK;
 			ack_packet.header.seqno = get_actual_ack(ack_number);

 			packet.rudp_packet = ack_packet;
 			packet.rudp_socket = rudp_socket;
 			packet.len = sizeof (struct rudp_hdr);
 			packet.counter = 0;

 			send_ack(rudp_socket, &packet);
 			return 0;

 		default:
 			printf("Receive a SYN packet unexpectedly.\n");
 			return -1;
 	}
}

int receive_DATA(rudp_socket_t rudp_socket, struct rudp_packet_t rudp_receive, int bytes){

 	struct rudp_packet_t ack_packet;
 	struct send_packet packet;
 	u_int32_t seqno_rec_relative = get_relative_ack(rudp_receive.header.seqno);

 	switch(state){
 		case DATA_TRANSFER:
 			// send ack, update ack number, make the difference between the packet we have already received (just resend the ack with the actual ack) and the others,
 			// send the packet to the application through the handler function, store and reorganize if wrong order.

 			if(seqno_rec_relative < ack_number){
 				// We have already received the packet. Acknowledge with the current ack_number.
 				ack_packet.header.version = RUDP_VERSION;
 				ack_packet.header.type = RUDP_ACK;
 				ack_packet.header.seqno = get_actual_ack(ack_number);
 				
 				packet.rudp_packet = ack_packet;
 				packet.rudp_socket = rudp_socket;
 				packet.len = sizeof (struct rudp_hdr);
 				packet.counter = 0;

 				send_ack(rudp_socket, &packet);
 				return 0;
 			}

 			if(seqno_rec_relative == ack_number){
 				// The packet is the one we expected next. We check if the buffer for arriving packet is empty. If not, we send all the packets
 				// which seq numbers follows each others to the application. We ack the last packet sent to the application.

 				// Send the packet we just received to the application.
 				handler_receive(rudp_socket, destination, (void*) rudp_receive.data, bytes - (int) sizeof(struct rudp_hdr));
 				ack_number = (ack_number+1)%NUM_SEQ_MAX;

 				if(list_buffer_to_app != NULL){

 					struct list_packet* temp = list_buffer_to_app;

 					while(temp != NULL && (((temp->packet).rudp_packet).header).seqno == get_actual_ack(ack_number)){
 						handler_receive(rudp_socket, destination, (void*) temp->packet.rudp_packet.data, temp->packet.len - (int) sizeof(struct rudp_hdr));
 						ack_number = (ack_number+1)%NUM_SEQ_MAX;
 						temp = temp->next_packet;
 						list_buffer_to_app = remove_head_list(list_buffer_to_app);
 					}
 				}
 				ack_packet.header.version = (RUDP_VERSION);
	 			ack_packet.header.type = (RUDP_ACK);
	 			ack_packet.header.seqno = get_actual_ack(ack_number);

	 			packet.rudp_packet = ack_packet;
	 			packet.rudp_socket = rudp_socket;
	 			packet.len = sizeof (struct rudp_hdr);
	 			packet.counter = 0;

	 			send_ack(rudp_socket, &packet);	
 				return 0;
 			}

 			if(seqno_rec_relative > ack_number){
 				// Put the packet in the buffer list, resend the ack of the packet we are expecting. We can't send packet to the applicatoin yet.
 				struct send_packet packet_to_insert;
 				packet_to_insert.rudp_packet = rudp_receive;
 				packet_to_insert.rudp_socket = NULL;

 				// The length parameter is size of rudp_header + size of data.
 				packet_to_insert.len = bytes;
 				packet_to_insert.counter = 0;

 				insert_list_seq(list_buffer_to_app, packet_to_insert);

 				ack_packet.header.version = (RUDP_VERSION);
 				ack_packet.header.type = (RUDP_ACK);
 				ack_packet.header.seqno = get_actual_ack(ack_number);

 				struct send_packet packet;
 				packet.rudp_packet = ack_packet;
 				packet.rudp_socket = rudp_socket;
 				packet.len = sizeof (struct rudp_hdr);
 				packet.counter = 0;

 				send_ack(rudp_socket, &packet);
 				return 0;
 			}

 			return 0;

 		default:
 			printf("Receive a DATA packet unexpectedly.\n");
 			return -1;
 	}
}

int receive_ACK(rudp_socket_t rudp_socket, struct rudp_packet_t rudp_receive){

	u_int32_t seq_wait_relative = get_relative_seq((((list_waiting_ack->packet).rudp_packet).header).seqno);

 	switch(state){
 		case SYN_SENT:
			
			
			//TODO expression bien sale dans le if, à vérifier qu'il n'y ait pas d'erreur de syntaxe.

 			if(get_relative_seq(rudp_receive.header.seqno) == seq_wait_relative + 1){
 				// Delete the timeout for the SYN packet.
 				event_timeout_delete(&retransmit, &(list_waiting_ack->packet));
 				list_waiting_ack = remove_head_list(list_waiting_ack);
 				state = DATA_TRANSFER;
 				// If the closure has been ask
 				if ( close_ask == 1){
 					state = WAIT_BUFFER;
 				}
 				// Send as many packets as possible (if packets are wainting + window > 0).
 				send_buffer(rudp_socket);

 				return 0;
 			}
 			else{
 				printf("Receive an ACK packet with wrong seq number.\n");
 				return -1;
 			}
 			return -1;

 		case DATA_TRANSFER:
 		case WAIT_BUFFER:
			// changer la valeur de window. déclencher l'envoi des paquets dans le buffer.
			// Remove the timeout event for the packets ack
 			if(get_relative_seq(rudp_receive.header.seqno) < seq_wait_relative + 1){
 				printf("Receive an ACK packet with an old seq number.\n");
 				return 0;
 			}
 			
 			if(get_relative_seq(rudp_receive.header.seqno) == seq_wait_relative + 1){
 				event_timeout_delete(&retransmit, &(list_waiting_ack->packet));
 				list_waiting_ack = remove_head_list(list_waiting_ack);
 				if(window < RUDP_WINDOW){
 					window += 1;
 				}
 				send_buffer(rudp_socket);
 				if (state == WAIT_BUFFER && list_waiting_ack == NULL){
 					// We send the FIN packet
					send_fin(rudp_socket);				
 				}
 				return 0;
 			}
 			
 			if(get_relative_seq(rudp_receive.header.seqno) > seq_wait_relative + 1){
 				int numb = get_number_packets_acked(rudp_receive.header.seqno);
 				int window_temp = window;

 				while(numb > 0){
 					list_waiting_ack = remove_head_list(list_waiting_ack);
 					window_temp += 1;
 					numb -= 1;
 				}
 				if(window_temp > RUDP_WINDOW){
 					window = RUDP_WINDOW;
 				}
 				else{
 					window = window_temp;
 				}
 				send_buffer(rudp_socket);
 				if (state == WAIT_BUFFER && list_waiting_ack == NULL){
 					// We send the FIN packet
 					send_fin(rudp_socket);			
 				}
 				return 0;
 			} 
 			return -1;

 		case WAIT_FIN_ACK:
 			if(rudp_receive.header.seqno == (((list_waiting_ack->packet).rudp_packet).header).seqno){
 				event_timeout_delete(&retransmit, &(list_waiting_ack->packet));
 				list_waiting_ack = remove_head_list(list_waiting_ack);
 				state = CLOSED;
 				handler_event(rudp_socket, RUDP_EVENT_CLOSED, NULL);
 			}
 			return 0;

 		default:
 			printf("Receive an ACK packet unexpectedly.\n");
 			return -1;
 	}
}

int receive_FIN(rudp_socket_t rudp_socket, struct rudp_packet_t rudp_receive){
 	struct rudp_packet_t ack_packet;
 	struct send_packet packet;
 	switch(state){
 		case DATA_TRANSFER:

 			if(rudp_receive.header.seqno == get_actual_ack(ack_number)){

				// send ACK
	 			ack_packet.header.version = RUDP_VERSION;
	 			ack_packet.header.type = RUDP_ACK;

				// The sequence number of the packet which ACK a packet FIN is the same as the last received data packet
				// (the same as the FIN packet received).
	 			ack_number = (rudp_receive.header.seqno);

	 			ack_packet.header.seqno = rudp_receive.header.seqno;

	 			packet.rudp_packet = ack_packet;
	 			packet.rudp_socket = rudp_socket;
	 			packet.len = sizeof (struct rudp_hdr);
	 			packet.counter = 0;

	 			send_ack(rudp_socket, &packet);

				// TODO faut-il faire plus que ça avant de passer dans l'état closed ?
	 			state = CLOSED;
	 			// We reset the parameters to be ready to accept a new connection
	 			reset_parameter();			
	 			return 0;
	 		}else{
	 			printf("FIN with wrong seq number");
	 			return -1;
	 		}

 		default:
 			printf("Receive a FIN packet unexpectedly.\n");
 			return -1;
 	}
}


/*
 * Functions to manipulate list_packet
 */

// Add the packet at the end of the list.

struct list_packet* add_list(struct list_packet *list, struct send_packet packet_to_send){
 	struct list_packet *new_element = malloc(sizeof(struct list_packet));

 	new_element -> packet = packet_to_send;
 	new_element -> next_packet = NULL;

 	if(list == NULL){
 		list = new_element;
 	}
 	else{
 		struct list_packet *temp = list;
 		while(temp->next_packet != NULL){
 			temp = temp->next_packet;
 		}
 		temp->next_packet = new_element;
 	}

 	numb_packet_to_send += 1;

 	return list;
}

// Remove the head of the list.
struct list_packet* remove_head_list(struct list_packet *list){
	printf("Remove the head of the list\n");
 	if(list != NULL){
 		struct list_packet *to_return = list->next_packet;
 		free(list);
 		numb_packet_to_send -= 1;
 		return to_return;
 	}
 	else{
 		printf("List empty.\n");
 		return NULL;
 	}
}

// Return the number of packets that have been acked.
int get_number_packets_acked(u_int32_t ack_numb_received){
	printf("number of packets that have been acked\n");
 	int numb_packet = 0;

 	u_int32_t relative_ack_received = get_relative_seq(ack_numb_received);

 	struct list_packet *temp = list_waiting_ack;

 	//TODO expression bien sale dans le while, à vérifier qu'il n'y ait pas d'erreur de syntaxe.
 	while(get_relative_seq(temp->packet.rudp_packet.header.seqno + 1) <= relative_ack_received){
 		numb_packet += 1;
 		temp = temp->next_packet;
 	}

 	return numb_packet;
}

// Insert the packet in the list, seq number following the ascending order.
struct list_packet* insert_list_seq(struct list_packet *list, struct send_packet packet_received){
	printf("Insert the packet in the list following seq number.\n");
	struct list_packet* return_list = list;

	if(list != NULL){
		struct list_packet* temp_before = NULL;
		struct list_packet* temp = list;

		u_int32_t seq_received_relative = get_relative_ack((((packet_received).rudp_packet).header).seqno);

		while(temp != NULL && (seq_received_relative > get_relative_ack((((temp->packet).rudp_packet).header).seqno)) {
			temp_before = temp;
			temp = temp->next_packet;
		}

		if(temp_before == NULL){
			// The packet receveived has the lowest seq number among the packets in the buffer. It goes at the beginning of the list.
			return_list = add_list(list, packet_received);
		}
		else if(temp == NULL){
			// The packet receveived has the highest seq number among the packets in the buffer. It goes at the end of the list.
			struct list_packet *new_element = malloc(sizeof(struct list_packet));

 			new_element -> packet = packet_received;
 			new_element -> next_packet = NULL;

 			temp_before -> next_packet = new_element;
		}
		else{
			// The packet belongs between temp_before and temp.
			struct list_packet *new_element = malloc(sizeof(struct list_packet));

 			new_element -> packet = packet_received;
 			new_element -> next_packet = temp;

 			temp_before -> next_packet = new_element;
		}
	}
	else{
		// The list is empty. We start it with the packet received.
		return_list = add_list(list, packet_received);
	}
	return return_list;
}

// Return the address of the packet at the end of the list.
struct send_packet* point_end_list(struct list_packet *list){
	struct send_packet* temp_return = &list->packet;
	struct list_packet* temp = list->next_packet;
	while(temp != NULL){
		temp_return = &temp->packet;
		temp = temp->next_packet;
	}

	return temp_return;
}

/*
 * Manipulation of seq numbers.
 */

 u_int32_t get_actual_seq(u_int32_t relative_seq){
 	return (relative_seq + initial_seq_number )%NUM_SEQ_MAX;
 }

 u_int32_t get_actual_ack(u_int32_t relative_ack){
 	return (relative_ack + initial_ack_number )%NUM_SEQ_MAX;
 }

 u_int32_t get_relative_seq(u_int32_t actual_seq){

 	u_int32_t return_seq = 0;
 	if(actual_seq < initial_seq_number){
 		return_seq = -((actual_seq - initial_seq_number )%NUM_SEQ_MAX);
 	}
 	else{
 		return_seq = (actual_seq - initial_seq_number )%NUM_SEQ_MAX;
 	}
 	return return_seq;
 }

 u_int32_t get_relative_ack(u_int32_t actual_ack){

 	u_int32_t return_ack = 0;
 	if(actual_ack < initial_ack_number){
 		return_ack = -((actual_ack - initial_ack_number )%NUM_SEQ_MAX);
 	}
 	else{
 		return_ack = (actual_ack - initial_ack_number )%NUM_SEQ_MAX;
 	}
 	return return_ack;
 }