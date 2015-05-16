/*
 * Prototypes 
 */
extern int getaddr(char *addrstr, struct sockaddr_in6 *sa);
extern char *getnameinfohost(struct sockaddr_in6 *sa);
extern char *getnameinfoserv(struct sockaddr_in6 *sa);
