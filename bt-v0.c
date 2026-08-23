/*==========================
 *
 * File: bt-v0.c
 * Content: BitTorrent implementation
 * for use with the command line.
 * Date: 2/6/2026
 * Author: Andy Oldham
 * Compile and link:
 * compile: gcc -I ~/Documents/C-containers -c bt-v0.c
 * link: gcc -o bt-v0 bt-v0.o tracker-interactions.o ml.o ~/Documents/C-containers/vector.o ~/Documents/C-containers/binary_tree.o -lm
 * run: ./bt-v0 "XXX-magnet-link-XXX"
 *
 *********************************/

#include "ml.h"
#include "tracker-interactions.h"
#include "peer-interactions.h"
#include "vector.h"
#include "binary_tree.h"
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
#include <poll.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <unistd.h>

// returns x to the power of y
uint32_t int_pow(uint32_t x, uint32_t y)
{
	uint32_t res = 1;
	for (uint32_t i=0; i<y; ++i) {
		res *= x;
	}
	return res;
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
unsigned char *get_bt_handshake(unsigned char *protocol_string,
				unsigned char *reserved_bytes,
				unsigned char *info_hash,
				unsigned char *peer_id)
{
	unsigned char *bt_handshake = (unsigned char *)malloc(68);
	if (bt_handshake == 0) {
		printf("Error allocating memory for bittorrent handshake. %s.\n",
			strerror(errno));
		return 0;
	}
	bt_handshake[0] = 19;
	memcpy(&(bt_handshake[1]),
	       protocol_string,
       	       19);
	memcpy(&(bt_handshake[20]),
		reserved_bytes,
		8);
	memcpy(&(bt_handshake[28]),
		info_hash,
		20);
	memcpy(&(bt_handshake[48]),
		peer_id,
		20);
	return bt_handshake;	
}

// receives the bittorrent handshake (68 bytes) from the peer connected
// through client_socket_fd.
// returns the handshake or a null poitner on error or timeout.
// we wait for 60 seconds between data receives to check for more data up to
// 68 bytes, the length of the complete handshake
unsigned char *recv_bittorrent_handshake(int client_socket_fd,
					 struct peer *curr_peer)
{
	unsigned char *bittorrent_handshake_recv = (unsigned char *)malloc(68);
	if (bittorrent_handshake_recv == 0) {
		printf("Error allocating memory o receive bittorrent handshake from peer ");
		print_peer(curr_peer);
		printf(".\n");
		return 0;
	}
	int total_bytes_recvd = 0;
	while (total_bytes_recvd != 68) {
		struct pollfd fds;
		fds.fd = client_socket_fd;
		fds.events = POLLIN;
		// wait for up to 60 seconds for data to arrive
		int poll_res = poll(&fds, 1, 60000);
		if (poll_res == -1) {
			printf("An error occurred receiving bittorrent handshake from peer ");
			print_peer(curr_peer);
			printf("\n%s\n", strerror(errno));
			free(bittorrent_handshake_recv);
			return 0;
		}
		else if (poll_res == 0) {
			// timout occurred whilst waiting for data
			printf("Timeout of 60 seconds occurred whilst witing for peer handshake data from peer ");
			print_peer(curr_peer);
			printf(".\n");
			free(bittorrent_handshake_recv);
			return 0;
		}
		else {
			if (fds.revents & POLLIN) {
				// there is data to be read
				int bytes_recvd = recv(
					client_socket_fd,
					&(bittorrent_handshake_recv[total_bytes_recvd]),
					68 - total_bytes_recvd,
					0);
				if (bytes_recvd == -1) {
					printf("Error occurred whilst receiving bittorrent handshake from peer ");
					print_peer(curr_peer);
					printf(" %s.\n", strerror(errno));
					free(bittorrent_handshake_recv);
					return 0;
				}  
				total_bytes_recvd += bytes_recvd;
			}
		}
	}
	return bittorrent_handshake_recv;
}

// bt_handshake is 68 bytes of bittorremt handshake and info_hash is the 
// 20 byte raw info hash. The function returns 0 if the 68 byte 
// handhsake is invalid, 1 if the handhsake is valid
unsigned char validate_handshake(unsigned char *bt_handshake,
				unsigned char *info_hash)
{
	if (bt_handshake[0] != 19) {
		printf("Received handshake fails first byte test equal to 19.\n");
		return 0;
	}
	unsigned char *protocol_string = "BitTorrent protocol";
	if (strncmp(&(bt_handshake[1]), protocol_string, 19) != 0) {
		printf("Received handhsake fails protocol string 'Bittorrent protocol' match.\n");
		return 0;	
	}
	for (int i=0; i<8; ++i) {
		if (i!=5) {
				
		}
		else {
			unsigned char curr_byte = bt_handshake[20+i];
			if ((curr_byte != 0x00) && (curr_byte != 0x10)) {
				printf("Bittorrent hadnshake rceived fails validation due to error in reserved bytes.\n");
				return 0;
			}
		}
	}
	for (int i=0; i<20; ++i) {
		if ( (bt_handshake[28+i]) != (info_hash[i]) ) {
			printf("Received handshake error due to info hash mismatch.\n");
			return 0;
		}
	}
	return 1;
}

//returns 0 on error occurred. or an unsigned integer greater than 4 representing the total bittorrent message length
//in bytes. Greter than 4 length means that we skip bittorrent keep alive messages.
uint32_t get_message_length(int client_socket_fd, struct peer *curr_peer)
{
	uint32_t message_length = 0;	
	unsigned char message_length_buffer[4];
	uint32_t total_bytes_recvd = 0;
	unsigned char error_occurred = 0;
	while (message_length <= 4) {
		message_length = 0;
		total_bytes_recvd = 0;
		while (total_bytes_recvd != 4) {
			int bytes_recvd = recv(client_socket_fd,
						&(message_length_buffer[total_bytes_recvd]),
						4 - total_bytes_recvd,
						0);
			if (bytes_recvd == -1) {
				printf("Error occurred whilst receiving first bittorrent message after handshake from peer ");
				print_peer(curr_peer);
				printf(". %s\n", strerror(errno));
				error_occurred = 1;
				break;
			}
			else if (bytes_recvd == 0) {
				// peer performed socket shutdown
				printf("Peer ");
				print_peer(curr_peer);
				printf(" performed socked shutdown whilst client was receiving first bittorrent message after handshake.\n");
				error_occurred = 1;
				break;
			}
			else {
				total_bytes_recvd += bytes_recvd;
			}
		}
		if (error_occurred) {
			break;
		}
		unsigned char *message_length_addr = (unsigned char *)(&message_length);

		*message_length_addr = message_length_buffer[3];
		++message_length_addr;
		*message_length_addr = message_length_buffer[2];
		++message_length_addr;
		++message_length_addr;
		*message_length_addr = message_length_buffer[1];
		++message_length_addr;
		*message_length_addr = message_length_buffer[0];
	}
	if (error_occurred) {
		return 0;
	}
	return message_length;
}

// returns a bittorrent message of message_length size (the bt mesasge excluding the inital 4 bytes total length)
// returns 0 on error
unsigned char *get_bt_message(int client_socket_fd, uint32_t message_length, struct peer *curr_peer)
{
	unsigned char *bt_message = (unsigned char *)malloc(message_length);
	if (bt_message == 0) {
		printf("Error allocating memory for bt message. %s.\n", strerror(errno));
		return 0;
	}
	uint32_t total_bytes_recvd = 0;
	while (total_bytes_recvd != message_length) {
		long long int bytes_recvd = recv(client_socket_fd,
				       &(bt_message[total_bytes_recvd]),
				       message_length - total_bytes_recvd,
				       0);
		if (bytes_recvd == -1) {
			printf("Error receiving bt message from peer ");
			print_peer(curr_peer);
			printf(". %s.\n", strerror(errno));
			return 0;
		}
		else if (bytes_recvd == 0) {
			printf("Error receiving bt message from peer ");
			print_peer(curr_peer);
			printf(" .\n");
			return 0;
		}
		else {	
			total_bytes_recvd += bytes_recvd;
		}	
	}
	return bt_message;
}

// circular left shift by 1 bit position
void circular_left_shift_1bit(unsigned char w[4])
{
	unsigned char circled_bit = (w[0] & 0x80) >> 7;
	w[0] = w[0] << 1;
	if (w[1] & 0x80) {
		w[0] += 1;
	}	
	w[1] = w[1] << 1;
	if (w[2] & 0x80) {
		w[1] += 1;
	}
	w[2] = w[2] << 1;
	if (w[3] & 0x80) {
		w[2] += 1;
	}
	w[3] = w[3] << 1;
	w[3] = w[3] + circled_bit;
}

// circular left shift w by n bits where 0 <= n <32
void circular_left_shift(uint32_t *w, unsigned char n)
{

	uint32_t res = ((*w) << n) | ((*w) >> (32-n));
	*w = res;
}

// returns an unsigned char pointer which is the address of 4 unsigned chars
// or returns the null pointer on error
uint32_t k_t(int t) {
	if ((t >= 0) && (t<=19)) {
		return 0x5A827999;
	}
	else if ( (t>=20) && (t<=39) ) {
		return 0x6ED9EBA1;
	}
	else if ( (t>=40) && (t<=59) ) {
		return 0x8F1BBCDC;
	}
	else if ( (t>=60) && (t<= 79)) {
		return 0xCA62C1D6;
	}
}

// returns a dynamically allocated array of 4 unsigned chars or the null pointer
// on failure
uint32_t f_t(int t, uint32_t B, uint32_t C, uint32_t D)
{
	uint32_t res = 0;
	if ((t>=0) && (t<=19)) {
		res = (B & C) | (~(B) & D);
		return res;
	}
	else if ( (t>= 20) && (t<=39) ) {
		res = B ^ C ^ D;
		return res;
	}
	else if ( (t>=40) && (t<= 59) ) {
		res = (((B & C) | (B & D)) | (C & D));
		return res;
	}
	else if ( (t>=60) && (t<=79) ) {
		res = B ^ C ^ D;
		return res;
	}
}


// performs an sha1 hash on the data of length bytes and returns a 20
// byte digest.
// returns the null pointer on error.
// data has been mallocated previously
unsigned char *sha_1_hash(unsigned char *data, unsigned long long int length)
{
	if (data == 0) {
		return 0;
	}
	uint64_t data_bit_length = length*8;

	if ( (data_bit_length % 512) != 0) {
		//perform padding
		if ( (data_bit_length % 512) <= (512 - 65) ) {
			unsigned long long int padding_bits = 512 - (data_bit_length % 512);
			if ((padding_bits % 8) != 0) {
				printf("Error in the number of padding bits required. SHould be a multiple of 8.\n");
				return 0;
			}	
			unsigned long long int no_of_padding_bytes = padding_bits/8;
			void *buff = realloc(data, length + no_of_padding_bytes);
			data = (unsigned char *)buff;
			unsigned char *original_bit_length = (unsigned char *)(&data_bit_length);
			uint64_t padded_length = length + no_of_padding_bytes;
			data[padded_length-1] = *original_bit_length;
			++original_bit_length;
			data[padded_length-2] = *original_bit_length;
			++original_bit_length;
			data[padded_length-3] = *original_bit_length;
			++original_bit_length;
			data[padded_length-4] = *original_bit_length;
			++original_bit_length;
			data[padded_length-5] = *original_bit_length;
                        ++original_bit_length;
                        data[padded_length-6] = *original_bit_length;
                        ++original_bit_length;
                        data[padded_length-7] = *original_bit_length;
                        ++original_bit_length;
                        data[padded_length-8] = *original_bit_length;
			data[length] = 0x80; // append 1
			memset(&(data[length+1]), 0, (padded_length-9)-(length+1)+1);
			data_bit_length = ((length+no_of_padding_bytes)*8);
		}
		else {
				
			uint64_t padding_bytes = ((512 - (data_bit_length % 512))/8)						 + 64;
			data = realloc(data, length+padding_bytes);
			data[length] = 0x80;
			unsigned char *original_bit_length = (unsigned char *)(&data_bit_length);
			uint64_t padded_length = length + padding_bytes;
			data[padded_length-1] = *original_bit_length;
			++original_bit_length;
			data[padded_length-2] = *original_bit_length;
			++original_bit_length;
			data[padded_length-3] = *original_bit_length;
			++original_bit_length;
			data[padded_length-4] = *original_bit_length;
			++original_bit_length;
			data[padded_length-5] = *original_bit_length;
                        ++original_bit_length;
                        data[padded_length-6] = *original_bit_length;
                        ++original_bit_length;
                        data[padded_length-7] = *original_bit_length;
                        ++original_bit_length;
                        data[padded_length-8] = *original_bit_length;
			memset(&(data[length+1]), 0, (padded_length-9) - (length+1) + 1);		
			data_bit_length = ((length+padding_bytes)*8);
		}
	}
	printf("Data bit length after padding: %lu\n", data_bit_length);
	printf("Padded message:\n");
	for (int i=0; i< (data_bit_length/8); ++i) {
		printf("%2x", data[i]);
	}
	printf("\n");
	// padding if necessary is complete
	uint32_t A = 0, B=0, C=0, D=0, E=0;
	uint32_t H0=0, H1=0, H2=0, H3=0, H4=0;
	uint32_t W[80] = {0}; // 80, 32 bit words
	uint32_t TEMP = {0};
	if ((data_bit_length % 512) != 0) {
		printf("Error during padding as padded message not a multiple of 512 bits.\n");

	}
	uint32_t n = data_bit_length / 512;
	H0 = 0x67452301;
	H1 = 0xEFCDAB89;
	H2 = 0x98BADCFE;
	H3 = 0x10325476;
	H4 = 0xC3D2E1F0;	
	for (int i=0; i<n; ++i) {
		for (int j=0; j<16; ++j) {
			W[j] = (data[(i*64)+(j*4)] << 24) +
			       (data[(i*64)+(j*4)+1] << 16) +
			       (data[(i*64)+(j*4)+2] << 8) +
			       (data[(i*64)+(j*4)+3]);
		}
		for (int t=16; t<=79; ++t) {
			uint32_t w_temp = W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16];
			circular_left_shift(&w_temp, 1);
			W[t] = w_temp;
		}
		A = H0;
		B=H1;
		C=H2;
		D=H3;
		E=H4;
		for (int t=0; t<=79; ++t) {
			uint32_t A_shifted = A;
			circular_left_shift(&A_shifted, 5);
			uint32_t f = f_t(t, B, C, D);
			uint32_t k = k_t(t);
			TEMP = A_shifted + f + E + W[t] + k;
			E=D;
			D=C;
			uint32_t B_shifted = B;
			circular_left_shift(&B_shifted, 30);
			C=B_shifted;
			B=A;
			A=TEMP;
		}
		// H0 = H0 + A
		H0 += A;
		H1 += B;
		H2 += C;
		H3 += D;
		H4 += E;
	}
	unsigned char *res = (unsigned char *)malloc(20);
	if (res == 0) {
		printf("Error allocating memory for 20 byte sha1 digest result.\n");
		return 0;
	}
	unsigned char *H0_addr = (unsigned char *)(&H0);
	H0_addr += 3;
	res[0] = *H0_addr;
	--H0_addr;
	res[1] = *H0_addr;
	--H0_addr;
	res[2] = *H0_addr;
	--H0_addr;
	res[3] = *H0_addr;
	unsigned char *H1_addr = (unsigned char *)(&H1);
        H1_addr += 3;
        res[4] = *H1_addr;
        --H1_addr;
        res[5] = *H1_addr;
        --H1_addr;
        res[6] = *H1_addr;
        --H1_addr;
        res[7] = *H1_addr;
	unsigned char *H2_addr = (unsigned char *)(&H2);
        H2_addr += 3;
        res[8] = *H2_addr;
        --H2_addr;
        res[9] = *H2_addr;
        --H2_addr;
        res[10] = *H2_addr;
        --H2_addr;
        res[11] = *H2_addr;
	unsigned char *H3_addr = (unsigned char *)(&H3);
        H3_addr += 3;
        res[12] = *H3_addr;
        --H3_addr;
        res[13] = *H3_addr;
        --H3_addr;
        res[14] = *H3_addr;
        --H3_addr;
        res[15] = *H3_addr;
	unsigned char *H4_addr = (unsigned char *)(&H4);
        H4_addr += 3;
        res[16] = *H4_addr;
        --H4_addr;
        res[17] = *H4_addr;
        --H4_addr;
        res[18] = *H4_addr;
        --H4_addr;
        res[19] = *H4_addr;
	return res;
}

