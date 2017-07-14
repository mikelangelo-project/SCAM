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

//#define PRINTF printf

LinesBuffer x; //TODO: check why without this line, it doesn't works

int CreateEvictionSets(LinesBuffer* pBuffer, Cache_Mapping* pCacheSets,int hitThreshold, int missThreshold)
{
	//printf("starts with: %x\n",pBuffer); fflush(stdout);
	char* Memory = pBuffer->Lines->Bytes;
	uint32_t Size = sizeof(LinesBuffer);
	int i, j;
	Raw_Cache_Mapping RawMapping;
	int FilledSetIndex = 0;
	int SetsInCurrModulu;
	int CurrSetInSlice;
	int FindSetAttempts;
	int FindAllSetsInRawAttempts= 0;
	//printf("Starts with set: %d, address: %d\n",AdressToSet(Memory),Memory);
	memset(pCacheSets, 0, sizeof(Cache_Mapping));
	memset(&RawMapping, 0, sizeof(RawMapping));
	// Go over all raw sets and split them to real sets.
	for (j = 0; j < RAW_SETS; j++)
	{
		int oldFilledSetIndex = FilledSetIndex;
		int k,Set;
		//printf("\nj=%d, set Found=%d",j,FilledSetIndex);
		for (i = j * BYTES_IN_LINE, SetsInCurrModulu = 0; i < Size && SetsInCurrModulu < SLICE_NUM; i += BYTES_IN_LINE * RAW_SETS)
		{
			// Get bit 1 of the slice.
			char* CurrAdress = Memory + i;
			Set = AdressToSet(CurrAdress);
			//printf("\nraw set = %d\n", Set);
			int Miss = 0;
			// First of all, determine if this line belongs to a set we already found!
			for (CurrSetInSlice = FilledSetIndex - SetsInCurrModulu; CurrSetInSlice < FilledSetIndex; CurrSetInSlice++)
			{

				if(pCacheSets->Sets[CurrSetInSlice].ValidLines)
				Miss = ProbeManyTimes(pCacheSets->Sets[CurrSetInSlice].Lines[0], pCacheSets->Sets[CurrSetInSlice].ValidLines, CurrAdress, hitThreshold, missThreshold);
				// If the line conflicts with this set, give it up, we don't care about it no more
				if (Miss) break;
			}

			if (Miss) continue;

			// Now go over all the relevant sets and check
			RandomizeRawSet(&RawMapping.Sets[Set]);
			Miss = ProbeManyTimes(RawMapping.Sets[Set].Lines[0], RawMapping.Sets[Set].ValidLines, CurrAdress, hitThreshold, missThreshold)&&ProbeManyTimes(RawMapping.Sets[Set].Lines[0], RawMapping.Sets[Set].ValidLines, CurrAdress, hitThreshold, missThreshold);
			if(Miss && RawMapping.Sets[Set].ValidLines<12){

				//printf("error\n");
				//exit(1);
			}
			// If its not a miss, there is no conflict, insert line into raw set.
			if (!Miss) {
				if (RawMapping.Sets[Set].ValidLines >= RAW_LINES_IN_SET)
				{
					//rollback
					break;
				}

				RawMapping.Sets[Set].Lines[RawMapping.Sets[Set].ValidLines] = CurrAdress;
				RawMapping.Sets[Set].ValidLines++;
			}
			// A miss means we conflict with a set. find the conflicting set!
			else
			{
				// We try and find the set a few times... mostly because at some point in the past
				// The function wasn't stable enough.
				for (FindSetAttempts = 0; FindSetAttempts < 10; FindSetAttempts++)
				{

					int FoundNewSet = FindSet(&RawMapping.Sets[Set], &pCacheSets->Sets[FilledSetIndex], CurrAdress, hitThreshold, missThreshold);
					SetsInCurrModulu += FoundNewSet;
					FilledSetIndex += FoundNewSet;

					if (FoundNewSet)
					{
						//if(!FindSetAttempts)
						//	printf("\n%d Attermpts, need to be in range %d-%d\n",FindSetAttempts,j*4,j*4+3);
						printf("Found set %d\n\033[1A", FilledSetIndex);
						//printf("Found set %d, RawSet=%d ,j=%d\n", FilledSetIndex, Set,j);
						RandomizeSet(&pCacheSets->Sets[FilledSetIndex]);
						break;
					}else{
						break;
					}
				}
			}
		}

		if(FilledSetIndex - oldFilledSetIndex<4 && FindAllSetsInRawAttempts < 100){
			FindAllSetsInRawAttempts++;
			//printf("\nerror in set %d mod %d\n",Set,SetsInCurrModulu);

			FilledSetIndex=oldFilledSetIndex;

			memset(&RawMapping.Sets[Set],0,sizeof(Raw_Cache_Set));

			for(k=0;k<4;k++){
				memset(&pCacheSets->Sets[k+oldFilledSetIndex],0,sizeof(Cache_Set));
			}
			j--;

		} else {
			FindAllSetsInRawAttempts=0;
		}

	}

	pCacheSets->ValidSets = FilledSetIndex;
	return FilledSetIndex;
}

