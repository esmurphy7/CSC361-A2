/*
 * rmtp.c
 * helper functions for the RMTP programming assignment
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "rmtp.h"

/*
 * Return the number of seconds since the first call to this procedure
 */
double
now()
{
	struct timeval tv;
	static struct timeval start;

	if (start.tv_sec == 0) {
		gettimeofday(&start, 0);
		tv = start;
	} else
		gettimeofday(&tv, 0);

	return (tv.tv_sec - start.tv_sec + 
		1e-6 * (tv.tv_usec - start.tv_usec));
}

/*
 * Convert a DNS name or numeric IP address into an integer value
 * (in network byte order).
 */
u_int32_t
hostname_to_ipaddr(const char *s)
{
	if (isdigit(*s))
		return (u_int32_t)inet_addr(s);
	else {
		struct hostent *hp = gethostbyname(s);
		if (hp == 0)
			/*XXX*/
			return (0);
		return *((u_int32_t **)hp->h_addr_list)[0];
	}
}

/*
 * Print an rmtp packet to standard output.
 */
void
dump(int dir, char *pkt, int len)
{
	struct rmtphdr* rh = (struct rmtphdr*)pkt;
	int type = ntohl(rh->type);
	int seqno = ntohl(rh->seqno);
	int win = ntohl(rh->window);

	printf("% 3.04f %c %s seq %u win %u len %d\n", now(), dir,
	       (type == RMTP_TYPE_DATA) ? "dat" : 
	       (type == RMTP_TYPE_ACK) ? "ack" : 
	       (type == RMTP_TYPE_SYN) ? "syn" : 
	       (type == RMTP_TYPE_FIN) ? "fin" : 
	       (type == RMTP_TYPE_RST) ? "rst" : "???",
	       seqno, win, len);

	/*XXX*/
	fflush(stdout);
}

/*
 * Print an error message and abort.
 */
void
adios(char *msg)
{
	fprintf(stderr, "%s... aborting\n", msg);
	abort();
}

/*
 * Helper function to send an RMTP packet over the network.
 * As a side effect print the packet header to standard output.
 */
void
sendpkt(int fd, int type, int window, int seqno, char* data, int len)
{
	char wrk[RMTP_MAXPKT + sizeof(struct rmtphdr)];
	struct rmtphdr *rh = (struct rmtphdr *)wrk;
	rh->type = htonl(type);
	rh->window = htonl(window);
	rh->seqno = htonl(seqno);
	if (data != 0)
		memcpy((char*)(rh + 1), data, len);

	dump('s', wrk, len + sizeof(*rh));
	if (send(fd, wrk, len + sizeof(*rh), 0) < 0) {
		perror("write");
		exit(1);
	}
}

/*
 * Helper function to read an RMTP packet from the network.
 * As a side effect print the packet header to standard output.
 */
int
readpkt(int fd, char* pkt, int len)
{
	int cc = read(fd, pkt, len);
	if (cc > 0)
		dump('r', pkt, cc);
	return (cc);
}

/*
 * Reset the network connection by sending an RST packet, print an error
 * message to standard output, and exit.
 */
void
reset(int fd)
{
	fprintf(stderr, "protocol error encountered... resetting connection\n");
	sendpkt(fd, RMTP_TYPE_RST, 0, 0, 0, 0);
	exit(0);
}

/*
 * Read a packet from the network but if "ms" milliseconds transpire
 * before a packet arrives, abort the read attempt and return
 * RMTP_TIMED_OUT.  Otherwise, return the length of the packet read.
 */

/*select may return before timer expires, re-run select */
int
readWithTimer(int fd, char *pkt, int ms)
{
	int s;
	fd_set fds;
	struct timeval tv;
        double entry_time=now();
        int remain_time=ms;
        int len;
        
    	do {

		tv.tv_sec = remain_time / 1000;
 		tv.tv_usec = (remain_time - tv.tv_sec * 1000) * 1000;
		FD_ZERO(&fds);
		FD_SET(fd, &fds);
    
		s = select(fd + 1, &fds, 0, 0, &tv);
		remain_time = ms - (int)((now() - entry_time)*1000);
		
		if (s == -1) {
			perror("Internal Error!");
			fflush(stderr);
			return(RMTP_TIMED_OUT);
		} 
  		if (s == 0) 
 			continue;

		if (FD_ISSET(fd, &fds)) {
			len = readpkt(fd, pkt, RMTP_MAXPKT + sizeof(struct rmtphdr));
			if (len <= 0) continue;
                      	return len;
		}
	} while (remain_time > 100);
	
	return (RMTP_TIMED_OUT);
}
					
				

/*
 * Set an I/O channel in non-blocking mode.
 */
void nonblock(int fd)
{       
        int flags = fcntl(fd, F_GETFL, 0);
#if defined(hpux) || defined(__hpux)
        flags |= O_NONBLOCK;
#else
        flags |= O_NONBLOCK|O_NDELAY;
#endif
        if (fcntl(fd, F_SETFL, flags) == -1) {
                perror("fcntl: F_SETFL");
                exit(1);
        }
}

/*
 * Open a UDP connection.
 */
int
udp_open(u_int32_t dst, int sport, int rport, int nonblocking)
{
	int fd;
	int len;
	int bufsize;
	struct sockaddr_in sin;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		perror("socket");
		exit(1);
	}
	if (nonblocking)
		nonblock(fd);

	/*
	 * bind the local host's address to this socket.  If that
	 * fails, another process probably has the addresses bound so
	 * just exit.
	 */
	memset((char *)&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(rport);
	if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		perror("bind");
		return (-1);
	}

	memset((char *)&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(sport);
	sin.sin_addr.s_addr = dst;
	if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		perror("connect");
		return(-1);
	}
	/*
	 * (try to) make the transmit socket buffer size large.
	 */
	bufsize = 80 * 1024;
	if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char *)&bufsize,
		       sizeof(bufsize)) < 0) {
		bufsize = 48 * 1024;
		if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char *)&bufsize,
			       sizeof(bufsize)) < 0)
			perror("SO_SNDBUF");
	}

	bufsize = 80 * 1024;
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char *)&bufsize,
			sizeof(bufsize)) < 0) {
		bufsize = 32 * 1024;
		if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char *)&bufsize,
				sizeof(bufsize)) < 0)
			perror("SO_RCVBUF");
	}
	return (fd);
}
