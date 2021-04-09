#define _GNU_SOURCE

#ifdef DEBUG
#include <stdio.h>
#endif
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#include "headers/main.h"
#include "headers/utils.h"

int util_strlen(char *str){
    int c = 0;
    while (*str++ != 0)
        c++;
    return c;
} 

void util_memcpy(void *dst, void *src, int len) {
    char *r_dst = (char *) dst;
    char *r_src = (char *) src;
    while (len--)
        *r_dst++ = *r_src++;
}

int util_strcpy(char *dst, char *src) {
    int l = util_strlen(src);

    util_memcpy(dst, src, l + 1);

    return l;
}

BOOL util_strcmp(char *str1, char *str2) {
    int l1 = util_strlen(str1), l2 = util_strlen(str2);

    if (l1 != l2)
        return FALSE;

    while (l1--) {
        if (*str1++ != *str2++)
            return FALSE;
    }

    return TRUE;
}
