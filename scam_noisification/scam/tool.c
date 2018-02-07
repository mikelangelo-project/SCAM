/*
 * Copyright 2016 CSIRO
 *
 * This file is part of Mastik.
 *
 * Mastik is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Mastik is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Mastik.  If not, see <http://www.gnu.org/licenses/>.
 */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <util.h>
#include <l3.h>
#include <low.h>

#include <time.h>
#include <sched.h>
#include "vlist.h"

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

//Parameters

#define NUM_OF_ACTIVE_SETS 300
#define NUM_OF_SUSPECTS 400
#define LINES_PER_SET 12


//Sampling Parameter
#define SAMPLES_FOR_RANKING 50000
#define ATTACKER_PHASES 20
#define RANKING_PHASES 10
#define INTERVAL_DEF 6666
#define SPECIFICSET_SAMPLES 500000

#define INPUTSIZE 		100
#define RUMBLE_CORE_0 	0
#define RUMBLE_CORE_1 	1

sig_atomic_t doNoise = 0;
sig_atomic_t noise1up = 0, noise2up = 0;

l3pp_t l3;
char * buffer;
bool *ActiveSets = NULL;
int NumOfActiveSets = 0;
int nsets = 0;
int setsPerSlice = 0;

int INTERVAL = INTERVAL_DEF;

int nslices = 0;
int associativity;
char input[INPUTSIZE];
char *token;
char str [INPUTSIZE];
struct stat st = {0};


uint16_t *specific_res;

char ** lines;
pthread_t rumble_tid_1,rumble_tid_2,signal_tid;
char signalFileName [100];

int rumble_core_0 = RUMBLE_CORE_0;
int rumble_core_1 = RUMBLE_CORE_1;

void pinToCore(int coreId){
	int rs;
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(coreId, &cpuset);
	rs = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
	if (rs) {
		perror("pthread_setaffinity_np");
		exit(EXIT_FAILURE);
	}
}


void *rumble(void* args){
	int * prms = (int*)args;
	int coreNum = prms[0];
	int lineOffset = prms[1];
	int intensity = prms[2];
	int isComplementTo = intensity > LINES_PER_SET;
	intensity = intensity % (LINES_PER_SET+1);

	free(prms);
	pinToCore(coreNum);
	if (!NumOfActiveSets) {
		printf("Nothing to Rumble!\n");
		fflush(stdout);
		doNoise = 0;
		return (void*) EXIT_SUCCESS;
	}

	char ** lines_arr = calloc(sizeof(char*),(intensity * NumOfActiveSets));

	int k = 0;
	int setForProbe = -1;

	for (int i = 0; i < intensity; i++) {
		for (int j = 0; j < nsets; j++){
			if (ActiveSets[j]){
				if(isComplementTo && setForProbe==-1){
					setForProbe=j;
					continue;
				}
				lines_arr[k] = l3_getline(l3, j, i+lineOffset);
				k++;
			}
		}
	}

	void * p;
	if(isComplementTo)
		p = sethead(l3,setForProbe);
	int flip = 0;



	if(!isComplementTo){
		while (doNoise) {
			for (int i = 0; i < (NumOfActiveSets) * (intensity); i++) {
				memaccess(lines_arr[i]);
			}
		}
	}else{
		while (doNoise) {
			int i, r;
			flip = !flip;
			r = flip ? probecount(p) : bprobecount(p);
			r = (r ? 1 : 0);
			for (i = 0; i < (NumOfActiveSets-1) * (intensity-r); i++) {
				memaccess(lines_arr[i]);
			}
		}
	}

	free(lines_arr);
	return (void*) EXIT_SUCCESS;
}


int* countMisses(int * dest) {

	if (dest == NULL)
		dest = (int*) calloc(nsets, sizeof(int));
	else
		bzero(dest,nsets * sizeof(int));
	uint16_t *res = calloc(SAMPLES_FOR_RANKING / RANKING_PHASES, sizeof(uint16_t));

	for (int k = 0; k < RANKING_PHASES; k++){
		printf("Monitor phase %d [%.2f%]\n", k,	(double) k / RANKING_PHASES * 100);
		fflush(stdout);
		for (int i = 0; i < nsets; i++) {
			l3_unmonitorall(l3);
			l3_monitor(l3, i);

			l3_repeatedprobecount(l3, SAMPLES_FOR_RANKING/RANKING_PHASES, res, INTERVAL);
			for (int j = 0; j < SAMPLES_FOR_RANKING/RANKING_PHASES; j++) {
				int16_t r = (int16_t) res[j];
				dest[i] += (r > 0);
			}
		}
	}
	free(res);
	return dest;
}

