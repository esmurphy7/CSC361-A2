/*
 * An RMTP receiver.  This module implements the RMTP receiver
 * protocol and dumps the contents of the connection to
 * a file called "testOutput" file in current directory.
 *
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <fcntl.h>

#include "rmtp.h"

/*
 * The receiver's Protocol Control Block, or PCB, houses all
 * of the protocol state for this end of the connection.
 * Messages are potentially buffered in this data structure
 * so they can be ultimately delivered to the "application"
 * in sequence.
 */
struct rmtp_rcvr_pcb {
	int state;		/* protocol state: normally ESTABLISHED */
	int fd;			/* UDP socket descriptor */
	int rwnd;		/* latest advertised window */
	int nerrs;		/* generic error counter */
	u_int32_t NFE;		/* next frame expected */
	struct pktbuf pkts[RMTP_MAXBUFS];
        char valid[RMTP_MAXBUFS]; /* are pkts are valid? */
};

/*
 * Send an RMTP ack back to the source.  The pcb tells
 * us what frame we expect, so we ack that sequence number.
 */
void
rmtp_send_ack(struct rmtp_rcvr_pcb *pcb)
{
	sendpkt(pcb->fd, RMTP_TYPE_ACK, pcb->rwnd, pcb->NFE, 0, 0);
}

/*
 * File descriptor for the output file
 */
int outFile;

/*
 * Routine that gets call for each message received.
 * RMTP ensures that messages are delivered in sequence.
 */
void
rmtp_consume(char *pkt, int len)
{
	write(outFile, pkt, len);
	printf("consume: %d bytes\n", len);
}

/*
 * Generic logic to handle an incoming packet.
 * We decode the packet depending on what state
 * the PCB says the connection is in.
 */
void
rmtp_recv(struct rmtp_rcvr_pcb *pcb, char* pkt, int len)
{
	u_int32_t LFA;
	u_int32_t seqno;
	struct rmtphdr* rh = (struct rmtphdr *)pkt;
	int type;

	type = ntohl(rh->type);
	seqno = ntohl(rh->seqno);
	if (len < sizeof(*rh)) {
		pcb->nerrs += 1;
		return;
	}
	if (pcb->state == RMTP_STATE_LISTEN) {
		if (type != RMTP_TYPE_SYN)
			reset(pcb->fd);
		pcb->NFE = seqno+1;
		pcb->state = RMTP_STATE_ESTABLISHED;
		rmtp_send_ack(pcb);
		return;
	}
	if (pcb->state == RMTP_STATE_TIME_WAIT) {
		if (type != RMTP_TYPE_FIN) {
			pcb->nerrs += 1;
			return;
		}
		/* if the seqno of the FIN is wrong, reset the connection */
		if (seqno != (pcb->NFE-1))
			reset(pcb->fd);
		/* otherwise, ack the fin and stay around in time wait */
		rmtp_send_ack(pcb);
		return;
	}
	/* otherwise, we're in the established state */

	if (type == RMTP_TYPE_FIN) {
		/*
		 * Received a FIN packet.  The sender cannot generate
		 * a FIN until all of its data has been ACK'd (by
		 * definition of the RMTP state diagram), so
		 * it's a protocol error if the FIN does not
		 * match up with our expectations.  If it does,
		 * ACK it and enter time wait to handle any
		 * retransmitted FINs.
		 */
		if (seqno != pcb->NFE)
			reset(pcb->fd);
                pcb->NFE = seqno+1;
		pcb->state = RMTP_STATE_TIME_WAIT;
		rmtp_send_ack(pcb);
		return;
	}

	LFA = pcb->NFE + pcb->rwnd;
	if (SEQ_GEQ(seqno, LFA) || SEQ_LT(seqno, pcb->NFE)) {
		/* not in feasible window */
		rmtp_send_ack(pcb);
		return;
	}
	/*
	 * New data has arrived.  If the ACK arrives in order,
	 * consume the data and see if we've filled a gap
	 * in the sequence space.  Otherwise, stash the packet
	 * in a buffer.  In either case, send back an ACK for
	 * the highest contiguously received packet.
	 */
	if (seqno == pcb->NFE) {
		/* packet in order */
		rmtp_consume(pkt + sizeof(*rh), len - sizeof(*rh));
		seqno += 1;
		/*
		 * Now check if the arrival of this packet,
		 * allows us to send any more packets.
		 */
		for (;;) {
			struct pktbuf *pb = &pcb->pkts[seqno % RMTP_MAXBUFS];
			 if ( (pb->seqno != seqno) || (pcb->valid[seqno % RMTP_MAXBUFS]==0) ) 
				break;
			rmtp_consume(pb->data, pb->len);
                        pcb->valid[seqno % RMTP_MAXBUFS]=0;
			seqno += 1;
		}
		pcb->NFE = seqno;
	} else {
		/*
		 * packet out of order but within receive window,
		 * copy the data and record the seqno to validate the buffer
		 * ack the packet but ack the expected seqno rather
		 * than the received one.
		 */
		struct pktbuf *pb = &pcb->pkts[seqno % RMTP_MAXBUFS];
		pb->seqno = seqno;
		pb->len = len - sizeof(*rh);
		memcpy(pb->data, pkt + sizeof(*rh), len - sizeof(*rh));
                pcb->valid[seqno % RMTP_MAXBUFS]=1;
	}
	/*
	 * In any event, send an ACK back to the sender.
	 */
	rmtp_send_ack(pcb);
}

