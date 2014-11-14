#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

// ======== Prototypes ========
char* format_timestamp(char*);
// ============================

// ========= Constants ========
#define BUF_SIZE = 512;

static const struct monthSet{
	char* jan;
	char* feb;
	char* mar;
	char* apr;
	char* may;
	char* jun;
	char* jul;
	char* aug;
	char* sep;
	char* oct;
	char* nov;
	char* dec;
} monthSet = {"Jan", "Feb", "Mar", "Apr",
			"May", "Jun", "Jul", "Aug",
			"Sep", "Oct", "Nov", "Dec"};
// ============================

int main(int argc, char** argv)
{
	int z;  					 // Socket API return values
	struct sockaddr_in adr_inet;
	struct sockaddr_in adr_clnt;
	int len_inet;
	int sock;  					 // UDP socket descriptor
	char dgram[BUF_SIZE];        // Recv buffer
	char dtfmt[BUF_SIZE];		 // Date/Time Result
	time_t td;                   // Current Time and Date
	struct tm tm;                // Date time values

	char* response;	  			 // Response line to print out: timestamp; client_address:port responseLine; filepath
	char* rec_ip				 // Receiver ip
	int rec_port;				 // Receiver port
	char* file_dest;			 // Destination for incoming file
	char* send_ip				 // Sender ip
	int send_port;				 // Sender port
	char* root;				  	 // User specified root directory

	// Handle command line arguments
	if(argc == 4)
	{
		rec_ip = argv[1];
		rec_port = atoi(argv[2]);
		file_dest = argv[3];
		// TODO: verify file destination
	}
	else
	{
		printf("Specify receiver ip, receiver port, and location to store file\n");
		exit(1);
	}
	/*
    * Create a UDP socket to use:
    */
	s = socket(AF_INET,SOCK_DGRAM,0);
	 if ( s == -1 ) {
		displayError("socket()");
	 }
	 /*
   * Create a socket address, for use
   * with bind(2):
   */
	  memset(&adr_inet,0,sizeof adr_inet);
	  adr_inet.sin_family = AF_INET;
	  adr_inet.sin_port = htons(rec_port);
	  adr_inet.sin_addr.s_addr = inet_addr(rec_ip);

	  if ( adr_inet.sin_addr.s_addr == INADDR_NONE ) {
		 displayError("bad address.");
	  }
	  len_inet = sizeof adr_inet;
  /*
   * Bind an address to our socket, so that
   * client programs can contact this
   * server:
   */
	int optval = 1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);

	  z = bind(s, (struct sockaddr *)&adr_inet, len_inet);
	  if ( z == -1 ) {
		 displayError("bind()");
	  }
	 printf("receiver is running on UDP ip %d port %d and storing at %s\n",atoi(rec_ip),rec_port,file_dest);

	 // Receive packets
	 while(1)
	 {
		 /*
		* Block until the program receives a
		* datagram at the address and port:
		*/
		len_inet = sizeof adr_clnt;
		z = recvfrom(s,            // Socket
					 dgram,        // Receiving buffer
					 sizeof dgram, // Max recv buf size
					 0,            // Flags: no options
					 (struct sockaddr *)&adr_clnt, // Addr
					 &len_inet);  // Addr len, in & out
		if ( z < 0 ) {
		   displayError("recvfrom(2)");
		}
		dgram[z] = '\0';
		/*
		* Get the current date and time:
		*/
		time(&td);
		tm = *localtime(&td);
		/*
		* Format the timestamp
		*/
		char* fmt = "%m %e %H:%M:%S";
		strftime(dtfmt,
				 sizeof dtfmt,
				 fmt,
				 &tm);
		char* timestamp = format_timestamp(dtfmt);
		/* TODO: Assert magic header field
		  * 	Determine packet type
		  * 	Store packet piece by piece
		  * 	Send response packet accordingly
		  */
	 }

}

/*
* Format and return "timestamp"
*/
char* format_timestamp(char* timestamp)
{
	char* month = (char*)malloc(sizeof(char*)*100);
	int monthNum;

	while(timestamp[0] != ' ')
	{
		charAppend(month, timestamp[0]);
		timestamp++;
	}
	timestamp++;
	char* monthAbrv = (char*)malloc(sizeof(char*)*100);
	monthNum = atoi(month);
	switch(monthNum)
	{
		case 1:
			monthAbrv = monthSet.jan;
			break;
		case 2:
			monthAbrv = monthSet.feb;
			break;
		case 3:
			monthAbrv = monthSet.mar;
			break;
		case 4:
			monthAbrv = monthSet.apr;
			break;
		case 5:
			monthAbrv = monthSet.may;
			break;
		case 6:
			monthAbrv = monthSet.jun;
			break;
		case 7:
			monthAbrv = monthSet.jul;
			break;
		case 8:
			monthAbrv = monthSet.aug;
			break;
		case 9:
			monthAbrv = monthSet.sep;
			break;
		case 10:
			monthAbrv = monthSet.oct;
			break;
		case 11:
			monthAbrv = monthSet.nov;
			break;
		case 12:
			monthAbrv = monthSet.dec;
			break;

	}
	timestamp = strAppend(monthAbrv, timestamp);
	return timestamp;
}