int AdressToSet(char* Adress)
{
	return (((uint64_t)Adress >> 6) & 0b11111111111);
}

void RandomizeRawSet(Raw_Cache_Set* pSet)
{
	int LineIdx;
	int* LinesInSet = malloc(sizeof(int) * pSet->ValidLines);

	if (pSet->ValidLines == 0)
		return;

	// At the start of the randomization process, every line points to the
	// next line in the array.
	for (LineIdx = 0; LineIdx < pSet->ValidLines - 1; LineIdx++)
	{
		LinesInSet[LineIdx] = LineIdx + 1;
	}

	int LinesLeft = pSet->ValidLines - 1;
	int CurrLine = 0;

	// Now go over the linked list and randomize it
	for (LineIdx = 0; LineIdx < pSet->ValidLines && LinesLeft > 0; LineIdx++)
	{
		int NewPos = rand() % LinesLeft;
		unsigned int RandomLine = LinesInSet[NewPos];
		LinesInSet[NewPos] = LinesInSet[LinesLeft - 1];
		LinesInSet[LinesLeft - 1] = 0;
		LinesLeft--;
		*((uint64_t*)pSet->Lines[CurrLine]) = ((uint64_t)pSet->Lines[RandomLine]);
		CurrLine = RandomLine;
	}

	*((uint64_t*)pSet->Lines[CurrLine]) = 0;

	free(LinesInSet);
}

void RandomizeSet(Cache_Set* pSet)
{
	int LineIdx;
	int* LinesInSet = malloc(sizeof(int) * pSet->ValidLines);

	if (pSet->ValidLines == 0){
		free(LinesInSet);
		return;
	}

	// At the start of the randomization process, every line points to the
	// next line in the array.
	for (LineIdx = 0; LineIdx < pSet->ValidLines - 1; LineIdx++)
	{
		LinesInSet[LineIdx] = LineIdx + 1;
	}

	int LinesLeft = pSet->ValidLines - 1;
	int CurrLine = 0;

	// Now go over the linked list and randomize it
	for (LineIdx = 0; LineIdx < pSet->ValidLines && LinesLeft > 0; LineIdx++)
	{
		int NewPos = rand() % LinesLeft;
		unsigned int RandomLine = LinesInSet[NewPos];
		LinesInSet[NewPos] = LinesInSet[LinesLeft - 1];
		LinesInSet[LinesLeft - 1] = 0;
		LinesLeft--;
		*((uint64_t*)pSet->Lines[CurrLine]) = ((uint64_t)pSet->Lines[RandomLine]);
		CurrLine = RandomLine;
	}

	*((uint64_t*)pSet->Lines[CurrLine]) = 0;

	free(LinesInSet);
}