/*
 * Run the receiver polling loop: allocate and initialize the PCB
 * then enter an infinite loop to process incoming packets.
 */
void
rmtp_receiver_run(u_int32_t dst, int sport, int rport)
{
	int i;
	struct rmtp_rcvr_pcb* pcb = (struct rmtp_rcvr_pcb*)malloc(sizeof(*pcb));
	/*
	 * Initialize the receiver's protocol control block
	 * and open the underlying UDP/IP communication channel.
	 */
	pcb->state = RMTP_STATE_LISTEN;
	pcb->fd = udp_open(dst, sport, rport, 0);
	pcb->rwnd = RMTP_MAXWIN;
	pcb->nerrs = 0;
	pcb->NFE = 1;
	for (i = 0; i < RMTP_MAXBUFS; ++i) {
		pcb->pkts[i].seqno = -1;
                pcb->valid[i]=0;
        }

	/*
	 * Enter an infinite loop reading packets from the network
	 * and processing them.  The receiver processing loop is
	 * relatively simple because we do not need to schedule
	 * timers or handle any other asynchronous event.
	 */
	for (;;) {
		int len;
		char pkt[RMTP_MAXPKT+sizeof(struct rmtphdr)];
		len = readpkt(pcb->fd, pkt, sizeof(pkt));
		rmtp_recv(pcb, pkt, len);
                if (pcb->state==RMTP_STATE_TIME_WAIT)
                   break;
	}
}

void
main(int argc, char **argv)
{
	int cnt = 0;
	struct rmtp_rcvr_pcb* pcb;
	u_int32_t dst;
	int sport, rport;

	if (argc != 4) {
		fprintf(stderr, "usage: receiver dest send-port recv-port\n");
		exit(1);
	}
	dst = hostname_to_ipaddr(argv[1]);
	if (dst == 0) {
		fprintf(stderr, "bad address: %s\n", argv[1]);
		exit(1);
	}
	sport = atoi(argv[2]);
	rport = atoi(argv[3]);

	/*
	 * Open the output file for writing.  The RMTP sender tranfers
	 * a file to us and we simply dump it to disk.
	 */
	outFile = open("testOutput", O_CREAT|O_WRONLY|O_TRUNC, 0644);
	if (outFile < 0) {
		perror("testOutput");
		exit(1);
	}

	/*
	 * "Run" the receiver protocol.
	 */
	rmtp_receiver_run(dst, sport, rport);
}
