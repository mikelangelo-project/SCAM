
#include "getThresholds.h"

int getThresholds(int * missMinThreshold, int * hitMaxThreshold,int try){
	int i,j,result,maxLine,maxVal,minLine,minVal,time,sum,missesInHitRange;
	int missMin, hitMax;
	int hitSamples[TIME_SAMPLE_MAXIMUM] = {0};
	int missSamples[TIME_SAMPLE_MAXIMUM] = {0};
	int error;
	double samplesMean[RAW_LINES_IN_SET+1] = {0};
	Raw_Cache_Set RawSet;
	Raw_Cache_Set RawSet_tmp;
	Raw_Cache_Set RawSet_Hit;
	Raw_Cache_Set RawSet_Miss;
	FILE *fd;
	char* memPointer = MemoryPool;
	if (AdressToSet(memPointer) + try < SETS_IN_CACHE){
		memPointer += try<<6;
	}
	printf("Set: %d\n",AdressToSet(memPointer));
	char* candidate, *candidateMiss, *candidateHit, *tmp, *originalCandidate;

	memset(&MemoryPool, 0, sizeof(char) * MEMORY_POOL_SIZE);

	memset(&RawSet,0,sizeof(Raw_Cache_Set));
	for(i=0; i <= MEMORY_POOL_SIZE; i += BYTES_IN_LINE*RAW_SETS){
		if (RawSet.ValidLines >= RAW_LINES_IN_SET){
			candidate = memPointer + i;
			break;
		}
		RawSet.Lines[RawSet.ValidLines] = memPointer + i;
		RawSet.ValidLines++;
	}

	originalCandidate = candidate; // the 49 line
	for (i = 0; i<NUM_OF_SAMPLE_PER_LINE; i++){
		for (j=0; j < RawSet.ValidLines+1; j++){
			//swap
			memcpy(&RawSet_tmp,&RawSet,sizeof(Raw_Cache_Set));
			candidate = originalCandidate;
			if (j!=RawSet.ValidLines){ //last run will be with the original set and original candidate
				tmp = RawSet_tmp.Lines[j];
				RawSet_tmp.Lines[j] = candidate;
				candidate = tmp;
			}
			RandomizeRawSet(&RawSet_tmp);
			result = ProbeAsm(RawSet_tmp.Lines[0], RawSet_tmp.ValidLines, candidate,0,0);
			samplesMean[j] = (samplesMean[j]*i+result)/(i+1);
		}
	}

	minLine = -1;
	maxLine = -1;
	for (i=0; i<RAW_LINES_IN_SET+1; i++){
		if(minLine == -1 || minVal > samplesMean[i]){
			minLine = i;
			minVal = samplesMean[i];
		}
		if(maxLine == -1 || maxVal < samplesMean[i]){
			maxLine = i;
			maxVal = samplesMean[i];
		}
		//printf("%d) %d",i,(int)samplesMean[i]);
		//printf("\n");
	}
	//printf("Minimum: {Line: %d, Val: %d}\nMaximum: {Line: %d, Val: %d}\n",minLine,minVal,maxLine,maxVal);

	memcpy(&RawSet_Hit,&RawSet,sizeof(Raw_Cache_Set));
	memcpy(&RawSet_Miss,&RawSet,sizeof(Raw_Cache_Set));

	candidateHit = originalCandidate;
	if (minLine != RAW_LINES_IN_SET){
		tmp = RawSet_Hit.Lines[minLine];
		RawSet_Hit.Lines[minLine] = candidateHit;
		candidateHit = tmp;
	}

	candidateMiss = originalCandidate;
	if (maxLine != RAW_LINES_IN_SET){
		tmp = RawSet_Miss.Lines[maxLine];
		RawSet_Miss.Lines[maxLine] = candidateMiss;
		candidateMiss = tmp;
	}

	for (i=0; i<NUM_OF_SAMPLES_TO_DECIDE; i++){
		RandomizeRawSet(&RawSet_Miss);
		time = ProbeAsm(RawSet_Miss.Lines[0], RawSet_Miss.ValidLines, candidateMiss,0,0);
		if (time < TIME_SAMPLE_MAXIMUM)
			missSamples[time]++;
		if (time < 80){
			char * pCurrLine = RawSet_Miss.Lines[0];
			for(j=0;j<RawSet_Miss.ValidLines;j++){
				pCurrLine = *(char**)pCurrLine;
			}
		}
		RandomizeRawSet(&RawSet_Hit);
		time = ProbeAsm(RawSet_Hit.Lines[0], RawSet_Hit.ValidLines, candidateHit,0,0);
		if (time < TIME_SAMPLE_MAXIMUM)
			hitSamples[time]++;
	}

	fd = fopen("log_hit_results.csv", "w+");
	for (i=0;i<TIME_SAMPLE_MAXIMUM;i++){
		if (hitSamples[i])
			fprintf(fd,"%d, %d\n", i,hitSamples[i] );
	}

	fclose(fd);
	fd = fopen("log_miss_results.csv", "w+");
	for (i=0;i<TIME_SAMPLE_MAXIMUM;i++){
		if (missSamples[i])
			fprintf(fd,"%d, %d\n", i, missSamples[i] );
	}
	fclose(fd);

	sum = 0;
	for (i=0;i<TIME_SAMPLE_MAXIMUM;i++){
		sum+=hitSamples[i];
		if(sum>=NUM_OF_SAMPLES_TO_DECIDE*(1 - PRERROR_HIT)){
			hitMax=i;
			break;
		}
	}
	sum = 0;
	missesInHitRange = 0;
	for (i=0;i<TIME_SAMPLE_MAXIMUM;i++){
		if (i<hitMax){
			missesInHitRange+=missSamples[i];;
		} else {
			sum+=missSamples[i];
			if(sum>=NUM_OF_SAMPLES_TO_DECIDE*PRERROR_MISS){
				missMin=i;
				break;
			}
		}
	}
	float missInHitRangeRatio = (double)missesInHitRange/NUM_OF_SAMPLES_TO_DECIDE;
	printf("Hit Threshold: %d, Miss Threshold: %d\n",hitMax,missMin);
	printf("Misses in hit range (Ratio):%.2f [%d / %d]\n",missInHitRangeRatio,missesInHitRange,NUM_OF_SAMPLES_TO_DECIDE);

	*missMinThreshold = missMin;
	*hitMaxThreshold = hitMax;

	error = missInHitRangeRatio > 0.1 || (missMin - hitMax < 100);
	return error;
}
