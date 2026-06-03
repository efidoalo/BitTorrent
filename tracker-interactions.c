/*=========================================;
 *
 * File: tracker-interactions.h
 * Content: function and data structure
 * definitions for interacting with trackers
 * Date: 3/6/2026
 * Author: Andy Oldham
 *
 ******************************************/

#include "tracker-interactions.h"
#include "vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <poll.h>
#include <fcntl.h>

struct peer
{
	unsigned char id[20]; // peer id
	char *ip; // internet protocol address
	unsigned short port; // peer port number
};

// returns 1 if the buffer of length len includes a bencoded dictionary, 0 otherwise
unsigned char includes_bencoded_dictionary(char *buff, int len)
{
	for (int i=0; i<len; ++i) {
		if (buff[i] == 'd') {
			
		}
	}
}

void print_peer(void *peer_address)
{
	struct peer *p = (struct peer *)peer_address;
	printf("{ id:");
	for (int i=0; i<20; ++i) {
		printf("%u",(p->id)[i]);
	}
	printf(", ip:");
	printf("%s, port:%u}", p->ip, p->port);
}

struct vector *get_peer_list_from_trackers(struct vector *trackers, 
					   char *percent_encoded_info_hash,
					   char *percent_encoded_peer_id,
					   char *local_ipv4_address,
					   unsigned short local_listening_port)
{
	int no_of_trackers = vector_get_size(trackers);
	struct vector *peers = vector_null_init(sizeof(struct peer), print_peer);
	for (int i=0; i<no_of_trackers; ++i) {
		char **pptracker_url = (char **)vector_read(trackers, i);
		char *tracker_url = *pptracker_url;
		char *http_prefix = "http://";
		size_t http_prefix_len = strlen(http_prefix);
		char *https_prefix = "https://";
		size_t https_prefix_len = strlen(https_prefix);
		char *udp_prefix = "udp://";
		size_t udp_prefix_len = strlen(udp_prefix);
		int init_index = 0;
		if (strncmp(tracker_url, http_prefix, http_prefix_len) == 0) {
			init_index = http_prefix_len;
		}
		else if (strncmp(tracker_url, https_prefix, https_prefix_len) == 0) {
			init_index = https_prefix_len;
		}
		else if (strncmp(tracker_url,  udp_prefix, udp_prefix_len) == 0) {
			init_index = udp_prefix_len;
		}
		int tracker_final_index = strlen(tracker_url);
		for (int i=init_index; i<tracker_final_index; ++i) {
			char port_delimiter = ':';
			if (tracker_url[i] == port_delimiter) {
				tracker_final_index = i;
				break;
			}
		}
		char *tracker_hostname = (char *)malloc(tracker_final_index - init_index + 1);
		if (tracker_hostname == NULL) {
			printf("Error allocating memory for tracker host. %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		memcpy(tracker_hostname, &(tracker_url[init_index]), tracker_final_index - init_index);
		tracker_hostname[tracker_final_index - init_index] = 0;
		
		int client_socket_fd = socket(AF_INET, SOCK_STREAM, 6);
		struct addrinfo hints;
		hints.ai_flags = 0;
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = 6;
		hints.ai_addrlen = 0;
		hints.ai_addr = NULL;
		hints.ai_canonname = NULL;
		hints.ai_next = NULL;
		struct addrinfo *tracker_address = NULL;
		char tracker_port_string[10] = {0};
		int tracker_port_string_index = tracker_final_index + 1;
		int tracker_url_len = strlen(tracker_url);	
		int tracker_port_index = 0;
		while (tracker_port_string_index < tracker_url_len) {
			tracker_port_string[tracker_port_index++] = tracker_url[tracker_port_string_index++];
			if (tracker_url[tracker_port_string_index] == '/') {
				break;
			}
		}
		printf("Trying to get address of tracker %s:%s\n", tracker_hostname, tracker_port_string);
		if (getaddrinfo(tracker_hostname, tracker_port_string, &hints, &tracker_address) != 0) {
			free(tracker_hostname);
			continue;
		}
		else {
			fcntl(client_socket_fd, F_SETFL, O_NONBLOCK);
			while (tracker_address) {
					
				connect(client_socket_fd, 
						tracker_address->ai_addr, 
						tracker_address->ai_addrlen);
				struct pollfd p;
				p.fd = client_socket_fd;
				p.events = POLLOUT;
				printf("Waiting for connect to tracker %s:%s...\n",
					tracker_hostname,
					tracker_port_string);
				int pollres = poll(&p, 1, 30000);
			        if (pollres == -1) {
					printf("Error waiting for connection to tracker %s:%s. %s.\n", tracker_hostname, tracker_port_string, strerror(errno));
					tracker_address = tracker_address->ai_next;
					continue;
				}
				if (pollres >= 0) {
					if (pollres > 0) {
						if (p.revents & POLLOUT) {
							int optval = -1;
							socklen_t optlen = sizeof(int);
							if (getsockopt(client_socket_fd, 
									SOL_SOCKET,
								   	SO_ERROR,
								   	&optval,
								   	&optlen) == -1) {
								printf("Error getting SO_ERROR socket option after attempting nonblocking connect to tracker %s:%s. %s.\n", tracker_hostname,
									 tracker_port_string,
									 strerror(errno));
								tracker_address = tracker_address->ai_next;
                                        			continue;
							}
							if (optval == 0) {
								// successfully connected
								printf("Connected to tracker %s:%s\n", tracker_hostname, tracker_port_string);
								break;
							}
							else {
								tracker_address = tracker_address->ai_next;
                                                        	continue;
							}
						}
						else {
							tracker_address = tracker_address->ai_next;								
							continue;
						}
					}
					else {
						
						// timeout occurred before socket became ready
						// for writing. Either connection was already
						// made or  timeout occurred
						int optval = -1;
						socklen_t optlen = sizeof(int);
						if (getsockopt(client_socket_fd,
								SOL_SOCKET,
								SO_ERROR,
								&optval,
								&optlen) == -1) {
							printf("Error getting SO_ERROR socket option after attempting nonblocking connect to tracker %s:%s. %s.\n", tracker_hostname,
								 tracker_port_string,
								 strerror(errno));
							tracker_address = tracker_address->ai_next;
							continue;
						}
						if (optval == 0) {
							// successfully connected
							printf("Connected to tracker %s:%s\n", tracker_hostname, tracker_port_string);
							break;
						}
						else {
							tracker_address = tracker_address->ai_next;
                                                        continue;
						}
					}
				}	
			}
			if (!tracker_address) {
				free(tracker_hostname);
				continue;
			}
			char tracker_http_request[700] = {0};
			char port[10] = {0};
			sprintf(port, "%d", local_listening_port);
			sprintf(tracker_http_request, 
				"GET /announce?info_hash=%s&peer_id=%s&ip=%s&port=%s&uploaded=0&downloaded=0&left=0&event=started HTTP/1.1\r\nHost: %s\r\n\r\n", 
				 percent_encoded_info_hash,
				 percent_encoded_peer_id,
				 local_ipv4_address,
				 port,
				 tracker_hostname);
			printf("Tracker http request:\n%s", tracker_http_request);	
			size_t tracker_http_request_len = strlen(tracker_http_request);
			
			// make client_socket_fd blocking again
			int client_socket_file_status_flags = fcntl(client_socket_fd, F_GETFL);
			client_socket_file_status_flags &= (~O_NONBLOCK);
			fcntl(client_socket_fd, F_SETFL, client_socket_file_status_flags);

			int total_bytes_sent = 0;
			int bytes_sent = 0;
			while (total_bytes_sent != tracker_http_request_len) {
				bytes_sent = send(client_socket_fd, 
				     		      &(tracker_http_request[total_bytes_sent]), 	
				     		      tracker_http_request_len - total_bytes_sent,
				     		      0);
				if (bytes_sent == -1){
					printf("Error sending tracker http announce request to tracker %s:%s. %s.\n", tracker_hostname, tracker_port_string, strerror(errno));
					break;
				}
				else {
					total_bytes_sent += bytes_sent;
				}
			}
			if (bytes_sent == -1) {
				free(tracker_hostname);
                                continue;
			}
			int tracker_http_announce_response_len = 500000;
			char *tracker_http_announce_response = (char *)malloc(tracker_http_announce_response_len);
			if (tracker_http_announce_response == NULL) {
				printf("Error allcoating memory for tracker announce http response. %s.\n", strerror(errno));
				exit(EXIT_FAILURE);
			}
			memset(tracker_http_announce_response, 
                               0, 
			       tracker_http_announce_response_len);
			int total_bytes_recvd = 0;
			while (1) {
				int bytes_recvd = recv(client_socket_fd, 
						       &(tracker_http_announce_response[total_bytes_recvd]), 
							tracker_http_announce_response_len - total_bytes_recvd,
							0);
				if (bytes_recvd == -1) {
					printf("Error occurred whilst recving http tracker announce response from tracker %s:%s. %s.\n", tracker_hostname, tracker_port_string, strerror(errno));
					free(tracker_hostname);
					continue;
				}
				else if (bytes_recvd == 0) {
					printf("Error occurred whlst recving tracker announce response from tracker %s:%s. Tracker closed connection.\n", tracker_hostname, tracker_port_string);
					free(tracker_hostname);
					continue;
				}
				else {
					total_bytes_recvd += bytes_recvd;
					printf("%s\n", tracker_http_announce_response);							  break;	
				}
			}
		}
	}
}