int FindSet(Raw_Cache_Set* pBigSet, Cache_Set* pSmallSet, char* pConflictingLine,int hitThreshold,int missThreshold){

char* pNewSetEnd 					= NULL;
char* pNonConflictingAnchor 		= pBigSet->Lines[0];
char* pLinesAnchor 					= pBigSet->Lines[0];
char* pLinesCurrentEnd				= pBigSet->Lines[0];
int ConflictLineIndex;
int LineInEvictionSet 				= 0;
int CurrentConflictSize 			= pBigSet->ValidLines;
Raw_Cache_Set tmpSet;
int SetFound						= 0;
int BigSetIdx, SmallSetIdx;
int WhileIdx = 0;
int LinesToCheck;

// Big set is the linked list with the potential lines for the set.
// If its less then LINES_IN_SET there isn't really anything to do.
if (pBigSet->Lines[0] == NULL || pBigSet->ValidLines < LINES_IN_SET){
/*	if(pBigSet->Lines[0] == NULL )
		printf("1 error\n");
	else if( pBigSet->ValidLines < LINES_IN_SET)
		printf("2 error\n");*/
	return 0;
}

RandomizeRawSet(pBigSet);

// Find the end of the set
while (*(char**)pLinesCurrentEnd != 0)
{
	pLinesCurrentEnd = *(char**)pLinesCurrentEnd;
}

// If we haven't found enough lines and not enough left to check, leave.
if (pBigSet->ValidLines < LINES_IN_SET)
{
	//printf("3 error\n");
	return 0;
}
// If we have exactly 12 lines, then we already have the full set.
else if (pBigSet->ValidLines == LINES_IN_SET)
{
	memcpy(&tmpSet, pBigSet, sizeof(tmpSet));
	LineInEvictionSet = LINES_IN_SET;
}
// Many lines are available, need to extract the ones which belong to the set.
else
{
	// Due to noise we might not be able to filter all irrelevant lines on first try.
	// So keep trying!
	while (LineInEvictionSet < LINES_IN_SET)
	{
		LinesToCheck = pBigSet->ValidLines - LineInEvictionSet;
		WhileIdx++;

		// Remerge the 2 lists together, and try finding conflicting lines once more
		if (WhileIdx > 1)
		{
			*(char**)pNewSetEnd = pLinesAnchor;
			pLinesAnchor = pNonConflictingAnchor;
			CurrentConflictSize = pBigSet->ValidLines;
		}

		// Eh, if we tried 5 times and still haven't succeeded, just give up.
		// We don't want to work forever.
		if (WhileIdx > 5)
			break;

		pNewSetEnd = NULL;
		pNonConflictingAnchor = NULL;

		// Find all the lines in the conflict set which are the same set as the target.
		for (ConflictLineIndex = 0; ConflictLineIndex < LinesToCheck; ConflictLineIndex++)
		{
			char* pCurrLine = pLinesAnchor;

			if (pLinesAnchor == NULL)
			{
				break;
			}

			// Pop the first line out of the conflict set.
			pLinesAnchor = *((char**)pLinesAnchor);

			// If the line we removed conflicted with the new line, add the line to the set.
			if (!ProbeManyTimes(pLinesAnchor, CurrentConflictSize - 1, pConflictingLine,hitThreshold,missThreshold)&&!ProbeManyTimes(pLinesAnchor, CurrentConflictSize - 1, pConflictingLine,hitThreshold,missThreshold))
			{
				tmpSet.Lines[LineInEvictionSet] = pCurrLine;
				*(char**)pCurrLine = 0;

				LineInEvictionSet++;
				*((char**)pLinesCurrentEnd) = pCurrLine;
				pLinesCurrentEnd = pCurrLine;
			}
			// Line does not conflict, and is therefore not interesting, move it to non conflicting list.
			else
			{
				if (pNewSetEnd == NULL)
					pNewSetEnd = pCurrLine;

				CurrentConflictSize--;
				*((char**)pCurrLine) = pNonConflictingAnchor;
				pNonConflictingAnchor = pCurrLine;
			}
		}
	}
}

// Lets check wether these lines conflict
if (LineInEvictionSet >= LINES_IN_SET)			//Allow only 12 lines/set!!!!
{
	tmpSet.ValidLines = LineInEvictionSet;
	RandomizeRawSet(&tmpSet);

	// Make sure the new set still conflicts with the line.
	if (ProbeManyTimes(tmpSet.Lines[0], tmpSet.ValidLines, pConflictingLine,hitThreshold,missThreshold) &&
		ProbeManyTimes(tmpSet.Lines[0], tmpSet.ValidLines, pConflictingLine,hitThreshold,missThreshold) &&
		ProbeManyTimes(tmpSet.Lines[0], tmpSet.ValidLines, pConflictingLine,hitThreshold,missThreshold))
	{
		// If there are too many lines, keep trimming then down.
		if (LineInEvictionSet > LINES_IN_SET){
			return 0;
			//SetFound = DecreaseSetSize(&tmpSet, pConflictingLine);
		//	printf("sdsdg");
		}
		else
			SetFound = 1;
	}else{
		return 0;
	}

	// Yay, the set is valid, lets call it a day.
	if (SetFound)
	{
		// Remove the lines in the new set from the list of potential lines.
		for (SmallSetIdx = 0; SmallSetIdx < tmpSet.ValidLines; SmallSetIdx++)
		{
			BigSetIdx = 0;
			pSmallSet->Lines[SmallSetIdx] = tmpSet.Lines[SmallSetIdx];

			while (BigSetIdx < pBigSet->ValidLines)
			{
				// if its the same line, remove it from the big set.
				if (pBigSet->Lines[BigSetIdx] == tmpSet.Lines[SmallSetIdx])
				{
					pBigSet->Lines[BigSetIdx] = pBigSet->Lines[pBigSet->ValidLines - 1];
					pBigSet->Lines[pBigSet->ValidLines - 1] = 0;
					pBigSet->ValidLines--;
					break;
				}

				BigSetIdx++;
			}
		}

		pSmallSet->ValidLines = tmpSet.ValidLines;

		return 1;
	} else {
		//PRINTF("Bad set!\r\n");
	}
} else
{
	//PRINTF("Not enough lines %d in set\r\n", LineInEvictionSet);
}
return 0;

}
