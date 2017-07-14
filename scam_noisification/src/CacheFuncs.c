
#include "CacheDefs.h"
#include "CacheFuncs.h"

#include <stdlib.h>
#include <stdio.h>

__always_inline int ProbeAsm(char* pSet, int SetSize, char* pLine, int hitThreshold, int missThreshold)
{
	int Time;
	int j;
	char* pCurrLine = pSet;

	if (SetSize == 0)
		return 0;

	// Add the line to check to the end of the linked list so we go over it last.
	for (j = 0; j < SetSize - 1; j++)
	{
		pCurrLine = *(char**)pCurrLine;
	}

	*(char**)pCurrLine = pLine;
	*(char**)pLine = 0;

	// First of all, touch the record.
	Time = hitThreshold + 1;

	// While the results aren't obvious, keep probing this line.
	while ((Time > hitThreshold && Time < missThreshold) || (!hitThreshold && !missThreshold))
	{
		// Now go over all the records in the set for the first time.
		// We do this 2 times to be very very sure all lines have been loaded to the cache.

		// First load all data into registers to avoid unneccesarily touching the stack.
		asm volatile ("movl %0, %%ecx;" : : "r" (SetSize) : "ecx");
		asm volatile ("mov %0, %%r8;" : : "r" (pSet) : "r8");
		asm volatile ("mov %0, %%r9;" : : "r" (pLine) : "r9");

		asm volatile ("mov (%r9), %r10;");		// Touch the line we want to test, loading it to the cache.
		asm volatile ("lfence;");
		asm volatile ("1:");
		asm volatile ("mov (%r8), %r8;");		// Move to the next line in the linked list.
		asm volatile ("loop	1b;");

		// This is a copy of the previous lines.
		// We go over all the records for the second time.
		asm volatile ("movl %0, %%ecx;" : : "r" (SetSize) : "ecx");
		asm volatile ("mov %0, %%r8;" : : "r" (pSet) : "r8");
		asm volatile ("lfence;");
		asm volatile ("2:");
		asm volatile ("mov (%r8), %r8;");
		asm volatile ("loop	2b;");
#if (NUM_PROBE > 2)
		asm volatile ("movl %0, %%ecx;" : : "r" (SetSize) : "ecx");
		asm volatile ("mov %0, %%r8;" : : "r" (pSet) : "r8");
		asm volatile ("lfence;");
		asm volatile ("3:");
		asm volatile ("mov (%r8), %r8;");
		asm volatile ("loop	3b;");
#endif
#if (NUM_PROBE > 3)
		asm volatile ("movl %0, %%ecx;" : : "r" (SetSize) : "ecx");
		asm volatile ("mov %0, %%r8;" : : "r" (pSet) : "r8");
		asm volatile ("lfence;");
		asm volatile ("4:");
		asm volatile ("mov (%r8), %r8;");
		asm volatile ("loop	4b;");
#endif
#if (NUM_PROBE > 4)
		asm volatile ("movl %0, %%ecx;" : : "r" (SetSize) : "ecx");
		asm volatile ("mov %0, %%r8;" : : "r" (pSet) : "r8");
		asm volatile ("lfence;");
		asm volatile ("5:");
		asm volatile ("mov (%r8), %r8;");
		asm volatile ("loop	5b;");
#endif
#if (NUM_PROBE > 5)
		asm volatile ("movl %0, %%ecx;" : : "r" (SetSize) : "ecx");
		asm volatile ("mov %0, %%r8;" : : "r" (pSet) : "r8");
		asm volatile ("lfence;");
		asm volatile ("6:");
		asm volatile ("mov (%r8), %r8;");
		asm volatile ("loop	6b;");
#endif
#if (NUM_PROBE > 6)
		asm volatile ("movl %0, %%ecx;" : : "r" (SetSize) : "ecx");
		asm volatile ("mov %0, %%r8;" : : "r" (pSet) : "r8");
		asm volatile ("lfence;");
		asm volatile ("7:");
		asm volatile ("mov (%r8), %r8;");
		asm volatile ("loop	7b;");
#endif
		// Now check if the original line is still in the cache
		asm volatile ("lfence;");
		asm volatile ("rdtsc;");				// Get the current time in cpu clocks
		asm volatile ("movl %eax, %edi;");		// Save it aside in edi
		asm volatile ("lfence;");
		asm volatile ("mov (%r8), %r8;");		// Touch the last line in the linked list, which is the new line.
		asm volatile ("lfence;");
		asm volatile ("rdtsc;");				// Get the time again.
		asm volatile ("subl %edi, %eax;");		// Calculate how much time has passed.
		asm volatile ("movl %%eax, %0;" : "=r" (Time));  // Save that time to the Time variable

		if(!hitThreshold) break;
	}

	*(char**)pCurrLine = 0;

	return  !hitThreshold ? Time : Time >= missThreshold;
}


