#include "packet.h"

/*
 * Create a single packet of type "pckt_type"
 */
struct packet* create_packet(char* data, int data_length, int pckt_type, int seqno)
{
	//printf("creating %s packet...\n", type_itos(pckt_type));
	struct packet_hdr header;
	struct packet* pckt;
	char* pckt_data;

	int ackno;
	switch(pckt_type)
	{
		case SYN:
			ackno = seqno + 1;
			break;

		case DAT:
			ackno = seqno + data_length;
			pckt_data = (char*)malloc(sizeof(pckt_data)*data_length);
			strcpy(pckt_data, data);
			break;

		case ACK:
			ackno = seqno + 1;
			break;

		case FIN:
			ackno = seqno + 1;
			break;

		case RST:
			ackno = seqno + 1;
			break;

		default:
			perror("sender: unknown packet type\n");
			exit(-1);
	}

	header.magic = MAGIC_HDR;
	header.type = pckt_type;
	header.seqno = seqno;
	header.ackno = ackno;
	//header->winsize = winsize;
	pckt = (struct packet*)malloc(sizeof(*pckt));
	pckt->header = header;
	pckt->payload = pckt_data;
	pckt->payload_length = data_length;

	return pckt;
}

/*
 * Add the packet to the list
 */
void add_packet(struct packet* pckt, struct packet** data_packets)
{
	// add packet to list of packets
	if(pckt->header.type != DAT)
	{
		perror("sender: attempted to store non-DAT packet\n");
		exit(-1);
	}
	int i=0;
	while(data_packets[i] != 0)
	{
		i++;
	}
	data_packets[i] = pckt;
	//printf("stored packet with seqno %d at data_packets[%d]\n",pckt->header.seqno,i);
}

/*
 * Send packet over socket to destination address
 */
void send_packet(struct packet* pckt, int socketfd, struct sockaddr_in adr_dest)
{
	char buffer[20] = "INSIDE send_packet\n";
	printf("sending %s packet...\n", type_itos(pckt->header.type));
	if(sendto(socketfd,
				//pckt,
				buffer,
				//sizeof(struct packet),
				sizeof(buffer),
				0,
				(struct sockaddr*)&adr_dest,
				sizeof(struct sockaddr_in)) < 0)
	{
		perror("sender: sendto()\n");
		exit(-1);
	}
	printf("payload: %s", pckt->payload);
	printf("sent %s packet to %s:%d\n",type_itos(pckt->header.type),
											inet_ntoa(adr_dest.sin_addr),
											ntohs(adr_dest.sin_port));
}

/*
 * Convert packet type from int to string (for printing purposes)
 */
char* type_itos(int pckt_type)
{
	char* type = (char*)malloc(sizeof(type)*3);
	switch(pckt_type)
	{
		case SYN:
			type = "SYN";
			break;
		case DAT:
			type = "DAT";
			break;
		case ACK:
			type = "ACK";
			break;
		case FIN:
			type = "FIN";
			break;
		case RST:
			type = "RST";
			break;
		default:
			perror("sender: attempted to convert unknown packet type\n");
			exit(-1);
	}
	return type;
}
