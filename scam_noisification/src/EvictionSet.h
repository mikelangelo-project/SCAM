
#ifndef EVICTION_SET_H
#define EVICTION_SET_H

#include "CacheDefs.h"
#include <stdint.h>

#define BITS_IN_CHAR 8

typedef struct Cache_Line_Node {
	struct Cache_Line_Node* pNext;
	Cache_Line* pLine;
}Cache_Line_Node;

extern int NUMBER_OF_KEYS_TO_SAMPLE;
extern int exitflag;

/**
 * This function gets a buffer of type LinesBuffer, and uses its lines to
 * map a cache eviction set.
 * I.E. A set of memory lines, sorted into sets, which will allow to rewrite the
 * entire cache.
 *
 * The found cache sets will be saved to the given Cache_Mapping pointer.
 *
 * Returns the number of cache sets which were found.
 */
int CreateEvictionSets(LinesBuffer* pBuffer, Cache_Mapping* pEvictionSets,int hitThreshold, int missThreshold);

/**
 * run in loop until user exit and read the first byte in the first line in each one of the suspected sets.
 */
//void noiseSuspectedSets(Cache_Mapping* pEvictionSets, int* suspectsSets , int suspectsNum);
/**
 * This function gets a setnum and a array of Suspects and return 1 if the set is in the list, else 0.
 */

/**
 * To make sure the pre-fetcher can't learn our cache usage, we randomize the order in which
 * we access lines in a set.
 */
void RandomizeRawSet(Raw_Cache_Set* pSet);

/**
 * Same thing but for a smaller set.
 *
 * TODO: merge these 2 functions, no real reason to have double of these.
 */
void RandomizeSet(Cache_Set* pSet);

/**
 * Gets a list of lines, and a line that conflicts with them.
 *
 * Goes over the list and searches for the exact subset that causes the conflict.
 * Then returns it in the pSmallSet pointer.
 * Returns the number of lines in said set.
 */
int FindSet(Raw_Cache_Set* pBigSet, Cache_Set* pSmallSet, char* pConflictingLine,int hitThreshold,int missThreshold);
/**
 * Returns the number of set a certain address is associated with
 */
int AdressToSet(char* Adress);
#endif
