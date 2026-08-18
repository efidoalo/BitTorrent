/*==========================
 *
 * File: bt-v0.c
 * Content: BitTorrent implementation
 * for use with the command line.
 * Date: 2/6/2026
 * Author: Andy Oldham
 * Compile and link:
 * compile: gcc -I ~/Documents/C-containers -c bt-v0.c
 * link: gcc -o bt-v0 bt-v0.o tracker-interactions.o ml.o ~/Documents/C-containers/vector.o -lm
 * run: ./bt-v0 "XXX-magnet-link-XXX"
 *
 *********************************/

#include "ml.h"
#include "tracker-interactions.h"
#include "peer-interactions.h"
#include "vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>



// generate random 20 bytes for the local peer id.
unsigned char *get_peer_id()
{
	unsigned char *peer_id = (unsigned char *)malloc(20);
	if (peer_id == NULL) {
		printf("Error allocating memory for peer id.%s.\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	srand(time(NULL));
	for (int i=0; i<20; ++i) {
		peer_id[i] = rand() % 256;
	}	
	return peer_id;
}

// returns the 20 byte peer_id but percent encoded, returned memory is 
// dynamically allocated so will hve to be freed at some point
char *get_peer_id_percent_encoded(unsigned char *peer_id)
{
	char *peer_id_pe = (char *)malloc(61);
	if (peer_id_pe == NULL) {
		printf("Error allocating memory for peer id (percent encoded). %s.\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	for (int i=0; i<20; ++i) {
		peer_id_pe[i*3] = '%';
		char first_char = (peer_id[i] >> 4);
		if (first_char <= 9) {
			first_char += 48;
		}
		else {
			first_char += 55;
		}
		char second_char = (peer_id[i]) & 15;
		if (second_char <= 9) {
			second_char += 48;
		}
		else {
			second_char += 55;
		}
		peer_id_pe[(i*3) +1] = first_char;
		peer_id_pe[(i*3) + 2] = second_char;
	}
	peer_id_pe[60] = 0;
	return peer_id_pe;
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		printf("Error. Expected a single argument of a magnet link.\n");
		exit(EXIT_FAILURE);
	}
	unsigned char ml_ver = ml_version(argv[1]);
	if (ml_ver != 1) {
		printf("Error. Unsupported Magnet Link format used.\n");
	}	
	struct vector *trackers = ml_get_trackers(argv[1], 1);
	unsigned char *ih = ml_get_raw_info_hash(argv[1], 1);
	unsigned char *peer_id = get_peer_id();	
	struct vector *peers  = get_peer_list_from_trackers(trackers,
						ih,
						peer_id);	

	printf("%d Peers obtained from trackers.\n", vector_get_size(peers));
	int number_of_peers = vector_get_size(peers);
	struct metadata_info mi;
	mi.length = 0;
	mi.files = 0;
	mi.name = 0;
	mi.pieces = 0;
	mi. piece_length = 0;
	struct peer_interactions_metadata pim;
	pim.metadata_info_downloaded = 0;
	pim.curr_piece = 0;
	pim.subpieces_downloaded = 0;
	for (int i=0; i<6; ++i) {
		pim.timestamps[i] = 0;
	}
	pim.snubbed_by_peers = 0;
	for (int i=0; i<5; ++i) {
		pim.optimistic_unchoked_timestamps[i] = 0;
	}
	struct peer_metadata peers_data[number_of_peers];
	for (int i=0; i<number_of_peers; ++i) {
		(peers_data[i]).peer_connected = 0; 
	}
	int curr_peer_index = 0;
	while (1) {
		if ((peers_data[curr_peer_index]).peer_connected == 0) {
			// peer not connected over tcp yet
			int client_socket_fd_ = socket(AF_INET, SOCK_STREAM, 6);
			struct peer *curr_peer_addr = (struct peer *)vector_read(peers, curr_peer_index);
			 
		}
	}


		
}
