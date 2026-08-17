/*==================================;
 *
 * File: ml.h
 * Content: Magnet link processing 
 * header file.
 * Date: 2/6/2026
 * Author: Andy Oldham'
 *
 ***********************************/

#ifndef __ML_H__
#define __ML_H__

#include "vector.h"

// returns 1 if btih format present
// returns 2 if btmh format present
// otherwsie returns 0 if unrecognised format
unsigned char ml_version(char *ml);

struct vector *ml_get_trackers(char *ml, unsigned char format);

// Return the 20 byte raw sha1 info hash from the manget linl
unsigned char *ml_get_raw_info_hash(char *ml, unsigned char format);
#endif

