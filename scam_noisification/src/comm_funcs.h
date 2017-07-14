#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef COMM_FUNCS_H
#define COMM_FUNCS_H

#define SERVER_PORT 12345		/* arbitrary, but client and server must agree */
#define BUF_SIZE 4096			/* block transfer size */
#define QUEUE_SIZE 10
#define ERR -1

int create_connection();
int recv_msg();
int send_msg();
int close_conenction();

#endif
