
#ifndef CACHE_DEFS_H
#define CACHE_DEFS_H

#define LLC

#define NUMBER_OF_DIFFERENT_KEYS 1
#define KEYS_LEN_IN_BITS 4096
//#define KEYS_LEN_IN_BITS 2048

#ifdef LLC
#define BYTES_IN_LINE 64
#define LINES_IN_SET 12
#define SETS_IN_CACHE 8192
#define LINES_IN_CACHE (SETS_IN_CACHE * LINES_IN_SET)
#define LINES_FOR_EVICTION_ALGORITHEM  (LINES_IN_CACHE * 5)
#define SLICE_NUM 4
#define SETS_IN_SLICE (SETS_IN_CACHE / SLICE_NUM)
#define RAW_SETS (SETS_IN_CACHE / SLICE_NUM)
#define RAW_LINES_IN_SET (LINES_IN_SET * SLICE_NUM)

#define CACHE_POLL_MAX_HIT_THRESHOLD_IN_NANO 70
#define CACHE_POLL_MIN_MISS_THRESHOLD_IN_NANO 125
#define CACHE_POLL_CONTEXT_SWITCH_IN_NANO 1500000

#define CACHE_POLL_MAX_HIT_THRESHOLD_IN_TICKS 100
#define CACHE_POLL_MIN_MISS_THRESHOLD_IN_TICKS 175
#define CACHE_POLL_CONTEXT_SWITCH_IN_TICKS 1000

#define CACHE_POLL_TIME_SAMPLE_DELAY 26
#define CACHE_POLL_TIME_PER_SET     3662110 //***7324219
#define CACHE_POLL_MIN_SET_MISS_THRESHOLD_IN_TICKS 175

#if KEYS_LEN_IN_BITS == 4096
#define CACHE_POLL_TIMEOUT_IN_NANO 1800
#define MIN_SCORE_TO_MONITOR_SET 100
#else
#define CACHE_POLL_TIMEOUT_IN_NANO 575
//#define CACHE_POLL_TIMEOUT_IN_NANO 750
#define MIN_SCORE_TO_MONITOR_SET 70
#endif

#define CACHE_POLL_SAMPLES_PER_SET (CACHE_POLL_TIME_PER_SET / CACHE_POLL_TIMEOUT_IN_NANO)
#define SEC_TO_TEST_SETS 30
#define NUMBER_OF_STATS 3
#define SET_STAT_THR_MIN 0
#define SET_STAT_THR_MAX 2034
#define ZERO_ONE_STATISTICS_DIFF_THR 400
#define ONE_STAT_THR_MIN 1000

#else

#define BYTES_IN_LINE 64
#define LINES_IN_SET 8
#define SETS_IN_CACHE 512
#define LINES_IN_CACHE (SETS_IN_CACHE * LINES_IN_SET)
#define LINES_FOR_EVICTION_ALGORITHEM  (LINES_IN_CACHE * 4)

#define CACHE_POLL_MAX_HIT_THRESHOLD_IN_NANO 60
#define CACHE_POLL_MIN_MISS_THRESHOLD_IN_NANO 65
#define CACHE_POLL_CONTEXT_SWITCH_IN_NANO 10000

#define CACHE_POLL_MAX_HIT_THRESHOLD_IN_TICKS 39
#define CACHE_POLL_MIN_MISS_THRESHOLD_IN_TICKS 50
#define CACHE_POLL_CONTEXT_SWITCH_IN_TICKS 1000

#endif

typedef unsigned long long int qword;

typedef struct Cache_Line {
	char Bytes[BYTES_IN_LINE];
}Cache_Line;

typedef struct Cache_Set {
	char* Lines[LINES_IN_SET];
	int ValidLines;
}Cache_Set;

typedef struct Raw_Cache_Set {
	char* Lines[RAW_LINES_IN_SET];
	int ValidLines;
}Raw_Cache_Set;

typedef struct LinesBuffer {
	Cache_Line Lines[LINES_FOR_EVICTION_ALGORITHEM];
}LinesBuffer;

typedef struct Raw_Cache_Mapping {
	Raw_Cache_Set Sets[RAW_SETS];
}Raw_Cache_Mapping;

typedef struct Cache_Mapping {
	Cache_Set Sets[SETS_IN_CACHE];
	int ValidSets;
}Cache_Mapping;

typedef struct Set_Statistics {
	qword num;
	float mean;
	float variance;
	long int offset;
}Set_Statistics;

typedef struct Cache_Statistics {
	Set_Statistics SetStats[SETS_IN_CACHE];
}Cache_Statistics;

/*
typedef struct config_struct {
	int attach; // True if the monitor has to attach to a given PID
	int run; // True if the monitor has to start the command given as argument
	char cmd[MAX_CMD_SIZE]; // The command to be run if run is true
	int pid;
	int numEvents;
	char events[MAX_EVENTS][MAX_STR_LEN];
	int interval;
	int iterations;
	FILE *file;
} config;
*/

#endif
