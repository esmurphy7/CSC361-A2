#include "packet.h"

/*
 * Create a single packet of type "pckt_type"
 */
struct packet* create_packet(char* data, int data_length, int pckt_type, int seqno)
{
	//printf("creating %s packet...\n", type_itos(pckt_type));
	struct packet_hdr* header;
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
			perror("attempted to create unknown packet type\n");
			exit(-1);
	}

	header = (struct packet_hdr*)malloc(sizeof(*header));
	strcpy(header->magic, MAGIC_HDR);
	header->type = pckt_type;
	header->seqno = seqno;
	header->ackno = ackno;
	header->winsize = WINDOW_SIZE;
	pckt = (struct packet*)malloc(sizeof(*pckt));
	pckt->header = *header;
	strcpy(pckt->payload, pckt_data);
	pckt->payload_length = data_length;

	//printf("Created packet, ");
	//print_contents(pckt);

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
	//print_contents(pckt);
	// Construct a string from the packet that can be sent over the network
	char* buffer = construct_string(pckt);

	printf("sending string: %s\n", buffer);
	//char* test_buffer = (char*)malloc(sizeof(char*)*8);
	//test_buffer = "testbuff";
	//printf("sending %s packet...\n", test_buffer);
	if(sendto(socketfd,
				//test_buffer,
				buffer,
				//sizeof(test_buffer),
				sizeof(char*)*MAX_PACKET_SIZE,
				0,
				(struct sockaddr*)&adr_dest,
				sizeof(struct sockaddr_in)) < 0)
	{
		perror("sender: sendto()\n");
		exit(-1);
	}
	//printf("payload: %s", pckt->payload);
	printf("sent %s packet to %s:%d\n",type_itos(pckt->header.type),
											inet_ntoa(adr_dest.sin_addr),
											ntohs(adr_dest.sin_port));
	free(buffer);
}

/*
 * Construct a string from the given packet that can be sent over the network.
 * Each packet field is separated by a semi-colon ";"
 */
char* construct_string(struct packet* pckt)
{
	char* buffer = (char*)malloc(sizeof(char*)*MAX_PACKET_SIZE);
	char sep[2] = ";";

	strcat(buffer, pckt->header.magic);
	strcat(buffer, sep);

	char type[sizeof(pckt->header.type)];
	sprintf(type, "%d", pckt->header.type);
	strcat(buffer, type);
	strcat(buffer, sep);

	char seqno[sizeof(pckt->header.seqno)];
	sprintf(seqno, "%d", pckt->header.seqno);
	strcat(buffer, seqno);
	strcat(buffer, sep);

	char ackno[sizeof(pckt->header.ackno)];
	sprintf(ackno, "%d", pckt->header.ackno);
	strcat(buffer, ackno);
	strcat(buffer, sep);

	char winsize[sizeof(pckt->header.winsize)];
	sprintf(winsize, "%d", pckt->header.winsize);
	strcat(buffer, winsize);
	strcat(buffer, sep);

	strcat(buffer, pckt->payload);
	strcat(buffer, sep);

	char payload_length[sizeof(pckt->payload_length)];
	sprintf(payload_length, "%d", pckt->payload_length);
	strcat(buffer, payload_length);
	strcat(buffer, sep);

	return buffer;
}

struct packet* deconstruct_string(char* buffer)
{
	//printf("deconstructing %s...\n",buffer);
	struct packet* pckt = (struct packet*)malloc(sizeof(pckt));
	char* magic = (char*)malloc(sizeof(magic)*MAX_PAYLOAD_SIZE);
	char* type = (char*)malloc(sizeof(type)*MAX_PAYLOAD_SIZE);
	char* seqno = (char*)malloc(sizeof(seqno)*MAX_PAYLOAD_SIZE);
	char* ackno = (char*)malloc(sizeof(ackno)*MAX_PAYLOAD_SIZE);
	char* winsize = (char*)malloc(sizeof(winsize)*MAX_PAYLOAD_SIZE);
	char* payload = (char*)malloc(sizeof(payload)*MAX_PAYLOAD_SIZE);
	char* payload_length = (char*)malloc(sizeof(payload_length)*MAX_PAYLOAD_SIZE);

	char* cur_field = (char*)malloc(sizeof(cur_field)*MAX_PAYLOAD_SIZE);
	int num_semicolons = 0;
	int i;
	for(i=0; i<strlen(buffer); i++)
	{
		//printf("buffer[%d]: %c\n",i,buffer[i]);
		if(buffer[i] == ';')
		{
			num_semicolons++;
			switch(num_semicolons)
			{
				case 1:
					strcpy(magic, cur_field);
					break;
				case 2:
					strcpy(type, cur_field);
					break;
				case 3:
					strcpy(seqno, cur_field);
					break;
				case 4:
					strcpy(ackno, cur_field);
					break;
				case 5:
					strcpy(winsize, cur_field);
					break;
				case 6:
					strcpy(payload, cur_field);
					break;
				case 7:
					strcpy(payload_length, cur_field);
					break;
				default:
					perror("attempted to deconstruct packet with too many fields\n");
					exit(-1);
			}
			cur_field = (char*)malloc(sizeof(char*)*MAX_PAYLOAD_SIZE);
		}
		else
			cur_field = charAppend(cur_field, buffer[i]);
		//printf("current field: %s\n",cur_field);
	}
	strcpy(pckt->header.magic, magic);
	pckt->header.type = atoi(type);
	pckt->header.seqno = atoi(seqno);
	pckt->header.ackno = atoi(ackno);
	pckt->header.winsize = atoi(winsize);
	strcpy(pckt->payload, payload);
	pckt->payload_length = atoi(payload_length);
	return pckt;
}

/*
* Utility: Append c to s
*/
char* charAppend(char* s, char c)
{
	int len = strlen(s);
	s[len] = c;
	return s;
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

void print_contents(struct packet* pckt)
{
	printf("Packet contents...\n");
	struct packet_hdr header = pckt->header;
	printf("magic: %s\n",header.magic);
	char type[3];
	sprintf(type, "%d", header.type);
	printf("type: %s\n",type);
	printf("seqno: %d\n",header.seqno);
	printf("ackno: %d\n",header.ackno);
	printf("winsize: %d\n",header.winsize);
	printf("payload: %s\n",pckt->payload);
	printf("payload_length: %d\n",pckt->payload_length);
}
