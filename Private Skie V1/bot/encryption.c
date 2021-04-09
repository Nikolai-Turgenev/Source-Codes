#define _GNU_SOURCE

#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <time.h>
#include <string.h>

#include "headers/encryption.h"
#include "headers/utils.h"

char dec_buf[512];
char dstring[30];

void decrypt_for_recv(char *str) {
	int i;
	strcpy(dstring, str);
	for(i = 0; i < util_strlen(dstring); i++){
		dstring[i] = dstring[i] - TABLE_KEY; 

	}
	strcpy(dec_buf, dstring);
}
