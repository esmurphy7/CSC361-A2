/* TODO: Change byte arrays from char types to appropriate types (unsigned int?)
 * 			- Replace string functions such as strcpy(), strlen(), (char*)malloc() with memcpy()
 * 		Trim the very last packet of excess null memory
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <unistd.h>
#include <math.h>

#include "rdps.h"

// ============== Prototypes =============
void create_packet(char*, int);
void packetize(char*, int);
// =============== Globals ===============
struct packet* data_packets;
// =======================================

int main(int argc, char* argv[])
{
	char* sender_ip;
	int sender_port;
	char* receiver_ip;
	int receiver_port;

	char* filename;
	FILE* file;
	int file_length;	// Length of file in bytes
	char* data;			// Each element is a byte from the file

	if(argc != 6)
	{
		printf("Usage: $rdps sender_ip sender_port receiver_ip receiver_port sender_file_name");
		exit(0);
	}

	sender_ip = argv[1];
	sender_port = atoi(argv[2]);
	receiver_ip = argv[3];
	receiver_port = atoi(argv[4]);
	filename = argv[5];

	// Open file
	if((file = fopen(filename, "r")) == NULL)
	{
		perror("sender: open\n");
		exit(-1);
	}
	// determine file size
	fseek(file, 0, SEEK_END);
	file_length = ftell(file);
	rewind(file);
	printf("file length: %d\n",file_length);
	data = (char*)malloc(sizeof(data)*file_length);
	// Read file byte by byte into data buffer
	if(fread(data, 1, file_length, file) != file_length)
	{
		perror("sender: fread");
		free(data);
		exit(-1);
	}
	fclose(file);

	float n = ((float)file_length) / ((float)MAX_DATA);
	printf("n: %e\n",n);
	float num_packets = ceil(n);
	printf("Number of data packets: %f\n",num_packets);
	packetize(data, file_length);
	// sending sequence

	return 0;
}

/*
 * Convert the file's data into a number of packets
 */
void packetize(char* data, int file_length)
{
	int current_byte;	// Index of "data"
	int bytes_read;		// Bytes read from "data"
	char* pckt_data;	// Bytes meant for a packet

	// Move bytes from data buffer into pckt_data and create packets
	current_byte = 0;
	bytes_read = 0;
	pckt_data = (char*)malloc(sizeof(pckt_data)*MAX_DATA);
	printf("Reading file...\n");
	while(current_byte < file_length)
	{
		pckt_data[bytes_read] = data[current_byte];
		if(bytes_read == MAX_DATA)
		{
			//printf("Current byte: %d, Bytes read: %d\n",current_byte, bytes_read);
			create_packet(pckt_data, bytes_read);
			free(pckt_data);
			pckt_data = (char*)malloc(sizeof(pckt_data)*MAX_DATA);
			bytes_read = 0;
		}
		bytes_read++;
		current_byte++;
	}
	// End of file, packet size is less than MAX_DATA
	//		=>Shave off empty memory
	create_packet(pckt_data, bytes_read);

	printf("sender: end of file\n");
	free(data);
}

/*
 * Create a single packet with data
 */
void create_packet(char* data, int data_length)
{
	//printf("Creating packet with data: %s\n",data);
	char* pckt_data = (char*)malloc(sizeof(pckt_data)*data_length);
	strcpy(pckt_data, data);
	//sleep(3);
	// init other packet fields
	// add packet to list of packets
}
