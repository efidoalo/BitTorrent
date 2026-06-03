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

char *ml_get_info_hash_percent_encoded(char *ml, unsigned char format);
#endif

