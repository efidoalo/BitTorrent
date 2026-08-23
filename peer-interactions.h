/*========================;
 *
 * File: peer-interactions.h
 * Content: header file for peer
 * interactions struct
 * Date: 18/8/2026
 * Author: Andy Oldham
 *
 *******************************/

#include "binary_tree.h"
#include <stdio.h>
#include <time.h>
#include <stdint.h>

struct file
{
        unsigned long long int length; // total number of bytes of the file
        char *path; // null terminated string of a path where the last entitfy of the path
                    // is the file name and the preceding parts of the path are folders
                    // within the downloads directory.
};

// function that prints a file
void file_print(void *f)
{
	struct file *fs = (struct file *)f;
	printf("file %s of %llu bytes.", fs->path, fs->length);
}

// these compare and print integer functions are used in the binary tree structures
// when storing integers
void *compare_int(void *i1, void *i2)
{
	uint32_t *integer1 = (uint32_t *)i1;
	uint32_t *integer2 = (uint32_t *)i2;
	if ( (*integer1) > (*integer2) ) {
		return i1;
	}
	else if ( (*integer1) < (*integer2) ) {
		return i2;
	}
	else {
		return 0;
	}
}

void print_int(void *i)
{
	uint32_t *integer = (uint32_t *)i;
	printf("%u", *integer); 
}

struct metadata_info
{
	char *name; // name of the file (for single file download) or directory (for download
		    // of multiple files) is null terminated string
	unsigned long long int piece_length; // number of bytes of each piece that constitutes the download
	unsigned char *pieces; // array of a multiple of 20 where each successive 20 bytes
			       // is the SHA1 hash of the corresponding piece in the download
			       //
	// one of length and files shall be 0, but not both
	unsigned long long int length; // number of total bytes of the download
	struct vector *files; // vector of files structures representing the download
			      // files in index order appear in that order in the download	
};

struct request
{
	uint32_t index;
	uint32_t begin;
	uint32_t length;
};

struct peer_interactions_metadata
{

	struct metadata_info *mi; // null if metadata info has not been found yet
				  // a poniter to the initialized struct if 
				  // it has been found and downloaded from peer
	long long int curr_piece_index_downloading; // 0 starting index of the current piece
					  // that the client is downloading.
					  // -1 if no piece has been selected yet for download
	
	struct binary_tree *pieces_downloaded; // binary tree storing the zero
					       // starting indeices of the peices
					       // that have been downloaded and
					       // SHA-1 hash verified
	unsigned char *curr_piece; // an (m)allocated array containing the current piece of the download
				   // having index curr_piece_index_downloading.
				   // When we receive data from peers we place data into this
				   // buffer and then when it's full verify it with the
				   // SHA-1 hash. NULL if no current piece has been selected yet
	unsigned long long int curr_piece_length; // gives the length in bytes
						  // of the current piece,
						  // which is the piece_lenfth
						  // in metadata_info or
						  // a smaller value
						  // for the last piece in the
						  // download
	struct binary_tree *subpieces_downloaded; // binary tree containing the 0 starting
						  // subpiece indexes of the current piece
						  // that we are downloading.
						  //
	unsigned long long int piece_length; // number of bytes each pieice is. set to 0 if unknown
	unsigned long long int subpiece_length; // number of bytes each subpiece of a piece is.
				      // set to zero whilst unknown
	time_t *timestamps[6]; // timestamps[0] gives the address of the oldest timestamp
			      // timestamp[6] gives the address of the most recent timestamp
			      // the idea is to have timestamps marking each 10 second
			      //  interval to base data download rates and unchoking as well
			      //  as anti snubbing off. timestamp[i] is null if there is no time
			      //  stamp yet having index i
	
	struct binary_tree *snubbed_by_peers; // binary tree storing the index of each peer
					      // that has snubbed the client and for whom
					      // the client will only upload to as a result
					      // of an optomistic unchoke
	long long int optimistic_unchoked_peers[5]; // array of peer incdices that are being currently
					   // optimisticlly unchoked. integer value of -1
					   // indicates no peer being optimistically unchoked. 
	time_t* optimistic_unchoked_timestamps[5]; //array of corresponding timestamps indicting
						   //the timestamps when the peers began to be
						   //optimistically unchoked. These are either
						   //null pointers if no correspdning peer
						   //is being optimistically unchoked or the 
						   //timestamp pointer will be one from the array
						   //tiemstamps.
};

struct peer_metadata
{
	int client_socket_fd; // tcp connected socket file descriptor to peer,
			      // -1 if no socket_fd has been created yet
				      // through client_socket_fd
	unsigned char extension_protocol_supported; // 0 if extnesion protocl
						    // isn't supported. 1 if it
						    // is supported by the peer
	unsigned char bittorrent_handshake_performed; // 0 if no initial handshake has been performed
						      // 1 if a handshake has been performed 
						      // so that the peer is ready for interactions
	unsigned char local_choke; // 0 if local client is choking peer
				   // 1 if local client is unchoking peer
	unsigned char local_interest; // 0 if local clinet is not interested in peer dat
				      // 1 if local client is interested in peer data
	unsigned char peer_choke; // 0 if peer is choking local client
				  // 1 if peer is unchoking local client
	unsigned char peer_interest; // 0 if peer is not interested in local client data
				     // 1 if peer is interested in local client data
	struct binary_tree *pieces_peer_has; // binary tree containing the 0 starting
					     // indexes of the full pieces this peer has
	struct request *cr; // null pointer 9 if no client requests are pending
			    // otherwise points to the client request that is
			    // pending
	struct request *pr; // null pointer 0 if no peer request is pending.
				 // otherwise set to the pointer to the peer request that the peer has requested and is pending
	long long int subpiece_pipeline[5]; // int value of array is -1 to indicate an abscent subpiece 
				  // index request, otherwise is the 0 starting index of the
				  // subpiece to request next. lowest indices of the array
				  // are checked for valid subpiece indices first.
	long long int data_downloaded[6]; // data_downloaded[0] is the amount of data download
			        // ed between timestamp[0] and timestamp[1]
				// data_downloaded[1] is the amount of data downloaded
				// between timestamp[1] and timestamp[2]
				// .. 
				// data_downloaded[4] is the amount of data downloaded
				// between timestamp[4] and tiemstamp[5]
				// and data_downloaded[5] is the amount of data 
				// downloaded since timestamp[5],
				// data_downloaded[i] = -1 if the the client hasn't been running
				// that long to obtain a downloaded data in the corresponding
				// timestamp interval
	
};
