/*=================================;
 *
 * File: ml.c
 * Content: Magnet link processing 
 * function definitions
 * Date: 2/6/2026
 * Author: Andy Oldham
 *
 *********************************/

#include "ml.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

unsigned char ml_version(char *ml)
{
	char *ml_prefix = "magnet:?xt=urn:";
	char *btih_indicator = "btih";
	char *btmh_indicator = "btmh";
	size_t type_len = strlen(btih_indicator);
	size_t indicator_index = strlen(ml_prefix);

	if (strncmp(&(ml[indicator_index]), btih_indicator, type_len)==0) {
		return 1;
	}
	else if (strncmp(&(ml[indicator_index]), 
			 btmh_indicator, 
			 type_len) == 0) {
		return 2;
	}
	else {
		// unrecognised magnet link format
		return 0;
	}
}	

void print_tracker(void *tr)
{
	char **ptracker_address = (char **)tr;
	char *tracker_address = *ptracker_address;
	printf("%s\n",tracker_address);
}

// takes the magnet link with format fmt (fmt 1 for btih, 2 for btmh)
// and returns a vector of tracker urls. the null pointer is returned on error
// or an unsuported magnet link format
struct vector *ml_get_trackers(char *ml, unsigned char fmt)
{
	if (fmt == 2) {
		return 0;
	}
	else if (fmt == 1) {
		struct vector *trackers = vector_null_init(sizeof(char *),
						           print_tracker);
		vector_null_init(sizeof(char *), print_tracker);
		unsigned int ml_len = strlen(ml);
		char *tracker_prefix = "&tr=";
		unsigned int tracker_prefix_len = strlen(tracker_prefix);
		for (int i=0; i<(ml_len-3); ++i) {
			if (strncmp(&(ml[i]), tracker_prefix, tracker_prefix_len)==0) {
				int tracker_index = i+4;
				while ( (tracker_index < ml_len) && 
					(ml[tracker_index]!='&')) {
					++tracker_index;		
				}
				if (tracker_index >= ml_len) {
					--tracker_index;
				}
				if (ml[tracker_index] == '&') {
					--tracker_index;
				}
				size_t tracker_url_len = tracker_index - (i+3);
				char *tracker_url = (char *)malloc(tracker_url_len+1);
				memset(tracker_url, 0, tracker_url_len+1);
				if (tracker_url == NULL) {
					printf("Error allocating memory for tracker url using malloc. %s.\n", strerror(errno));
					exit(EXIT_FAILURE);
				}
				int index = i+4;
				char *colon_prefix = "%3A";
				char *forward_slash_prefix = "%2F";
				int j = 0;
				while (1) {
					if (ml[index] == '%') {
						if (strncmp(&(ml[index]), colon_prefix, 3) == 0) {
							tracker_url[j] = ':';
						}
						else if (strncmp(&(ml[index]),
								 forward_slash_prefix, 3) ==0) {
							tracker_url[j] = '/';
						}
						++j;
						index += 3;
					}
					else {
						tracker_url[j] = ml[index];
						++j;
						++index;
					}
					if (index > tracker_index) {
						break;
					}
				}
				tracker_url[j] = 0;
				++j;
				char *truncated_tracker = (char *)malloc(j);
				if (truncated_tracker == NULL) {
					printf("Error allocating memory for truncated tracker via malloc. %s.\n", strerror(errno));
					exit(EXIT_FAILURE);
				}
				memcpy(truncated_tracker, tracker_url, j);
				free(tracker_url);
				tracker_url = truncated_tracker;
				vector_push_back(trackers, &tracker_url);
			}	
		}
		return trackers;
	}
}

unsigned char *ml_get_raw_info_hash(char *ml, unsigned char format)
{
	if (format != 1) {
		printf("Error. Unexpected magnet l8nk format when extracting info hash.\n");
		exit(EXIT_FAILURE);
	}
	else {
		unsigned char *info_hash = (unsigned char *)malloc(20);
		if (info_hash == NULL) {
			printf("Error allocating memory for info hash. %s.\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		char *ml_prefix = "magnet:?xt=urn:btih:";
		size_t ml_prefix_len = strlen(ml_prefix);
		for (int i=0; i<20; ++i) {
			unsigned char curr_byte = 0;
			if (ml[ml_prefix_len+i+0] <= 57) {
				curr_byte += ((ml[ml_prefix_len+i+0]) - 48)*16;
			}
			else if (ml[ml_prefix_len+i+0] <= 90) {
				curr_byte += ((ml[ml_prefix_len+i+0]) - 55)*16;
			}
			else if (ml[ml_prefix_len + i + 0] <= 122) {
				curr_byte += ((ml[ml_prefix_len+i+0]) - 87)*16;
			}
			if (ml[ml_prefix_len+i+1] <= 57) {
                                curr_byte += ((ml[ml_prefix_len+i+1]) - 48);
                        }
                        else if (ml[ml_prefix_len+i+1] <= 90) {
                                curr_byte += ((ml[ml_prefix_len+i+1]) - 55);
                        }
                        else if (ml[ml_prefix_len + i + 1] <= 122) {
                                curr_byte += ((ml[ml_prefix_len+i+1]) - 87);
                        }
			info_hash[i] = curr_byte;
		}
		return info_hash;
	}
}
