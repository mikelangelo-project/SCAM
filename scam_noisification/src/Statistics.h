
#ifndef STATISTICS_H
#define STATISTICS_H

#include "CacheDefs.h"


/**
 * Helper function for when we want to save our statistics to a file for later review.
 */
//void WriteStatisticsToFile(Cache_Statistics* pCacheStats, char* pFileName, int Sorted);

/**
 * Sorts a set of statistics in a Cache_Statistics struct.
 *
 * Returns the sorted list in the StatisticsToSort pointer.
 */
void SortStatistics(Cache_Statistics* BaseStatistics, Cache_Statistics* StatisticsToSort);
/**
 * this function try to find the attacked sets,
 * Gets mapped cache, a pointer to array of the suspected sets number.
 * return number of suspected sets
 */
//int findSuspects(Cache_Mapping* pEvictionSets, int* suspectsSets, int SetsFound);
/**
 * run in loop until user exit and read the first byte in the first line in each one of the suspected sets.
 */
//void noiseSuspectedSets(Cache_Mapping* pEvictionSets, int* suspectsSets , int suspectsNum);
/**
 * This function gets a setnum and a array of Suspects and return 1 if the set is in the list, else 0.
 */
//int IsSuspectSet(int setnum,int * suspectsSets);
/*
* This function gets a list of sets and a cache mapping, and fills the found statistics for the given list into the give Cache_Statistics
* pointer.
*/
void GetMemoryStatistics(Cache_Mapping* pEvictionSets, Cache_Statistics* pCacheStats);

/**
 * Compare between two Cache_Statistics in order to find suspicious sets
 */
int compareStats(Cache_Mapping* pEvictionSets, int* suspectsSets, int SetsFound,Cache_Statistics* withoutServer, Cache_Statistics* withServer);
#endif
