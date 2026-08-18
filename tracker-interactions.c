/*=========================================;
 *
 * File: tracker-interactions.c
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
#include <time.h>
#include <math.h>

unsigned char *get_transaction_id()
{
	unsigned char *transaction_id = (unsigned char *)malloc(4);
	if (transaction_id == 0) {
		printf("Error allocating memory for transaction id during tracker connection request. %s.\n", strerror(errno));
	}

	srand(time(NULL));
	for (int i=0; i<4; ++i) {
		transaction_id[i] = rand() % 256;
	}
	return transaction_id;
}

struct peer
{
	unsigned char ip[4]; // internet protocol address
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
	printf("{ ip:");
	for (int i=0; i<4; ++i) {
		printf("%u",(p->ip)[i]);
		if (i<3) {
			printf(".");
		}	
	}
	printf(", port: %u}", p->port);
}

int timeout0(int n)
{
	return (int)(pow((double)2,(double)n)*15);
}

// returns 0 on filure, 1 on success
int send_connect_request(int client_socket_fd,
			  unsigned char transaction_id[4],
			  unsigned char protocol_id[8])
{
	unsigned char connect_request[16] = {0};
	memcpy(connect_request, protocol_id, 8);
	memcpy(&(connect_request[12]), transaction_id, 4);
	ssize_t bytes_sent = 0;
	while (bytes_sent < 16) {
		ssize_t sent_bytes = send(client_socket_fd,
	            	 		&(connect_request[bytes_sent]), 
		    	 		16 - bytes_sent,
					0);
		if (sent_bytes == -1) {
			printf("Error sending connect request to tracker during tracker announce.\n%s", strerror(errno));
			return 0;
		}
		bytes_sent += sent_bytes;
	}
	return 1;
}

// returns 0 on error. 1 on failure to send due to having the same connect_id
// for more than 1 minute. 2 on success
int send_announce_request(int client_socket_fd,
			  time_t connect_id_recvd,
			  unsigned char connection_id[8],
    			  unsigned char transaction_id[4],
			  unsigned char* info_hash,
			  unsigned char* peer_id,
			  unsigned char key[4])
{

	unsigned char announce_request[98] = {0};
	memcpy(announce_request, connection_id, 8);
	announce_request[11] = 1;
	memcpy(&(announce_request[12]), transaction_id, 4);
	memcpy(&(announce_request[16]), info_hash, 20);
	memcpy(&(announce_request[36]), peer_id, 20);
	memset(&(announce_request[64]), 255, 8); 
	announce_request[83] = 2;
	// private ip address - default to 0
	/*announce_request[84] = 192;
	announce_request[85] = 168;
	announce_request[86] = 0;
	announce_request[87] = 237; */
	memcpy(&(announce_request[88]), key, 4); // client identifying key
	// num_want details the max number of peers returned by tracker
	memset(&(announce_request[92]), 255, 4); // default maximum number of peers
	// 6881 = 1ae1 in hex which is port number
	announce_request[96] = 0x1a;
	announce_request[97] = 0xe5;
	int total_bytes_sent = 0;
	while (total_bytes_sent < 98) {
		time_t now = time(NULL);
		double time_had_conn_id = difftime(now, connect_id_recvd);
		if (time_had_conn_id > 60.0) {
			return 1;
		}
		ssize_t bytes_sent = send(client_socket_fd,
					  &(announce_request[total_bytes_sent]),
					  98 - total_bytes_sent,
					  0);
		if (bytes_sent == -1) {
			printf("Error occurred sending tracker announce request.\n%s.", strerror(errno));
			return 0;
		}
		total_bytes_sent += bytes_sent;
	}
	return 2;
}

