/* 
 * Echo server: send back an identical copy of every packet received 
 */

#include <unistd.h> 
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "rudp_api.h"
#include "event.h"
#include "getaddr.h"

/* 
 * Prototypes 
 */

int recvfrom_handler(rudp_socket_t rsocket, struct sockaddr_in6 *remote, void *buf, int len);
int event_handler(rudp_socket_t rsocket, rudp_event_t event, struct sockaddr_in6 *remote);
int usage(void);

/* 
 * Global variables 
 */
int debug = 0;				/* Print debug messages */
int verbose = 0;			/* Print a message for every packet received */
char *progname;				/* Program name */

#ifdef HAVE_RUDP_DEBUG
extern int rudp_debug;
#endif /* HAVE_RUDP_DEBUG */
/* 
 * usage: how to use this program
 */

int usage(void) {
	fprintf(stderr, "Usage: %s [-v] [-d] port\n", progname);
	exit(1);
}

int main(int argc, char* argv[]) {
	rudp_socket_t rsock;
	int port;
	int c;

	/* 
	 * Parse and collect arguments
	 */
	opterr = 0;
	progname = argv[0];
	
	while ((c = getopt(argc, argv, "vd")) != -1) {
		if (c == 'v') {
			verbose = 1;
		}
		else if (c == 'd') {
#ifdef HAVE_RUDP_DEBUG
			rudp_debug = 1;
#else
			; /* Nothing */
#endif /* HAVE_RUDP_DEBUG */			
		}
		else 
			usage();
	}
	if (argc - optind != 1) {
		usage();
	}
	
	port = atoi(argv[optind]);
		if (port <= 0) {
			fprintf(stderr, "%s: bad destination port: %s\n", progname, argv[optind]);
		exit(1);
	}

	/*
	 * Create RUDP listener socket
	 */

	if ((rsock = rudp_socket(port)) == NULL) {
		fprintf(stderr,"%s: rudp_socket() failed\n", progname);
		exit(1);
	}

	/*
	 * Register receiver callback function
	 */

	rudp_recvfrom_handler(rsock, recvfrom_handler);

	/*
	 * Register event handler callback function
	 */

	rudp_event_handler(rsock, event_handler);

	if (debug) {
		printf("%s waiting on port %i.\n",progname, port);
	}
	/*
	 * Hand over control to event manager
	 */

	eventloop();

	return (0);
}

/* 
 * event_handler: callback function for RUDP events
 */

int event_handler(rudp_socket_t rsocket, rudp_event_t event, struct sockaddr_in6 *remote) {

	switch (event) {
	case RUDP_EVENT_TIMEOUT:
		if (remote) {
			fprintf(stderr, "%s: timeout in communication with %s:%s\n",
				progname, getnameinfohost(remote), getnameinfoserv(remote));
		}
		else {
			fprintf(stderr, "vs_recv: timeout\n");
		}
		break;
	case RUDP_EVENT_CLOSED:
                break;
	default:
		fprintf(stderr, "%s: unknown event %d\n", progname, event);
		break;
	}
	return 0;
}

/*
 * recvfrom_handler: callback function for processing data received
 * on RUDP socket.
 */

int recvfrom_handler(rudp_socket_t rsock, struct sockaddr_in6 *remote, void *buf, int len) {
	if (verbose) {
		fprintf(stdout, "%s: received %d bytes from %s:%s\n",
			progname, len, getnameinfohost(remote), getnameinfoserv(remote));
	}
	if (rudp_sendto(rsock, (char *) buf, len, remote) < 0) {
		fprintf(stderr,"%s: send failure\n", progname);
		rudp_close(rsock);		
	}
	return 0;
}	

