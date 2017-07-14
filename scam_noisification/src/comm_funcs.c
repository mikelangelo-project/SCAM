#include "comm_funcs.h"

int s, b, l, fd, sa, bytes;
char buf[BUF_SIZE];			/* buffer for outgoing file */
struct sockaddr_in channel;		/* hold's IP address */

int create_connection(){
	/* Build address structure to bind to socket. */
	memset(&channel, 0, sizeof(channel));	/* zero channel */
	channel.sin_family = AF_INET;
	channel.sin_addr.s_addr = htonl(INADDR_ANY);
	channel.sin_port = htons(SERVER_PORT);

	/* Passive open. Wait for connection. */
	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); /* create socket */
	if (s < 0) perror("socket failed");

	b = bind(s, (struct sockaddr *) &channel, sizeof(channel));
	if (b < 0) perror("bind failed");

	l = listen(s, QUEUE_SIZE);		/* specify queue size */
	if (l < 0) perror("listen failed");

	/* Socket is now set up and bound. Wait for connection and process it. */
	sa = accept(s, 0, 0);		/* block for connection request */
	if (sa < 0) perror("accept failed");

	printf("TCP Socket with server is open\n");
	return EXIT_SUCCESS;
}

int recv_msg(){
	bzero(buf,BUF_SIZE);
	bytes = recv(sa,buf,BUF_SIZE,0);
	if (bytes == ERR){
		perror("recv() failed");
	} else if (!bytes){
		printf("Server Closed\n");
	} else if (strcmp(buf,"ServerReq")){
		printf("Got it!\n");
		//check msg content
	} else {
		printf("unknown msg format (continue anyway)\n");
	}
	return EXIT_SUCCESS;
}

int send_msg(){
	const char msg[] = "RankFinished";
	bytes = send(sa,msg,strlen(msg),0);
	if (bytes == ERR){
		perror("recv() failed");
	} else if (!bytes){
		printf("Server Closed\n");
	} else if (bytes==3){
		printf("Got it!\n");
	}
	return EXIT_SUCCESS;
}

int close_conenction(){
	close(sa);				 /* close connection */
	return EXIT_SUCCESS;
}
