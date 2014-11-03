/*
 *
 * An RMTP sender.  This module implements the RMTP sender
 * protocol and reads the data to be transferred over the connection
 * from a file called "inputFile" in your current directory.
 *
 */


#include <stdio.h>
#include <sys/types.h>

#include <fcntl.h>
#include <netdb.h>


#include "rmtp.h"

struct rmtp_sender_pcb {
	/* YOUR CODE HERE */
};

/* YOUR CODE HERE */

/*
 * Send a message using RMTP.
 * The entry point to the dummy side of the protocol.
 */
void
rmtp_send(struct rmtp_sender_pcb* pcb, char* data, int len)
{
	/* YOUR CODE HERE */
}

struct rmtp_sender_pcb *
rmtp_sender_open(u_int32_t dst, int sport, int rport)
{
	struct rmtp_sender_pcb* pcb;

	pcb = (struct rmtp_sender_pcb*)malloc(sizeof(*pcb));

	/* YOUR CODE HERE */

	return (pcb);
}

void
rmtp_sender_close(struct rmtp_sender_pcb *pcb)
{
	/* YOUR CODE HERE */
}

void
main(int argc, char **argv)
{
	int cnt = 0;
	struct rmtp_sender_pcb* pcb;
	u_int32_t dst;
	int sport, rport;
	int file;

	if (argc != 4) {
		fprintf(stderr, "usage: sender dest send-port recv-port\n");
		exit(1);
	}
	dst = hostname_to_ipaddr(argv[1]);
	if (dst == 0) {
		fprintf(stderr, "bad address: %s\n", argv[1]);
		exit(1);
	}
	sport = atoi(argv[2]);
	rport = atoi(argv[3]);

	file = open("inputFile", O_RDONLY);
	if (file < 0) {
		perror("inputFile");
		exit(1);
	}
	pcb = rmtp_sender_open(dst, sport, rport);
	for (;;) {
		char wrk[RMTP_MAXPKT / 2];
		int cc = read(file, wrk, sizeof(wrk));
		if (cc <= 0)
			break;
		rmtp_send(pcb, wrk, cc);
	}
	rmtp_sender_close(pcb);
	exit(0);
}
