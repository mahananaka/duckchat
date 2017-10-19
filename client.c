#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "duckchat.h"

#define BUFF_SIZE 2048
#define REQ_ARG_COUNT 4
#define USERNAME_MAX 32
#define CHANNEL_MAX 32
#define SAY_MAX 64
#define MAX_PORT 65535
#define SERVERNAME_MAX 255

typedef struct request req;
typedef struct request_login req_login;
typedef struct request_logout req_logout;
typedef struct request_join req_join;
typedef struct request_leave req_leave;
typedef struct request_say req_say;
typedef struct request_list req_list;
typedef struct request_who req_who;
typedef struct request_keep_alive req_keep_alive;

char sname[SERVERNAME_MAX], uname[USERNAME_MAX], achannel[CHANNEL_MAX];
int port_num = 0;

int parse_args(int argc, char** argv){
    int i, port;

    if(argc<REQ_ARG_COUNT){
        printf("ERR: Missing arguments\nUSAGE: ./client hostname port username\n");
        return 1;
    }

    for(i=0;i<argc;i++){
        switch(i){
            case 1:
                strcpy(sname,argv[i]);
                break;
            case 2:
                port = atoi(argv[i]);
                if(port>MAX_PORT){
                    printf("ERR: Port can't be higher than %d\n",MAX_PORT);
                    return -1;   
                }
                else
                    port_num = port;
                break;
            case 3:
                if(strlen(argv[i])>USERNAME_MAX){
                    printf("ERR: Username can't be more than %d bytes\n",USERNAME_MAX);
                    return -1;   
                }
                else
                    strcpy(uname,argv[i]);
                break;
            default:
                // do nothing
                break;
        }
    }

    return 0; //no errors
}

void write_login(req_login* rl){
    rl->req_type = htonl(REQ_LOGIN);
    strcpy(rl->req_username,uname);
}


int main (int argc, char** argv){
    struct sockaddr_in sa;
    int bytes_sent;
    char rbuffer[BUFF_SIZE];
    ssize_t recsize;
    socklen_t fromlen;

    int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    void* request = malloc(sizeof(req_say)*2);
    
    if(sock == -1){
        printf("Error Creating Socket");
        exit(EXIT_FAILURE);
    }

    if(parse_args(argc,argv)){
        exit(EXIT_FAILURE);
    }

    printf("server: %s\nport: %d\nusername: %s\n",sname,port_num,uname);
    

    memset(&sa, 0, sizeof(sa));                     //zero out socket address
    sa.sin_family = AF_INET;                        //use ipv4
    sa.sin_addr.s_addr = inet_addr(sname);          //set servername
    sa.sin_port = htons(port_num);                  //set port
  
    write_login((req_login*)request);
    bytes_sent = sendto(sock, request, sizeof(req_login), 0, (struct sockaddr*)&sa, sizeof(sa));

    if(bytes_sent < 0){
        printf("Error sending packet: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    else
        printf("Sent\n");

    /*
    recsize = recvfrom(sock, (void *)rbuffer, sizeof(rbuffer), 0, (struct sockaddr*)&sa, &fromlen);

    if(recsize < 0){
        printf("%s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    printf("recsize: %d\n ", (int)recsize);
    sleep(1);
    printf("datagram: %.*s\n",(int)recsize, rbuffer);
    */
    
    close(sock); //close the socket
    return 0;
}
