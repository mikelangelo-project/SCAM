#define _GNU_SOURCE

#include <sys/time.h>

#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "sched.h"
#include <stdio.h>
#include <netinet/in.h>
#include <signal.h>
#include <pthread.h>
#include <stdbool.h>

#include "CacheDefs.h"
#include "getThresholds.h"
#include "EvictionSet.h"
#include "Statistics.h"

//#include "comm_funcs.h"
//#include "config.h"
//#include "quickhpc.h"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define MONITOR_ITERATIONS -1
#define MONITOR_INTERVAL  100
#define MONITOR_EVENTSFILE "events.conf"

#define ALL_CORE 				0
#define NOISE_CORE 				1
#define WAIT_FOR_SERVER_SECONDS 5
#define INPUTSIZE 				20
#define OUTPUTSIZE 				20


//Mapping
LinesBuffer Buffer;

Cache_Mapping EvictionSets;

int SuspectedSets[SETS_IN_CACHE];
int numOfSuspects;
sig_atomic_t doNoise = 0;

void* rumble(void* args) {
	while (doNoise) {
		int i, j;
		for (i = 0; i < numOfSuspects; i++) {
			if (1 == (&EvictionSets)->Sets[SuspectedSets[i]].Lines[0][0])
				j++;
		}
	}
	return EXIT_SUCCESS;
}

int main(void) {
	int missThreshold = CACHE_POLL_MIN_MISS_THRESHOLD_IN_TICKS; // Default Value
	int hitThreshold = CACHE_POLL_MAX_HIT_THRESHOLD_IN_TICKS; 	// Default Value
	int rs = -1;
	int SetsFound;
	int try = 0;
	char input[INPUTSIZE];
	int option;
	pthread_t rumble_tid;
	Cache_Statistics withServer, withoutServer;


	while (true) {
		if (fgets(input, INPUTSIZE, stdin) != NULL)
			option = atoi(input);
		else
			perror("input");
		switch (option) {
		case 1:
			puts("Getting Thresholds"); fflush(stdout);
			try++;
			rs = getThresholds(&missThreshold, &hitThreshold,try);
			if (rs){
				puts("Getting Thresholds Failed!");  fflush(stdout);
				fputs("0\n",stderr); fflush(stderr);
			} else {
				puts("Getting Thresholds Succeeded!"); fflush(stdout);
				fputs("1\n",stderr); fflush(stderr);
			}
			break;
		case 2:
			puts("Mapping"); fflush(stdout);
			SetsFound = CreateEvictionSets(&Buffer, &EvictionSets, hitThreshold, missThreshold);
			fprintf(stdout,"\nTotal Sets found: %d\r\n", SetsFound); fflush(stdout);
			fputs("1\n",stderr); fflush(stderr);
			puts("Mapping Done"); fflush(stdout);
			break;
		case 3:
			puts("Ranking I, Please Wait"); fflush(stdout);
			memset(&withoutServer, 0, sizeof(Cache_Statistics));
			GetMemoryStatistics(&EvictionSets, &withoutServer);
			fputs("1\n",stderr); fflush(stderr);
			puts("Ranking I Done"); fflush(stdout);
			break;
		case 4:
			puts("Ranking II, Please Wait"); fflush(stdout);
			memset(&withServer, 0, sizeof(Cache_Statistics));
			GetMemoryStatistics(&EvictionSets, &withServer);
			puts("Ranking II Done"); fflush(stdout);
			puts("Comparing Ranks I & II"); fflush(stdout);
			numOfSuspects = compareStats(&EvictionSets, SuspectedSets, SetsFound, &withoutServer, &withServer);
			fprintf(stdout,"Number of high activity sets: %d\n", numOfSuspects); fflush(stdout);
			fputs("1\n",stderr); fflush(stderr);
			break;
		case 5:
			puts("Turning on Rumble"); fflush(stdout);
			doNoise = true;
			if(pthread_create(&rumble_tid, NULL, &rumble, NULL)){
				fputs("0\n",stderr); fflush(stderr);
				fputs("pthread_create failed\n",stdout); fflush(stdout);
				break;
			}
			puts("Rumble is on"); fflush(stdout);
			fputs("1\n",stderr); fflush(stderr);
			break;
		case 6:
			puts("Turning off Rumble"); fflush(stdout);
			doNoise = false;
			if(pthread_join(rumble_tid, NULL)){
				fputs("0\n",stderr); fflush(stderr);
				fputs("pthread_join failed\n",stdout); fflush(stdout);
				break;
			}
			puts("Rumble is off"); fflush(stdout);
			fputs("1\n",stderr); fflush(stderr);
			break;
		default:
			fputs("0\n",stderr); fflush(stderr);
			puts ("invalid input"); fflush(stdout);
			break;
		}
	}
	return EXIT_SUCCESS;
}
