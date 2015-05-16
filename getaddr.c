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

#include "getaddr.h"

/*
 * getaddr: parse a "host:port" string, and generate socket address structure.
 * Returns: 1 if success, 0 if string does match "host:port" format.
 * Exits: if getaddrinfo lookup fails (exit status 1)
 *
 * "port" is a decimal port number, or a service name.
 * valid formats for "host":
 * - domain (DNS) name "host.domain"
 * - literal IPv4 address "1.2.3.4"
 * - literal IPv6 address within brackets "[20:30:40::1]"
 * - empty string ""
 */

int getaddr(char *addstr, struct sockaddr_in6 *sa) {
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	char hoststr[NI_MAXHOST], servstr[NI_MAXSERV];
	int err;

	if (strlen(addstr) >= NI_MAXHOST) {
		printf("ret1\n");
		return 0;
	}
	if (addstr[0] == '[') { /* IPv6 literal */
		strcpy(hoststr, addstr+1);		
		if (strchr(hoststr, ']') == NULL) { /* No enclosing square bracket */
			return 0;
		}
		*strchr(hoststr, ']') = 0; /* Terminate string where square bracket is */
		if (*(strchr(addstr, ']') + 1) != ':') {/* No port */
			return 0;
		}
		strcpy(servstr, strchr(addstr, ']') + 2);		
	}
	else { /* Domain name or IPv4 literal */
		if (strchr(addstr, ':') == NULL) /* No port */
			return 0;
		strcpy(hoststr, addstr);
		*strchr(hoststr, ':') = 0; /* Terminate string where colon is */
		strcpy(servstr, strchr(addstr, ':') + 1);
	}

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET6;        /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_DGRAM;    /* Datagram socket */
	hints.ai_flags |= AI_V4MAPPED;     /* Return IPv4 addresses as IPv4-mapped IPv6 addresses */
	hints.ai_protocol = IPPROTO_UDP;   /* Require UDP */

	err = getaddrinfo(hoststr, servstr, &hints, &result);
	if (err != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
		exit(1);
		/*NOTREACHED*/
	}

	for (rp = result; rp; rp = rp->ai_next) {
		/* Use first IPv6 address */
		if (rp->ai_family == AF_INET6) {
			memcpy(sa, rp->ai_addr, rp->ai_addrlen);
				freeaddrinfo(result);
			return 1;
		}
	}
	freeaddrinfo(result);
	return 0;
}

/*
 * call_getnameinfo: helper function for lookup functions below
 */

static void  call_getnameinfo(struct sockaddr_in6 *sa, char *hb, int hblen, char *sb, int sblen, char *errstr) {
	int err;
	if (sa->sin6_family != AF_INET6) {
		fprintf(stderr, "%s: Not an IPv6 socket address (sa_family = %d)\n", errstr, sa->sin6_family);
	}
	if ((err = getnameinfo((struct sockaddr *) sa, sizeof(struct sockaddr_in6), hb, hblen, sb, sblen, 
			       NI_NUMERICHOST | NI_NUMERICSERV))) {
		fprintf(stderr, "%s: %s\n", errstr, gai_strerror(err));
		exit(1);
	}
}

/*
 * getnameinfohost: use getnameinfo(3) to translate a sockaddr_in6 to a host string
 * Returns a pointer to the host string
 * Note: The memory area where the string resides is overwritten between consecutive calls
 */

char *getnameinfohost(struct sockaddr_in6 *sa) {
	static char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

	call_getnameinfo(sa, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), "getnameinfohost");
	return hbuf;
}
/*
 * getnameinfoserv: same as getnameinfohost, but for service name
 * Returns port number, as a string 
 */

char *getnameinfoserv(struct sockaddr_in6 *sa) {
	static char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

	call_getnameinfo(sa, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), "getnameinfoserv");
	return sbuf;
}
