#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h> /* for close() for socket */ 
#include <stdlib.h>
#include "duckchat.h"
#include "duckchatserver.h"

#define REQ_ARGC 3
#define SERVER_NAME_MAX 256
#define MAX_PORT 65535

int sock, port;
char domain[SERVER_NAME_MAX];
ChannelList c_list;
UserList u_list;

void init_duckchat(char* buffer, SockAddrIn* sa){
    c_list.num_channels = 0;
    u_list.num_users = 0;
    int max_users = MAX_NO_CHANNELS*MAX_USERS_PER_CHANNEL;

    memset(buffer, 0, sizeof(buffer));

    //resolve the host name
    struct hostent *h;
    if((h=gethostbyname(domain)) == NULL){
        herror("Can't resolve hostname.\n");
        exit(EXIT_FAILURE);
    }

    //setup inbound socket
    memset(sa, 0, sizeof sa);
    sa->sin_family = AF_INET;
    sa->sin_addr = *((struct in_addr *)h->h_addr);
    //sa->sin_addr.s_addr = htonl(INADDR_ANY);
    sa->sin_port = htons(port);

    int i;
    for(i=0;i<MAX_NO_CHANNELS;i++)
        c_list.list[i].num_subers = 0;

    for(i=0;i<max_users;i++)
        u_list.users[i].num_subs = 0;
}


void handle_request(Req* r, SockAddrIn *to){

    switch(r->req_type){
        case REQ_LOGIN:
            handle_login((ReqLogin*)r, to);
            break;
        case REQ_LOGOUT:
            handle_logout((ReqLogout*)r, to);
            break;
        case REQ_JOIN:
            handle_join((ReqJoin*)r, to);
            break;
        case REQ_LEAVE:
            handle_leave((ReqLeave*)r, to);
            break;
        case REQ_SAY:
            handle_say((ReqSay*)r, to);
            break;
        case REQ_LIST:
            handle_list((ReqList*)r, to);
            break;
        case REQ_WHO:
            handle_who((ReqWho*)r, to);
            break;
        case REQ_KEEP_ALIVE:
            handle_keep_alive((ReqKeepAlive*)r);
            break;
        default:
            handle_error("ERR: Invalid request type.", to);
            break;
    }
}

int parse_args(int argc, char** argv){

    if(argc < REQ_ARGC){
        printf("ERR: Missing arguments\nUSAGE: %s hostname port\n", argv[0]);
        return -1;
    }
    else{
        int i;
        for(i=1;i<argc;i++){
            switch (i){
                case 1:
                    ;
                    int len = strlen(argv[i]);
                    if(len<=SERVER_NAME_MAX && len > 0){
                        strcpy(domain,argv[i]);
                    }
                    break;
                case 2:
                    port = atoi(argv[i]);
                    if(port>MAX_PORT || port<1){
                        printf("ERR: %d is an invalid port number.\n",port);
                        return -1;   
                    }
                    break;
                default:
                    break;
            }
        }
    }

    return 0;
}

void cleanup(void){
    if(sock)
        close(sock);
}

int main(int argc, char** argv)
{
    SockAddrIn sa; 
    char buffer[2048];
    ssize_t recsize;
    socklen_t fromlen;

    //checks correct usage of client and sets values
    if(parse_args(argc,argv)<0){
        //incorrect usage, exit
        exit(EXIT_FAILURE);
    }

    init_duckchat(buffer, &sa);
    fromlen = sizeof(sa);

    atexit(cleanup);

    //make socket and check for failure
    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (bind(sock, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
        perror("error bind failed");
        exit(EXIT_FAILURE);
    }

    //main program loop
    printf("Server started, waiting for requests.\n");
    for (;;) {
        recsize = recvfrom(sock, (void*)buffer, sizeof(buffer), 0, (SockAddrIn*)&sa, &fromlen);
        if (recsize < 0) {
            fprintf(stderr, "%s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        //majority of work done here
        handle_request(buffer, &sa);
    }

    return 0;
}