int CacheMapping(){
	printf("Mapping the Cache...\n");
	fflush(stdout);

	l3 = l3_prepare(NULL);

	if (!l3)
		return EXIT_FAILURE;

	nsets = l3_getMappedSets(l3);

	printf("Mapping Done... Found %d sets\n", nsets);
	fflush(stdout);

	if (nsets != l3_getSets(l3)){
		printf("mapping failure, %d != %d\n",nsets,l3_getSets(l3));
		fflush(stdout);
		return EXIT_FAILURE;
	}

	if (!ActiveSets)
		ActiveSets = (bool*) calloc(nsets, sizeof(bool));
	else
		bzero(ActiveSets,nsets * sizeof(bool));

	nslices = l3_getSlices(l3);
	setsPerSlice = nsets / nslices;
	associativity = l3_getAssociativity(l3);
	return EXIT_SUCCESS;
}

float monitorSpecificSet(l3pp_t l3, int setToMonitor, char * filename) {
	int zeroCounter;
	uint64_t AvrMiss;
	FILE * f_specificset;
	if (setToMonitor < 0 || setToMonitor >= nsets) {
		printf("invalid set number!\n");
		return 0;
	}
	//	printf("filename = %s\n", filename);
	CASE5: printf("Monitoring %d...", setToMonitor);
	fflush(stdout);
	f_specificset = fopen(filename, "w+");
	l3_unmonitorall(l3);
	l3_monitor(l3, setToMonitor);
	l3_repeatedprobecount(l3, SPECIFICSET_SAMPLES, specific_res, INTERVAL);

	zeroCounter = 0;
	AvrMiss = 0;
	for (int j = 0; j < SPECIFICSET_SAMPLES; j++) {
		int16_t r = (int16_t) specific_res[j];
		if (r < 13 && r > 0)
			AvrMiss += (uint16_t) r;
		//			printf("%d - %ld, ", (uint16_t) r, AvrMiss);
		if (!r)
			zeroCounter++;
		fprintf(f_specificset, "%d ", r);
	}
	fclose(f_specificset);

	printf("\tdone \t avrMiss %.2f\t (%.2f %)!\n", (float) AvrMiss / SPECIFICSET_SAMPLES,(float) (zeroCounter) / SPECIFICSET_SAMPLES * 100);

	return (float) (zeroCounter) / SPECIFICSET_SAMPLES * 100;
}
void syncSlices(){
	//assuming there is another mastik already open with the same activesets list
	for (int s = 0; s < nslices; s++){
		int set = -1;
		for (int i = s*setsPerSlice; i < s*setsPerSlice + setsPerSlice; i++){
			if (ActiveSets[i] && !ActiveSets[(i+setsPerSlice)%nsets] && !ActiveSets[(i+2*setsPerSlice)%nsets] && !ActiveSets[(i+3*setsPerSlice)%nsets]){
				set = i;
				break;
			}
		}
		int toSlice=-1;
		int minValue = 100;
		for (int i = 0; i < 4; i++){
			float missPercents = monitorSpecificSet(l3,(set + i*setsPerSlice)%nsets, "specificset_res.txt");
			if (missPercents < minValue){
				minValue = missPercents;
				toSlice = (s+i)%4;
			}
		}
		printf("Swap slice %d and %d\n",s,toSlice);
		l3_swapslices(l3,s,toSlice);
	}
}

int getInput(char * buffer) {
	int result;
	token = strtok(buffer, " ");
	result = atoi(token);
	printf("%d\n",result);
	fflush(stdout);
	return result;
}

void turnOffNoise(){
	printf("Turn off Noise!\n");
	fflush(stdout);
	doNoise = false;
	if (noise1up && pthread_join(rumble_tid_1, NULL)) {
		printf("pthread_join 1 failed\n");
		fflush(stdout);
		return;
	}
	if (noise2up && pthread_join(rumble_tid_2, NULL)) {
		printf("pthread_join 2 failed\n");
		fflush(stdout);
		return;
	}

	noise1up = false;
	noise2up = false;

	printf("Noise is off\n");
	fflush(stdout);
}


