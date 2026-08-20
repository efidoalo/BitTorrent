/*==============================;
 *
 * File: tracker-interactions.h
 * Content: Header file declaring
 * functions and data structures that
 * are used to interact with trackers.
 * Announces are done over http
 * Date: 3/6/2026
 * Author: Andy Oldham
 *
 **************************************/

#ifndef __TRACKER_INTERACTIONS_H__
#define __TRACKER_INTERACTIONS_H__
#include "vector.h"

struct peer
{
        unsigned char ip[4]; // internet protocol address
        unsigned short port; // peer port number
};

void print_peer(void *p);

// returns vector of peers, where each peer contains an ip adress, id and port
// number. Returns NULL on error or exits program. trackers is a vector
// of tracker urls obtained from the magnet link (btih formt).
struct vector *get_peer_list_from_trackers(struct vector *trackers,
					   unsigned char *info_hash,
					   unsigned char *peer_id);


#endif
