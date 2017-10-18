#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define BUFF_SIZE 2048
#define SOCKET_NUM 7654
#define REQ_ARG_COUNT 4
#define MAX_USERNAME 31
#define MAX_CHANNEL 31
#define MAX_PORT 65535
#define MAX_SERVERNAME 255

char sname[MAX_SERVERNAME+1], uname[MAX_USERNAME+1], achannel[MAX_CHANNEL+1];
int port_num = 0;

int parse_args(int argc, char** argv){
    int i, port;
    char *arg = NULL;

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
                if(strlen(argv[i])>MAX_USERNAME){
                    printf("ERR: Username can't be more than %d bytes\n",MAX_USERNAME);
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

int main (int argc, char** argv){
    struct sockaddr_in sa;
    int sock, bytes_sent;
    char buffer[BUFF_SIZE];


    if(parse_args(argc,argv)){
        exit(EXIT_FAILURE);
    }

    printf("server: %s\nport: %d\nusername: %s\n",sname,port_num,uname);
    
    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sock == -1){
        printf("Error Creating Socket");
        exit(EXIT_FAILURE);
    }

    memset(&sa, 0, sizeof(sa));                     //zero out socket address
    sa.sin_family = AF_INET;                        //use ipv4
    sa.sin_addr.s_addr = inet_addr(sname);          //set servername
    sa.sin_port = htons(SOCKET_NUM);                //set port

    strcpy(buffer, "hello world!");
    
    bytes_sent = sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr*)&sa, sizeof(sa));

    if(bytes_sent < 0){
        printf("Error sending packet: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    else
        printf("Done\n");

    close(sock); //close the socket
    return 0;
}