void turnOnNoise(int core1, int offest1, int intensity1 ,int core2, int offest2, int intensity2){
	int * rumbleArgs1, * rumbleArgs2;

	printf("Noise #1 is on\n");
	doNoise = true;
	noise1up = true;
	rumbleArgs1 = calloc(3,sizeof(int));
	rumbleArgs1[0] = core1;
	rumbleArgs1[1] = offest1;
	rumbleArgs1[2] = intensity1;

	if (pthread_create(&rumble_tid_1, NULL, &rumble, rumbleArgs1)) {
		printf("pthread_create 1 failed\n");
		fflush(stdout);
		return;
	}

	if (!intensity2)
		return;

	printf("Noise #2 is on\n");
	noise2up = true;
	rumbleArgs2 = calloc(3,sizeof(int));
	rumbleArgs2[0] = core2;
	rumbleArgs2[1] = offest2;
	rumbleArgs2[2] = intensity2;
	if (pthread_create(&rumble_tid_2, NULL, &rumble, rumbleArgs2)) {
		printf("pthread_create 2 failed\n");
		fflush(stdout);
		return;
	}
}


int tcpConnect(int port){
	int clientSocket;
	struct sockaddr_in serverAddr;
	socklen_t addr_size;
	/*---- Create the socket. The three arguments are: ----*/
	/* 1) Internet domain 2) Stream socket 3) Default protocol (TCP in this case) */
	clientSocket = socket(PF_INET, SOCK_STREAM, 0);
	/*---- Configure settings of the server address struct ----*/
	/* Address family = Internet */
	serverAddr.sin_family = AF_INET;
	/* Set port number, using htons function to use proper byte order */
	fflush(stdout);
	printf("port = %d\n",port);
	serverAddr.sin_port = htons(port);
	/* Set IP address to localhost */
	serverAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	/* Set all bits of the padding field to 0 */
	memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);
	/*---- Connect the socket to the server using the address struct ----*/
	addr_size = sizeof serverAddr;
	printf("Trying to connect...\t"); fflush(stdout);
	while(connect(clientSocket, (struct sockaddr *) &serverAddr, addr_size) < 0){
		sleep(1);
	}
	printf("Connected!\n");
	return clientSocket;

}
int intComparator(const void* p1, const void* p2){
	int a = *(int*)p1;
	int b = *(int*)p2;
	return a-b;
}

