/*
 * rmtp.h
 * helper functions and definitions for
 * the RMTP programming assignment
 */

#define RMTP_MAXPKT 1024
#define RMTP_MAXWIN 128
#define RMTP_MAXBUFS (2*RMTP_MAXWIN)

#define RMTP_TIMED_OUT (-1)

/*
 * Packet types
 */
#define RMTP_TYPE_DATA	1
#define RMTP_TYPE_ACK	2
#define RMTP_TYPE_SYN	3
#define RMTP_TYPE_FIN	4
#define RMTP_TYPE_RST	5

/*
 * State types
 */
/* shared */
#define RMTP_STATE_ESTABLISHED	1
#define RMTP_STATE_CLOSED	2
/* sender */
#define RMTP_STATE_SYN_SENT	3
#define RMTP_STATE_FIN_WAIT	4
#define RMTP_STATE_CLOSING	7
/* receiver */
#define RMTP_STATE_LISTEN	5
#define RMTP_STATE_TIME_WAIT	6

/*
 * Try to figure out how to define types of explicit
 * width based on the architecture.
 */
#if defined(sgi) || defined(__bsdi__) || defined(__FreeBSD__)
#include <sys/types.h>
#elif defined(linux)
#include <sys/bitypes.h>
#else
#ifdef ultrix
#include <sys/types.h>
#endif
typedef unsigned char u_int8_t;
typedef short int16_t;
typedef unsigned short u_int16_t;
typedef unsigned int u_int32_t;
typedef int int32_t;
#endif

/*
 * From the BSD TCP code:
 *  sequence numbers are 32 bit integers operated
 * on with modular arithmetic.  These macros can be
 * used to compare such integers.
 */
#define	SEQ_LT(a,b)	((int)((a)-(b)) < 0)
#define	SEQ_LEQ(a,b)	((int)((a)-(b)) <= 0)
#define	SEQ_GT(a,b)	((int)((a)-(b)) > 0)
#define	SEQ_GEQ(a,b)	((int)((a)-(b)) >= 0)

struct pktbuf {
	int seqno;
	int len;
	char data[RMTP_MAXPKT];
};

struct rmtphdr {
	u_int32_t type;
	u_int32_t window;
	u_int32_t seqno;
};

extern double now();
extern int readWithTimer(int fd, char *pkt, int ms);
extern void adios(char *msg);
extern u_int32_t hostname_to_ipaddr(const char *s);
