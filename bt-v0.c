/*==========================
 *
 * File: bt-v0.c
 * Content: BitTorrent implementation
 * for use with the command line.
 * Date: 2/6/2026
 * Author: Andy Oldham
 *
 *********************************/

#include "ml.h"
#include "tracker-interactions.h"
#include "vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>


char *get_local_ipv4_address()
{
	struct ifaddrs *local_addr;
	if ( getifaddrs(&local_addr) == -1) {
		printf("Error obtaining local machine address. %s.\n", 
			strerror(errno));
		exit(EXIT_FAILURE);
	}		
	char *home_ipv4_addr ="127.0.0.1";
	while (local_addr) {
		if (local_addr->ifa_addr->sa_family == AF_INET) {
			struct sockaddr_in *localaddr = (struct sockaddr_in *)(local_addr->ifa_addr);
			char *local_ipv4_address = inet_ntoa(localaddr->sin_addr);
			if (strncmp(local_ipv4_address, home_ipv4_addr, strlen(home_ipv4_addr)) != 0) {
				char *local_ipv4_addr = (char *)malloc(strlen(local_ipv4_address) + 1);
				if (local_ipv4_addr == NULL) {
					printf("Error allocating memory for local iv4 address.%s\n", strerror(errno));
				}
				memcpy(local_ipv4_addr, local_ipv4_address, strlen(local_ipv4_address));
				local_ipv4_addr[strlen(local_ipv4_address)] = 0;
				return local_ipv4_addr;
			}
			local_addr = local_addr->ifa_next;
		}
		else if (local_addr->ifa_addr->sa_family == AF_INET6) {
			local_addr = local_addr->ifa_next;
		}
		else {
			local_addr = local_addr->ifa_next;
		}
	}
	printf("Error finding local ipv4 address. List exhausted.\n");
	exit(EXIT_FAILURE);
}

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

	int client_socket_fd = socket(AF_INET, SOCK_STREAM, 6);
	unsigned short local_listening_port = 6881;
	char *local_ipv4_address = get_local_ipv4_address();	
	struct sockaddr_in local_address;
	local_address.sin_family = AF_INET;
	local_address.sin_port = htons(local_listening_port);
	local_address.sin_addr.s_addr = inet_addr(local_ipv4_address);
	while ( bind(client_socket_fd, 
	     	    (struct sockaddr *)&local_address,
	     	    (socklen_t)(sizeof(struct sockaddr_in))) != 0) {
		++local_listening_port;
		printf("Local listening socket binding to port %d failed. %s. Trying other ports...\n", local_listening_port, strerror(errno));
		if (local_listening_port < 6889) {
			printf("Exhausted conventional listening port list.\n");
			exit(EXIT_FAILURE);
		}
		local_address.sin_port = htons(local_listening_port);
	}
	
	char *ih_percent_encoded = ml_get_info_hash_percent_encoded(argv[1], 1);
	unsigned char *peer_id = get_peer_id();	
	char *pi_pe = get_peer_id_percent_encoded(peer_id);
	struct vector *peers  = get_peer_list_from_trackers(trackers,
						ih_percent_encoded,
						pi_pe,
						local_ipv4_address,
						local_listening_port);	
}