int PollAsm(char* pSet, int SetSize)
{
	long int Miss = 0;
	int PollTimeLimit = CACHE_POLL_MIN_SET_MISS_THRESHOLD_IN_TICKS;

	if (SetSize == 0)
		return 0;

	// Read the variables to registers.
	asm volatile ("mov %0, %%r8;" : : "r" (pSet) : "r8");
	asm volatile ("movl %0, %%ebx;" : : "r" (PollTimeLimit) : "ebx");
	asm volatile ("mov %0, %%r10;" : : "r" (Miss) : "r10");
	asm volatile ("movl %0, %%ecx;" : : "r" (SetSize) : "ecx");

	// This loop goes over all lines in the set and checks if they are still in the cache.
	asm volatile ("CHECK_LINE:");

	asm volatile ("rdtsc;");			// Get the current time
	asm volatile ("movl %eax, %edi;");	// Save it in edi
	asm volatile ("mov (%r8), %r8;");	// Read the next line into the cache
	asm volatile ("lfence;");
	asm volatile ("rdtsc;");			// Get time again
	asm volatile ("subl %edi, %eax;");	// Check how much time has passed.
	asm volatile ("cmp %eax, %ebx;");	// Is it too much?
	asm volatile ("jnl HIT");			// If not, the line is still in the cache (Cache Hit)
	asm volatile ("MISS:");				// If yes, then this is a cache miss.
	asm volatile ("inc %r10");			// Count how many cache misses we've had so far.
	asm volatile ("HIT:");

	asm volatile ("loop	CHECK_LINE;");

	asm volatile ("mov %%r10, %0;" : "=r" (Miss));	// Save to variable how many misses have we seen.

	return Miss;
}

int ProbeManyTimes(char* pSet, int SetSize, char* pLine, int hitThreshold, int missThreshold)
{
	int Misses = 4;
	int Result = 0;

	// We don't trust probe, because probe code could conflict with the line we are probing.
	// And there could be noise and prefetcher and whatnot.
	// So we probe many times, then check if we've had enough misses/hits to make a decision.
	// Each call to probe asm duplicates its code, promising they do not sit in the same area in the cache.
	// Thus reducing the chance all calls will conflict with the lines.
	// We also add variables to the stack in between calls, to make sure we also use different stack
	// memory in each call.

	// Current probe count is 15.
	// This code was written when the code was much less stable and many probes were needed.
	// We probably don't need these many probe operations anymore.
	// Still, these are cheap operations, so we can allow ourselves.
	while (Misses > 2 && Misses < 5) {
		Misses = 0;
		Result = ProbeAsm(pSet, SetSize, pLine, hitThreshold, missThreshold);
		Misses += Result;
		char Pad1[128];
		Result = ProbeAsm(pSet, SetSize, pLine, hitThreshold, missThreshold);
		Misses += Result;
		char Pad2[128];
		Result = ProbeAsm(pSet, SetSize, pLine, hitThreshold, missThreshold);
		Misses += Result;
		char Pad3[128];
		Result = ProbeAsm(pSet, SetSize, pLine, hitThreshold, missThreshold);
		Misses += Result;
		char Pad4[128];
		Result = ProbeAsm(pSet, SetSize, pLine, hitThreshold, missThreshold);
		Misses += Result;
		char Pad5[128];
		Result = ProbeAsm(pSet, SetSize, pLine, hitThreshold, missThreshold);
		Misses += Result;
		char Pad6[128];
		Result = ProbeAsm(pSet, SetSize, pLine, hitThreshold, missThreshold);
		Misses += Result;
		char Pad7[128];
		Result = ProbeAsm(pSet, SetSize, pLine, hitThreshold, missThreshold);
		Misses += Result;
		char Pad8[128];
		Result = ProbeAsm(pSet, SetSize, pLine, hitThreshold, missThreshold);
		Misses += Result;
		char Pad9[128];
		Result = ProbeAsm(pSet, SetSize, pLine, hitThreshold, missThreshold);
		Misses += Result;
		char Pad10[128];
		Result = ProbeAsm(pSet, SetSize, pLine, hitThreshold, missThreshold);
		Misses += Result;
		char Pad11[128];
		Result = ProbeAsm(pSet, SetSize, pLine, hitThreshold, missThreshold);
		Misses += Result;
		char Pad12[128];
		Result = ProbeAsm(pSet, SetSize, pLine, hitThreshold, missThreshold);
		Misses += Result;
		char Pad13[128];
		Result = ProbeAsm(pSet, SetSize, pLine, hitThreshold, missThreshold);
		Misses += Result;
		char Pad14[128];
		Result = ProbeAsm(pSet, SetSize, pLine, hitThreshold, missThreshold);
		Misses += Result;
		memset(Pad1, 0, sizeof(Pad1));
		memset(Pad2, 0, sizeof(Pad2));
		memset(Pad4, 0, sizeof(Pad4));
		memset(Pad3, 0, sizeof(Pad3));
		memset(Pad5, 0, sizeof(Pad4));
		memset(Pad6, 0, sizeof(Pad3));
		memset(Pad7, 0, sizeof(Pad4));
		memset(Pad8, 0, sizeof(Pad4));
		memset(Pad9, 0, sizeof(Pad4));
		memset(Pad10, 0, sizeof(Pad4));
		memset(Pad11, 0, sizeof(Pad4));
		memset(Pad12, 0, sizeof(Pad4));
		memset(Pad13, 0, sizeof(Pad4));
		memset(Pad14, 0, sizeof(Pad4));
	}

	return Misses >= 10;
}