// return 0 on failure, 1 on success
unsigned char insert_piece_into_file(unsigned char *piece, struct metadata_info *mi, long long int piece_index)
{
	if (mi->length) {
		char *download_prefix = "~/Downloads/";
		char *file_name = (char *)malloc(strlen(download_prefix) + strlen(mi->name) + 1);
		memcpy(file_name, download_prefix, strlen(download_prefix));
		memcpy(&(file_name[strlen(download_prefix)]), mi->name, strlen(mi->name));
		file_name[strlen(download_prefix) + strlen(mi->name)] = 0;
		FILE *f = fopen(file_name, "r+");
		if (f==0) {
			printf("Error opening downloaded file to write downloaded data to. %s.\n",
					strerror(errno));
			return 0;
		}		
		else {
			if ( fseek(f, SEEK_SET, piece_index*(mi->piece_length)) == -1) {
				printf("Error seeking to file location in order to write data downloaded data into the file. %s.\n",
					strerror(errno));
				return 0;
			}
			else {
				if ( fwrite(piece, 1, mi->piece_length, f) != (mi->piece_length) ) {
					printf("Error occurred writing downloaded data to file.\n");
					return 0;
				}
				else {
					printf("Successfully written sha1 validated piece to downloaded file.\n");
					return 1;
				}
				
			}
		}
	}
	else {
		// download represents multiple files
		uint64_t download_offset = piece_index*(mi->piece_length);
		
	}
}

	


