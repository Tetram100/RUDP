#ifndef RUDP_API_H
#define	RUDP_API_H

#define RUDP_MAXPKTSIZE 1000	/* Number of data bytes that can sent in a
				 * packet, RUDP header not included */

/*
 * Event types for callback notifications
 */

typedef enum {
	RUDP_EVENT_TIMEOUT, 
	RUDP_EVENT_CLOSED,
} rudp_event_t; 

/*
 * RUDP socket handle
 */

typedef void *rudp_socket_t;

/*
 * Prototypes
 */

/* 
 * Socket creation 
 */
rudp_socket_t rudp_socket(int port);

/* 
 * Socket termination
 */
int rudp_close(rudp_socket_t rsocket);

/* 
 * Send a datagram 
 */
int rudp_sendto(rudp_socket_t rsocket, void* data, int len, struct sockaddr_in6 *to);

/* 
 * Register callback function for packet receiption 
 * Note: buf and len arguments to callback function 
 * are only valid during the call to the handler
 */
int rudp_recvfrom_handler(rudp_socket_t rsocket, 
			  int (*handler)(rudp_socket_t rsock, 
					 struct sockaddr_in6 *remote,
					 void *buf, int len));
/*
 * Register callback handler for event notifications
 */
int rudp_event_handler(rudp_socket_t rsocket, 
		       int (*handler)(rudp_socket_t rsock, 
				      rudp_event_t event, 
				      struct sockaddr_in6 *remote));
#endif /* RUDP_API_H */