// returns -1 if a timeout occurred waiting for a response. In this case an 
// another announce request will have to be sent again with an incremented
// timeout factor n
// returns -2 if an error occurred whilst waiting for response. In this case
// another announce request should be sent with an incremented timeout factor n
// returns nonnegative value on success which gives the number of peers in
// the buffer at address *announce_response 
int recv_announce_response(int client_socket_fd,
			   unsigned char transaction_id[4],
			   unsigned char **announce_response,
			   int *n)
{
	
	int number_of_retries = *n;
	struct pollfd fds;
	fds.fd = client_socket_fd;
	fds.events = POLLIN;
	int poll_res = poll(&fds, 1, timeout0(number_of_retries)*1000);
	if (poll_res == -1) {
		printf("Error occurred whilst calling poll waiting for tracker announce response. %s.\n",
			strerror(errno));
		return -2;
	}
	if (poll_res == 0) {
		// a timeout occurred, so we will need to resend the announnce
		// request
		return -1;
	}
	else if (fds.revents & POLLIN) {
		// allocate memory for the announce_response buffer 
		// that allows for a maximum of 1000 peers
		*announce_response = (unsigned char *)malloc(6020);
		if ((*announce_response) == NULL) {
			printf("Error allocating memory for announce_response. %s", strerror(errno));
			return -2;
		}
		int total_bytes = 0;
		int recvd_bytes = recv(client_socket_fd,
					*announce_response,
					6020,
					0);
		total_bytes += recvd_bytes;
		recvd_bytes = 0;
		unsigned char timeout_occurred = 0;
		while (!timeout_occurred) {
			fds.events = POLLIN;
			poll_res = poll(&fds, 
					1, 
					timeout0(number_of_retries)*1000);
			if (poll_res == -1) {
				free(*announce_response);
				return -2;
			}
			else if (poll_res == 0) {
				timeout_occurred = 1;
				break;
			}
			else {
				if (fds.revents & POLLIN) {
					recvd_bytes = recv(client_socket_fd,
							   &((*announce_response)[total_bytes]),
						   	   6020 - total_bytes,
							0);
		     			total_bytes += recvd_bytes;			
				}
				else {
					// error occurred whilst waiting
					free(*announce_response);
					return -2;
				}
			}
		}
		if (timeout_occurred) {
			unsigned char *announce_response_buffer = realloc(*announce_response, total_bytes);
			*announce_response = announce_response_buffer;
			unsigned char valid_announce_response = 1;
			for (int i=0; i<3; ++i) {
				if (announce_response_buffer[i] != 0) {
					valid_announce_response = 0;
					free(announce_response_buffer);
					return -2;
				}
			}
			if (announce_response_buffer[3] != 1) {
				valid_announce_response = 0;
				free(announce_response_buffer);
				return -2;
			}
			if (total_bytes < 20) {
				valid_announce_response = 0;
				free(announce_response_buffer);
				return -2;
			}
			else {
				for (int i=0; i<4; ++i) {
					if (announce_response_buffer[4+i] != transaction_id[i]) {
						valid_announce_response = 0;
						free(announce_response_buffer);
						break;			
					}
				}
			}
			if (!valid_announce_response) {
				return -2;
			}
			if ( ((total_bytes - 20) % 6) != 0) {
				// have recvd data off boundary
				free(announce_response_buffer);
				return -2;
			}
			int number_of_peers_obtained = (total_bytes-20)/6;
			return number_of_peers_obtained;
		}
	}

}

