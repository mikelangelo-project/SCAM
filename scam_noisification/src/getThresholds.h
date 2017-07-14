
#ifndef GET_THRES_H
#define GET_THRES_H

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "CacheDefs.h"
#include "EvictionSet.h"
#include "CacheFuncs.h"

#define NUM_OF_SAMPLES_TO_DECIDE 	100000
#define PRERROR_MISS 				0.01
#define PRERROR_HIT 				0.01
#define TIME_SAMPLE_MAXIMUM			1000
#define NUM_OF_SAMPLE_PER_LINE		1000
#define BYTES_IN_CACHE 				(LINES_IN_CACHE * BYTES_IN_LINE)
#define MEMORY_POOL_SIZE 			(LINES_IN_CACHE/SLICE_NUM * RAW_LINES_IN_SET * BYTES_IN_LINE)


char MemoryPool[MEMORY_POOL_SIZE];

/* sanity check + determine thresholds
 * returns 0 in case of success, else non zero
 */
int getThresholds(int * missMinThreshold, int * hitMaxThreshold, int try);

#endif