// the first byte of message indicates the message type as follows
// 0 - choke
// 1 - unchoke
// 2 - interested
// 3 - not interested
// 4 - have
// 5 - bitfield
// 6 - request
// 7 - piece
// 8 - cancel
//returns 0 on failure, 1 on successful processing of message
int process_message( int client_socket_fd,
		      struct peer *curr_peer,
			unsigned char *message,
			uint32_t message_length,
			struct peer_metadata *peer_data,
			struct peer_interactions_metadata *pim)
{
	unsigned char message_type = message[0];
	switch (message_type) {
		case 0: {
			// message_type is choke, the peer has choked the client
			peer_data->peer_choke = 0;
			break;
		}
		case 1: {
			peer_data->peer_choke = 1;
			break;
		}
		case 2: {
			peer_data->peer_interest = 1;
			break;
		}
		case 3: {
			peer_data->peer_interest = 0;
			break;
		}
		case 4: {
			if (message_length != 5) {
				printf("Error processing 'have' message from peer ");
				print_peer(curr_peer);
				printf(" . Message length mismatch.\n");
			       return 0;	
			}
			else {
				uint32_t piece_index = 0;
				unsigned char *piece_index_addr = (unsigned char *)(&piece_index);
				*piece_index_addr = message[4];
				++piece_index_addr;
				*piece_index_addr = message[3];
				++piece_index_addr;
				*piece_index_addr = message[2];
				++piece_index_addr;
				*piece_index_addr = message[1];
				btree_insert(peer_data->pieces_peer_has, &piece_index);
				return 1;
			}
		}
		case 5: {
			unsigned char bit0 = 0x80;
			unsigned char bit1 = 0x40;
			unsigned char bit2 = 0x20;
			unsigned char bit3 = 0x10;
			unsigned char bit4 = 0x08;
			unsigned char bit5 = 0x04;
			unsigned char bit6 = 0x02;
			unsigned char bit7 = 0x01;
			for (int i=0; i<(message_length-1); ++i) {
				uint32_t piece_index = 0;
				if ((message[1+i]) & bit0) {
					piece_index = 8*i;
					btree_insert(peer_data->pieces_peer_has, &piece_index);
				}
				if ( (message[1+i]) & bit1 ) {
					piece_index = (8*i)+1;
					btree_insert(peer_data->pieces_peer_has, &piece_index);
				}
				if ( (message[1+i]) & bit2 ) {
					piece_index = (8*i)+2;
					btree_insert(peer_data->pieces_peer_has, &piece_index);
				}
				if ((message[1+i]) & bit3) {
                                        piece_index = (8*i) + 3;
                                        btree_insert(peer_data->pieces_peer_has, &piece_index);
                                }
                                if ( (message[1+i]) & bit4 ) {
                                        piece_index = (8*i)+4;
                                        btree_insert(peer_data->pieces_peer_has, &piece_index);
                                }
                                if ( (message[1+i]) & bit5 ) {
                                        piece_index = (8*i)+5;
                                        btree_insert(peer_data->pieces_peer_has, &piece_index);
                                }
				if ( (message[1+i]) & bit6 ) {
                                        piece_index = (8*i)+6;
                                        btree_insert(peer_data->pieces_peer_has, &piece_index);
                                }
                                if ( (message[1+i]) & bit7 ) {
                                        piece_index = (8*i)+7;
                                        btree_insert(peer_data->pieces_peer_has, &piece_index);
                                }
			}
			return 1;
		}
		case 6: {
			if (message_length != 13) {
				printf("Error processing 'request' message from peer ");
				print_peer(curr_peer);
				printf(" . Incorrect message length.\n");
				return 0;
			}
			else {
				struct request *peer_req = (struct request *)malloc(sizeof(struct request));
				if (peer_data->pr ) {
					free(peer_data->pr);
				}
				uint32_t index = 0;
				uint32_t begin = 0;
				uint32_t length = 0;
				unsigned char *index_addr = (unsigned char *)(&index);
				unsigned char *begin_addr = (unsigned char *)(&begin);
				unsigned char *length_addr = (unsigned char *)(&length);
				*index_addr = message[4];
				++index_addr;
				*index_addr = message[3];
				++index_addr;
				*index_addr = message[2];
				++index_addr;
				*index_addr = message[1];
				*begin_addr = message[8];
                                ++begin_addr;
                                *begin_addr = message[7];
                                ++begin_addr;
                                *begin_addr = message[6];
                                ++begin_addr;
                                *begin_addr = message[5];
				*length_addr = message[12];
                                ++length_addr;
                                *length_addr = message[11];
                                ++length_addr;
                                *length_addr = message[10];
                                ++length_addr;
                                *length_addr = message[9];
				peer_req->index = index;
				peer_req->begin = begin;
				peer_req->length = length;
				peer_data->pr = peer_req;
				return 1;
			}
		}
		case 7: {
			if (message_length < 10) {
				// piece doesnt contain any data
				printf("Error receiving peice message from peer ");
				print_peer(curr_peer);
				printf(" having no piece data.\n");
				return 0;
			}	
			uint32_t message_index = 0;
			unsigned char *message_index_addr = (unsigned char *)(&message_index);
			*message_index_addr = message[4];
			++message_index_addr;
			*message_index_addr = message[3];
			++message_index_addr;
			*message_index_addr = message[2];
			++message_index_addr;
			*message_index_addr = message[1];
			uint32_t message_begin = 0;
			unsigned char *message_begin_addr = (unsigned char *)(&message_begin);
			*message_begin_addr = message[8];
                        ++message_begin_addr;
                        *message_begin_addr = message[7];
                        ++message_begin_addr;
                        *message_begin_addr = message[6];
                        ++message_begin_addr;
                        *message_begin_addr = message[5];
			if ( (peer_data->cr) == 0 ) {
				printf("No peer reqeust is pending yet we have received some a subpiece from peer ");
				print_peer(curr_peer);
				printf(" .\n");
				return 0;
			}
			else {
				if ( ( ((peer_data->cr)->index) != message_index) ||
				     ( ((peer_data->cr)->begin) != message_begin) ) {
					printf("Client requested subpiece not matching the subpiece returned from peer ");
					print_peer(curr_peer);
					printf(" .\n");
					return 0;
				}
				else {
					if ( (pim->curr_piece_index_downloading) != message_index) {
						if (btree_search(pim->pieces_downloaded, &message_index) == 1) {
							// arrived subpiece is part of piece that the client has already downloaded
							free(peer_data->cr);
							peer_data->cr = 0;
							return 1;
						}
						else {
							printf("Error receiving piece from peer ");
							print_peer(curr_peer);
							printf(" in that the received message index is neither of a previously downloaded piece or of the current piece index.\n");
							free(peer_data->cr);
							peer_data->cr = 0;
							return 0;
						}
					}
					else {
						if ( (message_begin % (int_pow(2,14))) != 0) {
							printf("Error - received subpiece from peer ");
							print_peer(curr_peer);
							printf(" that was not alligned to a 2^14 byte boundary.\n");
							free(peer_data->cr);
							peer_data->cr = 0;
							return 0;
						}
						uint32_t subpiece_index = message_begin/int_pow(2, 14);
						if (btree_search(pim->subpieces_downloaded, &subpiece_index) == 0) {
							// received new subpiece
							if (pim->curr_piece == 0) {
								printf("Error - received subpiece but the current piece memroy has not been allocated to store the piece. Received subpiece from peer ");
								print_peer(curr_peer);
								printf(" .\n");
								return 0;
							}
							else {
								memcpy(&((pim->curr_piece)[message_begin]), &(message[9]), (peer_data->cr)->length);
								btree_insert(pim->subpieces_downloaded, &subpiece_index);
								uint32_t number_of_subpieces_in_piece = ((pim->curr_piece_length) / int_pow(2,14)) + 1;
								if (btree_no_of_nodes(pim->subpieces_downloaded) == number_of_subpieces_in_piece) {
									// we have a complete piece, perform SHA-1 hash on curr_piece to determine if the downloaded piece is valid
									//TODO: implement SHA-1 algorithm	
									unsigned char *sha1_hash = sha_1_hash(pim->curr_piece, pim->curr_piece_length);
									unsigned char valid_piece_hash = 1;
									if (pim->mi == 0) {
										printf("Somehow we have received a piece before receiving the metdata info. \n");
										return 0;
									}
									else {
										for (int i=0; i<20; ++i) {
											if ( ((pim->mi)->pieces)[(pim->curr_piece_index_downloading*20) + i] != sha1_hash[i]) {
												valid_piece_hash = 0;
											}
										}
									}
									if (!valid_piece_hash) {
										free_btree(pim->subpieces_downloaded);
										pim->subpieces_downloaded = init_null_btree(sizeof(uint32_t),
															    compare_int,
															    print_int);
										free(peer_data->cr);
										peer_data->cr = 0;
									}
									else {
										uint32_t successfull_download_piece_index = pim->curr_piece_index_downloading;
										btree_insert(pim->pieces_downloaded, &successfull_download_piece_index);
										insert_piece_into_file(pim->curr_piece, pim->mi, pim->curr_piece_index_downloading);	
									}
								}
								return 1;
							}
						}
						else {
							//subpiece already received
							free(peer_data->cr);
							peer_data->cr = 0;
							return 1;
						}
					}
				}
			}
		}
	}
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
	mi.piece_length = 0;
	printf("Initiating peer_interactions_metadata structure...\n");
	struct peer_interactions_metadata pim;
	pim.mi = 0;
	pim.curr_piece_index_downloading = -1;
	pim.curr_piece = 0;
	pim.curr_piece_length = 0;
	pim.pieces_downloaded = init_null_btree(sizeof(uint32_t),
						 compare_int,
						 print_int);
	pim.subpieces_downloaded = init_null_btree(sizeof(uint32_t),
						  compare_int,
						  print_int);
	pim.piece_length = 0;
	pim.subpiece_length = 0;
	for (int i=0; i<6; ++i) {
		pim.timestamps[i] = 0;
	}
	pim.snubbed_by_peers = init_null_btree(sizeof(unsigned int),
						compare_int,
						print_int);
	for (int i=0; i<5; ++i) {
		(pim.optimistic_unchoked_peers)[i] = -1;
	}
	for (int i=0; i<5; ++i) {
		pim.optimistic_unchoked_timestamps[i] = 0;
	}
	printf("Initiating peer data structures...\n");
	struct peer_metadata peers_data[number_of_peers];
	for (int i=0; i<number_of_peers; ++i) {
		(peers_data[i]).client_socket_fd = -1;
		(peers_data[i]).extension_protocol_supported = 0;
		(peers_data[i]).bittorrent_handshake_performed = 0;
	        (peers_data[i]).local_choke = 0;
		(peers_data[i]).local_interest = 0;
		(peers_data[i]).peer_choke = 0;
		(peers_data[i]).peer_interest = 0;
		(peers_data[i]).pieces_peer_has = init_null_btree(sizeof(uint32_t),
								  compare_int,
								  print_int);
		(peers_data[i]).cr = 0;
		(peers_data[i]).pr = 0;
		for (int j=0; j<5; ++j) {
			((peers_data[i]).subpiece_pipeline)[j] = -1;
		}
		for (int j=0; j<6; ++j) {
			((peers_data[i]).data_downloaded)[j] = 0;
		}	
	}
	int curr_peer_index = 0;
	unsigned char *protocol_string = "BitTorrent protocol";
	// reserved bytes are chosen to support the extension protocol
	unsigned char reserved_bytes[8] = {0x00, 0x00, 0x00, 0x00,
					   0x00, 0x10, 0x00, 0x00};
	