int main(int ac, char **av) {
	delayloop(3000000000U);
	//	pinToCore(ATTACKER_CORE);
	void * buff = NULL;
	int rs;
	int tmp;
	int intensity_arg1, intensity_arg2;
	int intensity;
	uint64_t t;
	int option, setToMonitor = 0;
	uint64_t AvrMiss;

	//files
	FILE  *f_suspectedsets = NULL;

	int setIdx = 0;
	int result, *result1,*result2;
	void** lines_array;
	void** linesToRead;
	int sliceNumA = 0,sliceNumB = 0;
	int *res_rank1 = NULL, *res_rank2 = NULL, *res_rank_tmp = NULL;
	specific_res = calloc(SPECIFICSET_SAMPLES, sizeof(uint16_t));

	lines = calloc(sizeof(void*),nsets * associativity);
	uint16_t *twosets_res = calloc(SPECIFICSET_SAMPLES*2, sizeof(uint16_t));
	uint setToMonitor_1,setToMonitor_2;
	size_t len = 0;
	pid_t pid;
	void * lineAddr1, *lineAddr2;
	/* get the process id */

	if ((pid = getpid()) < 0)
		perror("unable to get pid");
	else
		printf("The process id is %d\n", pid);
	void * p;

	if (ac < 2){
		printf("missing port\n");
		exit(EXIT_FAILURE);
	}

	//socket
	int clientSocket;
	char buffer[1024];
	struct sockaddr_in serverAddr;
	socklen_t addr_size;
	int port;
	int settmp;

	CacheMapping();

	/*Start with establishing connection with SCAM*/
	port = atoi(av[1]);
	clientSocket = tcpConnect(port);


	while (true) {

		bzero(buffer,sizeof(char) * 1024);
		rs = recv(clientSocket , buffer , 1024 , 0);
		if (rs <= 0){
			perror("recv");
			tcpConnect(port);
			continue;
		} else {
			printf("Data received: %s\n",buffer);
		}
		option = getInput(buffer);

		switch (option) {
		case 1:
			CacheMapping();
			break;
		case 2:
			printf("Ranking I!\n");			fflush(stdout);
			res_rank1 = countMisses(res_rank1);
			putchar('\n');
			printf("Ranking I Done! Please Start the Target before Ranking II\n");
			fflush(stdout);
			break;
		case 3:
			printf("Ranking II!\n");			fflush(stdout);
			bzero(ActiveSets, nsets * sizeof(uint8_t));
			NumOfActiveSets = 0;
			res_rank2 = countMisses(res_rank2);
			putchar('\n');

			if (res_rank_tmp == NULL)
				res_rank_tmp = calloc(nsets,sizeof(int));
			else
				bzero(res_rank_tmp,nsets * sizeof(int));


			for (int i = 0; i < nsets; i++)
				res_rank_tmp[i] = res_rank2[i] - res_rank1[i];

			qsort(res_rank_tmp,nsets,sizeof(int),intComparator);

//			for (int i = 0; i < nsets; i++)
//				printf("%d ",res_rank_tmp[i]);

			tmp = res_rank_tmp[nsets - 1 - NUM_OF_SUSPECTS];

			for (int i = 0; i < nsets; i++){
				if (res_rank2[i] - res_rank1[i] >= tmp){
					NumOfActiveSets++;
					ActiveSets[i] = 1;
				}
			}

			printf("\nRanking II done!\n");
			printf("\nNum of suspects = %d!\n",NumOfActiveSets);
			fflush(stdout);

			break;
		case 6:
			if (doNoise){
				printf("doNoise = true, pause the noise and try again\n");
				break;
			}
			printf("Noise I!\n");

			puts("Enter intensity #1");
			intensity_arg1 = getInput(NULL);

			puts("Enter intensity #2");
			intensity_arg2 = getInput(NULL);

			puts("Enter core #1");
			rumble_core_0 = getInput(NULL);

			puts("Enter core #2");
			rumble_core_1 = getInput(NULL);

			turnOnNoise(rumble_core_0,0,intensity_arg1,rumble_core_1,6,intensity_arg2);

			break;
		case 7:
			turnOffNoise();
			break;
		case 9:
			printf("DummyRank = %d\n", NumOfActiveSets);
			NumOfActiveSets = 0;
			bzero(ActiveSets, nsets * sizeof(uint8_t));
			srand(rdtscp());
			for (int i = 0; i < NUM_OF_ACTIVE_SETS; i++){
				tmp = rand() % nsets;
				if (ActiveSets[tmp]){
					i--;
					continue;
				}
				ActiveSets[tmp] = true;
				NumOfActiveSets++;
			}
			printf("NumOfActiveSets = %d\n", NumOfActiveSets);
			break;
//		case 11:
//			setIdx = 0;
//			printf("Add sets to suspect list from file: \n");
//			f_suspectedsets = fopen("suspected_sets_num.txt", "r+");
//			bzero(input,INPUTSIZE);
//			while ((fscanf(f_suspectedsets,"%d", &setIdx))) {
//				if(setIdx==-1) break;
//				ActiveSets[setIdx] = 1;
//				NumOfActiveSets++;
//			}
//			fclose(f_suspectedsets);
//			break;

		case 13:
			setIdx = 0;
			printf("Erase Suspects List: ");
			NumOfActiveSets = 0;
			bzero(ActiveSets, nsets * sizeof(uint8_t));
			printf("Removed all!\n");
			break;

		case 16:
			printf("Suspected Sets:\n");
			f_suspectedsets = fopen("suspected_sets_num.txt", "w+");
			for (int i = 0; i < nsets; i++) {
				if (!(i % 2048))
					printf("\n%d-%d:\n", i, i + 2047);
				if (ActiveSets[i]) {
					fprintf(f_suspectedsets, "%d\n", i);
					printf("%d, ", i);
				}
			}
			printf("\n");
			fprintf(f_suspectedsets, "-1");
			fclose(f_suspectedsets);

			fflush(stdout);
			break;
		case 29:
			syncSlices();
			break;
		}

		rs = -1;
		while(rs < 0){
			sprintf(buffer,"ack %d",option);
			printf("sending %s\n", buffer);
			rs = send(clientSocket,buffer,strlen(buffer),0);
			if (rs <= 0){
				tcpConnect(port);
			}
		}

	}

	CLOSE:

	free(specific_res);
	free(ActiveSets);
	free(lines);
	if (res_rank1)
		free(res_rank1);
	if (res_rank2)
		free(res_rank2);
	l3_release(l3);
}
