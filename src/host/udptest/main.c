/*
 * main.c
 *	Entry point to the test.
 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * usage()
 *	Tell our user how to invoke this client.
 */
void usage(char *progname)
{
	fprintf(stderr, "Usage: %s [-h hostname] [-p port]\n", progname);
	exit(1);
}

/*
 * main()
 *	Entry point.
 */
int main(int argc, char **argv)
{
	char c;
	int s;
        struct sockaddr_in sa;
        struct hostent *hp;
        char *progname;
        char hostname[128];
        int port;
        char *tmp;
                                		
	printf("Liquorice UDP test 20000701a\n");

	progname = argv[0];
        strcpy(hostname, "127.0.0.1");
        port = 1923;
        		
	/*
	 * What options have been passed to us on the command line?
	 */
	while ((c = getopt(argc, argv, "h:p:")) != EOF) {
		switch (c) {
			case 'h':
				strncpy(hostname, optarg, sizeof(hostname));
				break;
			
			case 'p':
				port = (int)strtol(optarg, &tmp, 0);
				break;
			
			default:
				usage(progname);
		}
	}
	
	/*
	 * Create a datagram socket.
	 */
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		fprintf(stderr, "main(): unable to create socket - error %d\n", s);
		exit(1);
	}

        /*
         * Determine the host and port that we're going to talk to.
         */
        hp = gethostbyname(hostname);
        if (hp == NULL) {
		fprintf(stderr, "main(): unable to get host entry\n");
		exit(1);
	}
                                	
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	memcpy(&sa.sin_addr, hp->h_addr, hp->h_length);

	printf("Sending packet to '%s', port %d\n", hostname, port);
		
	sendto(s, hostname, 16, 0, &sa, sizeof(struct sockaddr_in));
	
	close(s);
	
	return 0;
}