	//TODO: Have seperate thread that sends bittorrent keep alives periodically to 
	// connected peers. have a shared binary tree storing integers that are the peer
	// indexes of connected peers that the thread sends keep alives to and insert
	// indices into the binary tree as client connects with peers
	printf("Starting peer communications...\n");
	while (1) {
		if ((peers_data[curr_peer_index]).bittorrent_handshake_performed == 0) {
			printf("curr_peer_index : %d  ", curr_peer_index);
			// peer not connected over tcp yet
			int clt_socket_fd = socket(AF_INET, SOCK_STREAM, 6);
			if (clt_socket_fd == -1) {
				printf("Error creating socket for client-peer connection. %s.\n",
					strerror(errno));
				++curr_peer_index;
				curr_peer_index = (curr_peer_index % number_of_peers);
				continue;
			}
			struct peer *curr_peer = (struct peer *)vector_read(peers, curr_peer_index);
			struct sockaddr_in peer_address;
			peer_address.sin_family = AF_INET;
			peer_address.sin_port = htons(curr_peer->port);
			uint32_t peer_ipv4_address = 0;
			unsigned char *address_bytes = (unsigned char *)(&peer_ipv4_address);
			memcpy(address_bytes, curr_peer->ip, 4);
			peer_address.sin_addr.s_addr =  peer_ipv4_address;
			socklen_t addrlen = sizeof(struct sockaddr_in);
			if (fcntl(clt_socket_fd, F_SETFL, O_NONBLOCK) == -1) {
				printf("Error setting client socket file descriptor for peer ");
				print_peer(curr_peer);
				printf(" to NONBLOCKing before connect call.\n");
				++curr_peer_index;
				curr_peer_index = curr_peer_index % number_of_peers;
				continue;
			}

			connect(clt_socket_fd,
			        (struct sockaddr *)(&peer_address),
		       		addrlen); 
			struct pollfd fds;
			fds.fd = clt_socket_fd;
			fds.events = POLLOUT;
			int poll_res = poll(&fds, 1, 1000); // poll for 1 second
			if (poll_res == -1) {
				printf("Error whilst polling for connect call to peer ");
				print_peer(curr_peer);
				printf(".\n");
				++curr_peer_index;
				curr_peer_index = curr_peer_index % number_of_peers;
				continue;
			}
			else if (poll_res == 0) {
				printf("Timeout occurred whilst polling after connect call to peer ");
				print_peer(curr_peer);
				printf(".\n");
				++curr_peer_index;
				curr_peer_index = curr_peer_index % number_of_peers;
                                continue;
			}
			else {
				printf("Writing data now available after connect call.\n");
				int socket_error;
				socklen_t len = sizeof(int);

				if ( getsockopt(clt_socket_fd,
					        SOL_SOCKET,
					        SO_ERROR,
					        &socket_error,
					        &len) == -1) {
					printf("Error calling getsockopt to see if connect call to peer ");
					print_peer(curr_peer);
					printf(" was successfull or not.\n");
					++curr_peer_index;
					curr_peer_index = curr_peer_index % number_of_peers;
                                	continue;
				}
				else {
					if (socket_error == 0) {
						//set clt_socket_fd back to blocking
						int socket_flags = fcntl(clt_socket_fd, F_GETFL);
						if (socket_flags == -1) {
							printf("Error whilst setting connected socket back to blocking.%s.\n", strerror(errno));
							++curr_peer_index;
							curr_peer_index = curr_peer_index % number_of_peers;
							continue;
						}
						socket_flags ^= O_NONBLOCK;
						if (fcntl(clt_socket_fd, F_SETFL, socket_flags) == -1) {
							printf("Error whilst setting connected socket back to blocking.%s.\n", strerror(errno));
                                                        ++curr_peer_index;
                                                        curr_peer_index = curr_peer_index % number_of_peers;
                                                        continue;
						}
						/*int keepalive_opt = 1;
						socklen_t len = sizeof(keepalive_opt);
						if ( setsockopt(clt_socket_fd, 
								SOL_SOCKET, 
								SO_KEEPALIVE, 
								&keepalive_opt,
								len) ==-1) {
							printf("Error setting socket keepalive option after connection success for peer ");
							print_peer(curr_peer);
							printf(" .\n");
							++curr_peer_index;
							curr_peer_index = curr_peer_index % number_of_peers;
							continue;
						}*/

						printf("Client connected to peer ");
                                                print_peer(curr_peer);
                                                printf("\n");
                                                (peers_data[curr_peer_index]).client_socket_fd = clt_socket_fd;		
						printf("Attempting to perform bittorrent handshake with peer ");
						print_peer(curr_peer);
						printf(" .\n");
						
						// try to complete bittorrent handshake
						// bittorrent_handshake is 68 bytes
						unsigned char *bittorrent_handshake = get_bt_handshake(protocol_string,
										reserved_bytes,
										ih,
										peer_id);
						if (bittorrent_handshake == 0) {
							++curr_peer_index;
							curr_peer_index = curr_peer_index % number_of_peers;
							continue;
						}					
						printf("Constructed bittorrent handshake and now going to send it to peer ");
						print_peer(curr_peer);
						printf(".\n");
						int total_bytes_sent = 0;
						unsigned char error_sending_handshake = 0;
						while (total_bytes_sent != 68) {
							int bytes_sent = send((peers_data[curr_peer_index]).client_socket_fd,
										&(bittorrent_handshake[total_bytes_sent]),
										68 - total_bytes_sent,
										0);
							if (bytes_sent == -1) {
								printf("Error sending bittorrent handshake to peer ");
								print_peer(curr_peer);
								printf("\n");
								error_sending_handshake = 1;
								break;
							}
							else if (bytes_sent == 0) {
								printf("Error in that only 0 bytes were sent to peer ");
								print_peer(curr_peer);
								printf(" during sending of bt_handshake.\n");
								error_sending_handshake = 1;
								break;
							}
							else {
								total_bytes_sent += bytes_sent;
							}
						}
						if (error_sending_handshake == 1) {
							++curr_peer_index;
							curr_peer_index = (curr_peer_index % number_of_peers);
							continue;
						}
						printf("Client sent Bittorrent Handshake to peer ");
						print_peer(curr_peer);
						printf(". \n");
						free(bittorrent_handshake);
						// receive bittorrent handshake from peer
						unsigned char *bittorrent_handshake_recv = recv_bittorrent_handshake( (peers_data[curr_peer_index]).client_socket_fd,
									curr_peer
					);
						if (bittorrent_handshake_recv == 0) 
						{
							++curr_peer_index;
							curr_peer_index = curr_peer_index % number_of_peers;
							continue;	
						}
						unsigned char valid_handshake = validate_handshake(bittorrent_handshake_recv,
										ih);             
						if (!valid_handshake) {
							printf("Received invalid bittorrent handshake from peer ");
							print_peer(curr_peer);
							printf("\n");
							++curr_peer_index;
							curr_peer_index = (curr_peer_index % number_of_peers);
							free(bittorrent_handshake_recv);
							continue;
						}
						else {	
							printf("Received valid bittorrent handshake from peer ");
							print_peer(curr_peer);
							printf(",\n");
							unsigned char processed_extension = 0;
							if ( (bittorrent_handshake_recv[25]) & 0x10) {
								peers_data[curr_peer_index].extension_protocol_supported = 1;
								free(bittorrent_handshake_recv);
								while (processed_extension == 0) {	
									// recv extension protocol handshake
									uint32_t message_length = get_message_length( (peers_data[curr_peer_index]).client_socket_fd,
															curr_peer);
									if (message_length == 0) {
										++curr_peer_index;
										curr_peer_index = curr_peer_index % number_of_peers;
										continue;
									}
									message_length -= 4;	
									unsigned char *message = get_bt_message((peers_data[curr_peer_index]).client_socket_fd,
													     message_length, curr_peer);

									if (message == 0) {
										++curr_peer_index;
										curr_peer_index = curr_peer_index % number_of_peers;
										continue;
									}
									else {
										process_message((peers_data[curr_peer_index]).client_socket_fd,
												curr_peer,
												message,
												message_length,
												&(peers_data[curr_peer_index]),
												&(pim));
									}	
								}
								
							}
							else {

							}
							++curr_peer_index;
							curr_peer_index = (curr_peer_index % number_of_peers);
							continue;
						}
					}	
					else {

						printf("Client failed to connect to peer ");
						print_peer(curr_peer);
						printf(" in the 1 second given.\n");
						++curr_peer_index;
						curr_peer_index = curr_peer_index % number_of_peers;
                                        	continue;
					}
				}
			}
		}
		else {

		}
	}		
}
