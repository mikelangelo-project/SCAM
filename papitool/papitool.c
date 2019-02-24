#include <stdio.h>
#include <unistd.h>
#include "papi_test.h"
#include <time.h>
#include "argparse.h"
#include  <signal.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_EVENTS 50
static const char *const usage[] = {
    "./papitool [options] [[--] args]",
    "./papitool [options]",
    NULL,
};
int flag_monitor = 0;
int flag_run = 1;
void sig_handler(int signo)
{
  if (signo == SIGUSR1){
      flag_monitor = !flag_monitor;
      fprintf(stderr,"Monitor %s\n",(flag_monitor ? "on" : "off"));
    }
  if (signo == SIGINT){
      flag_run = 0;
      fprintf(stderr,"Catched ctrl+c \n");
    }
}

void addEventName(int EventSet, char *eventName) {
    int eventCode = 0;
    int retval;
    char msg[PAPI_MAX_STR_LEN];
    PAPI_event_name_to_code(eventName, &eventCode );
    retval = PAPI_add_event(EventSet, eventCode);
    if ( retval != PAPI_OK ) {
        sprintf(msg, "PAPI_add_event (%s)", eventName);
        test_fail_exit( __FILE__, __LINE__, msg, retval );
    }
}

int addEventNames(const char *filePath, int EventSet) {
	int i = 0;
	size_t len = 0;
	FILE *fp;
	char *tempStr;
    int nEvents = 0;
	fp = fopen(filePath, "r");
	if (fp == NULL) {
		perror("Events file path error");
		exit(1);
	}
    
	while (getline(&tempStr, &len, fp) != -1){
		if (tempStr[0] != '#') {
			//strncpy(cfg->events[i], tempStr, strlen(tempStr) - 1); // Remove final LF
            tempStr[strlen(tempStr)-1] = '\0';
			addEventName(EventSet,tempStr);
            fprintf(stderr, "%s\n", tempStr);
			i++;
		}
	}
    
	free(tempStr);
	nEvents = i;
    fprintf(stderr, "Monitoring %d events\n", nEvents);
	fclose(fp);
    return nEvents;
}



int main( int argc, char **argv ) {
    printf("pid = %d\n",getpid());
	int interval = 0;
    int nsamples = 0;
	int attach = 0;
    const char *cmd = NULL; 
    const char *eventsFilePath = "events.conf"; 
    const char *log = NULL; 
    
    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_GROUP("Basic options"),
        OPT_INTEGER('i', "interval", &interval, "Interval between samples [usec]"),
        OPT_INTEGER('n', "nsamples", &nsamples, "Num of samples"),
        OPT_STRING('e', "events", &eventsFilePath, "Events file path"),
        OPT_STRING('c', "cmd", &cmd, "Command to execute"),
        OPT_STRING('l', "log", &log, "logfile"),
		OPT_INTEGER('a', "attach", &attach, "PID to attach"),
        OPT_END(),
	};
	
	struct argparse argparse;
    argparse_init(&argparse, options, usage, 0);
    argparse_describe(&argparse, "\n", "\n");
    argc = argparse_parse(&argparse, argc, (const char**)argv);
    
	int retval, pid, EventSet = PAPI_NULL;
	long long int values[MAX_EVENTS] = {0};
	PAPI_option_t opt;

	if ( ( retval = PAPI_library_init( PAPI_VER_CURRENT ) ) != PAPI_VER_CURRENT )
		test_fail_exit( __FILE__, __LINE__, "PAPI_library_init", retval );

	if ( ( retval = PAPI_create_eventset( &EventSet ) ) != PAPI_OK )
		test_fail_exit( __FILE__, __LINE__, "PAPI_create_eventset", retval );

	if ( ( retval = PAPI_assign_eventset_component( EventSet, 0 ) ) != PAPI_OK )
		test_fail_exit( __FILE__, __LINE__, "PAPI_assign_eventset_component", retval );
	
	if (attach){
        fprintf(stderr, "Attach mode. PID: %d\n", attach);
		retval = PAPI_attach( EventSet, ( unsigned long ) attach );
        if ( retval != PAPI_OK )
			test_fail_exit( __FILE__, __LINE__, "PAPI_attach", retval );
	} else {
		fprintf(stderr, "Inherit mode\n");
		memset( &opt, 0x0, sizeof ( PAPI_option_t ) );
		opt.inherit.inherit = PAPI_INHERIT_ALL;
		opt.inherit.eventset = EventSet;
		if ( ( retval = PAPI_set_opt( PAPI_INHERIT, &opt ) ) != PAPI_OK ) {
			if ( retval == PAPI_ECMP) {
				test_skip( __FILE__, __LINE__, "Inherit not supported by current component.\n", retval );
			} else {
				test_fail_exit( __FILE__, __LINE__, "PAPI_set_opt", retval );
			}
		}
	}
	fprintf(stderr, "Using events file: %s\n", eventsFilePath);
    int nevents = addEventNames(eventsFilePath,EventSet);

	if ( retval != PAPI_OK ) {
		test_fail_exit( __FILE__, __LINE__, "PAPI_add_event", retval );
	}

	//PAPI START
	if ( ( retval = PAPI_start( EventSet ) ) != PAPI_OK )
		test_fail_exit( __FILE__, __LINE__, "PAPI_start", retval );

	//FORK
    if (cmd){
        pid = fork(  );
        if ( pid == 0 ) {
            fprintf(stderr,"Fork() start, Executing: %s\n",cmd);
            system(cmd);
            fprintf(stderr,"Fork() done.\n");
            exit( 0 );
        }
    }
    if (signal(SIGUSR1, sig_handler) == SIG_ERR)
        printf("\ncan't catch SIGUSR1\n");
    if (signal(SIGINT, sig_handler) == SIG_ERR)
        printf("\ncan't catch SIGINT\n");
	
	/*if ( waitpid( pid, &status, 0 ) == -1 ) {
	  perror( "waitpid()" );
	  exit( 1 );
	}*/
    FILE * out = stdout;
    if (log){
        out = fopen(log, "wb");
    }
	fprintf(stderr,"Monitor is ready\n");
	while (flag_run){
        if (flag_monitor || attach){
            if ( ( retval = PAPI_read( EventSet, values ) ) != PAPI_OK )
                test_fail_exit( __FILE__, __LINE__, "PAPI_read", retval );

            if ( ( retval = PAPI_reset( EventSet ) ) != PAPI_OK )
                test_fail_exit( __FILE__, __LINE__, "PAPI_reset", retval );


            for (int i = 0; i < nevents; i++){
                fprintf(out,"%lld",values[i]);
                if (i<nevents-1)
                fprintf(out,",");
            }
            fprintf(out,"\n");
            fflush(out);
        }
        usleep(interval);
	}
    if (log)
	    fclose(out);
	if ( ( retval = PAPI_stop( EventSet, values ) ) != PAPI_OK )
		test_fail_exit( __FILE__, __LINE__, "PAPI_stop", retval );
    
	exit(EXIT_SUCCESS);
}
