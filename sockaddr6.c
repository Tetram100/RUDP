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

/* Utility library. */

#include "sockaddr6.h"

/* sockaddr6_cmp - compare sockaddr_in6 for equality */

int     sockaddr6_cmp(const struct sockaddr_in6 * sa,
		      const struct sockaddr_in6 * sb)
{
    if (sa->sin6_family != sb->sin6_family)
	return (sa->sin6_family - sb->sin6_family);

    if (sa->sin6_family == AF_INET6) {
	if (sa->sin6_port != sb->sin6_port)
		return (sa->sin6_port - sb->sin6_port);
	return (memcmp((char *) &sa->sin6_addr, &sb->sin6_addr, sizeof(sa->sin6_addr)));
    } else {
	    fprintf(stderr, "sockaddr6_cmp: unsupported address family %d",
		  sa->sin6_family);
	    exit(1);
    }
}