struct vector *get_peer_list_from_trackers(struct vector *trackers, 
					   unsigned char *info_hash,
					   unsigned char *peer_id)
{
	int max_number_of_peers = 100;
	int no_of_trackers = vector_get_size(trackers);
	struct vector *peers = vector_null_init(sizeof(struct peer), print_peer);
	unsigned char key[4] = { rand() % 256, rand() % 256, rand() % 256, rand() % 256};
	for (int i=0; i<no_of_trackers; ++i) {
		if (vector_get_size(peers) >max_number_of_peers) {
			return peers;
		}
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
		if (tracker_port_string[0] == 0) {
			// no udp port number specified to announce to
			continue;
		}
		// tracker announce over udp
		int client_socket_fd = socket(AF_INET, SOCK_DGRAM, 17);
		struct addrinfo hints;
                hints.ai_flags = 0;
                hints.ai_family = AF_INET;
                hints.ai_socktype = SOCK_DGRAM;
                hints.ai_protocol = 17;
                hints.ai_addrlen = 0;
                hints.ai_addr = NULL;
                hints.ai_canonname = NULL;
                hints.ai_next = NULL;
                struct addrinfo *tracker_address = NULL;
		printf("Trying to get address of tracker %s:%s\n", tracker_hostname, tracker_port_string);
		if (getaddrinfo(tracker_hostname, tracker_port_string, &hints, &tracker_address) != 0) {
                        free(tracker_hostname);
                        continue;
                }
		else {
			unsigned char protocol_id[8] = {0x00, 0x00, 0x04, 0x17, 0x27, 0x10, 0x19, 0x80};
			// tracker announce over udp
			unsigned char error_occurred = 0;
			int n = 0;
			while (tracker_address) {
				if (connect(client_socket_fd, tracker_address->ai_addr, tracker_address->ai_addrlen)==-1) {
					printf("Error connecting client sock_dgram socket to tracker. %s\n", 
						strerror(errno));
					tracker_address = tracker_address->ai_next;
					n=0;
					continue;
				}
				n = 0;
				connect_id_expired:
				unsigned char *transaction_id = get_transaction_id();
				int bytes_sent = 0;
				error_occurred = 0;
				// send connect_request
				printf("Sending connect request to tracker %s:%s.\n", tracker_hostname, tracker_port_string);
				int connect_request_res = send_connect_request(client_socket_fd,
									       transaction_id,
									       protocol_id);

				if (connect_request_res == 0) {
					// sending connection request resulted in error
					tracker_address = tracker_address->ai_next;
					free(transaction_id);
					continue;
				}
				unsigned char received_connect_response = 0;
				error_occurred = 0;
				unsigned char connect_response[16] = {0};
				int total_bytes = 0;
				while (n<8) {
					struct pollfd read_fds;
					read_fds.fd = client_socket_fd;
					read_fds.events = POLLIN;
					int pollres = poll(&read_fds, 1, timeout0(n)*1000);
					if (pollres > 0) {
						if (read_fds.revents & POLLIN) {
							while (total_bytes < 16) {
								printf("Receiving bytes from tracker %s:%s connect response.\n", tracker_hostname, tracker_port_string);
								int bytes_recvd = recv(client_socket_fd, 
									       	   &(connect_response[total_bytes]), 
									       	   16 - total_bytes,
								       	           0);
								if (bytes_recvd == -1) {
									printf("Error receiving connect response bytes. %s.\n",
										strerror(errno));
									error_occurred = 1;
									break;
								}
								total_bytes += bytes_recvd;		
								if (total_bytes < 16) {
									continue;
								}
								else {
									received_connect_response = 1;
									break;
								}
							}
							if (error_occurred) {
								break;	
							}
							if (received_connect_response) {
								break;
							}
						}
						else {
							// error_occurred
							error_occurred = 1;
							break;
						}
					}
					else {
						// timeout occurred.
						// resend connect_request
						int conn_req_res = send_connect_request(
									client_socket_fd,
								        transaction_id,
								        protocol_id);
						++n;
						if (conn_req_res == 0) {
							error_occurred = 1;
							break;
						}
					}
				}
				if (error_occurred || (n>=8) ) {
					tracker_address = tracker_address->ai_next;
					free(transaction_id);
					continue;
				}
				else if (received_connect_response) {
					printf("Recevied tracker connect response from tracker %s:%s\n", tracker_hostname, tracker_port_string);
					unsigned char connect_action = 1;
					for (int i=0; i<4; ++i) {
						if (connect_response[i] != 0) {
							connect_action = 0;
						}
					}
					if (!connect_action) {
						++n;
						goto connect_id_expired; // retransmit connect response and increment n
					}
					unsigned char transaction_id_match = 1;
					for (int i=0; i<4; ++i) {
						if ( (connect_response[4+i]) != (transaction_id[i]) ) {
							transaction_id_match = 0;
						}
					}
					if (!transaction_id_match) {
						++n; // retransmit connect_response and increment n
						goto connect_id_expired;
					}
					free(transaction_id);
				        transaction_id = get_transaction_id();
					unsigned char connection_id[8] = {0};
					memcpy(connection_id, &(connect_response[8]), 8);	
announce_request:
					time_t connect_id_recvd = time(NULL);
					printf("Sending tracker announce request over UDP to tracker %s:%s.\n", tracker_hostname, tracker_port_string);
					int announce_req_res = send_announce_request(
								    client_socket_fd,
                          				            connect_id_recvd,
                          				            connection_id,
                          				      	    transaction_id,
                          				      	    info_hash,
                          				      	    peer_id,
								    key);
					if (announce_req_res == 0) {
						tracker_address = tracker_address->ai_next;
						free(transaction_id);
						continue;		
					}		
					else if (announce_req_res == 1) {
						// connect_id expired
						++n;
						free(transaction_id);
						goto connect_id_expired;
					}		
					else {
						// announce request sent
						unsigned char *announce_response = 0;
						int response_res = recv_announce_response(
									client_socket_fd, 
									transaction_id,
									&announce_response,
									&n);
						if ((response_res == -2) || (response_res == -1)) {
							// -2 error occurred whilst
							// waiting for announce_response. resend announce request withthe incremented n
							// -1 indicates a timeout occurred. resend announce request and incremebnt n
							++n;
							free(transaction_id);
							goto announce_request;
						}	
						else {
							printf("Received valid tracker announce response from tracker %s:%s\n", tracker_hostname, tracker_port_string);
							int number_of_peers = response_res;
							printf("Received valid tracker announce response from tracker %s:%s containing %d peers.\n", tracker_hostname, tracker_port_string, number_of_peers);
							for (int i=0; i<number_of_peers; ++i) {
								int peer_index = 20 + (6*i);
								struct peer p;
							 	for (int j=0; j<4; ++j) {
									(p.ip)[j] = announce_response[peer_index+j];
									(p.port) = 0;
									(p.port) = ((announce_response[peer_index+4])*256) + announce_response[peer_index+5];	
								}	
								if ( vector_search(peers, &p) == -1) {
									vector_push_back(peers, &p);
								}
							}
							free(transaction_id);
							free(announce_response);
							break;
						}
					}		     
				}
			}
		}
	}
	return peers;
}

