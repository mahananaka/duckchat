#include <sys/socket.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <sys/select.h>
#include "raw.h"
#include "duckchat.h"
#include "duckchatclient.h"

#define REQ_ARG_COUNT 4
#define DEFAULT_CHANNEL "Common"
#define TIMEOUT_SECS 5

/* some global variables */
char sname[SERVERNAME_MAX], uname[USERNAME_MAX], achannel[CHANNEL_MAX];
int port_num = 0, sock = 0, keep_running =1;
void* request;
ChannelList ch_list;

int parse_args(int argc, char** argv){
    int i, port;

    if(argc<REQ_ARG_COUNT){
        printf("ERR: Missing arguments\nUSAGE: ./client hostname port username\n");
        return -1;
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
                    printf("ERR: Username can't be more than %d characters\n",USERNAME_MAX);
                    return -1;   
                }
                else
                    strcpy(uname,argv[i]);
                break;
            default:
                ;   // do nothing
                break;
        }
    }

    return 0; //no errors
}

int get_input(char* store){
    char* c = store;
    read(STDIN_FILENO,c,1);
    write(STDOUT_FILENO,c,1);
    if(*c == '\n'){
        *c = '\0';
        return 1;
    }
    c++;
    *c = '\0';
    return 0;
}


void interpret_userinput(char* input, void* request, SockAddrIn* to){
    if((strlen(input))<1)
        return;

    //if first char is a '/' then a command should be processed
    switch(*input){
        case '/':
            process_command(request,input,to);
            break;
        default:
            request_say(request,input,achannel,to);
            break;
    }
}

void init_duckchat_client(SockAddrIn *to, struct timeval *tv, int* offset){
    ch_list.num_channels = 0;
    *offset = 0;

    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);    
    if(sock == -1){
        printf("Error Creating Socket");
        exit(EXIT_FAILURE);
    }
    printf("sock: %d",sock);

    memset(to, 0, sizeof(*to));                   //zero out socket address
    to->sin_family = AF_INET;                   //use ipv4
    to->sin_addr.s_addr = inet_addr(sname);     //set servername
    to->sin_port = htons(port_num);             //set port

    request = malloc(sizeof(ReqSay));           //ReqSay is the largest request we can make.
    if(!request){
        printf("ERR: couldn't malloc enough memory.\n");
        exit(EXIT_FAILURE);
    }

    tv->tv_sec = TIMEOUT_SECS;
    tv->tv_usec = 0;

    keep_running = 1;
}

void run_on_exit(){
        cooked_mode();
        free(request);
}

int main (int argc, char** argv){
    SockAddrIn to;
    char rcv_buff[IN_BUFFER_SIZE], out_buff[OUT_BUFFER_SIZE];
    int n, rv, bytes_sent, offset;
    ssize_t recsize;
    socklen_t fromlen;
    fd_set readfds;
    struct timeval tv;

    if(parse_args(argc,argv)<0){
        exit(EXIT_FAILURE);
    }

    init_duckchat_client(&to,&tv,&offset);
    n = sock + 1;
    fromlen = sizeof(to);
        
    request_login((ReqLogin*)request, &to);
    request_join((ReqJoin*)request, DEFAULT_CHANNEL, &to);
    
    /* setup the socket address for the desired connection */
    printf("Connected to %s:%d with username: %s\n",sname,port_num,uname);
    printf("Joined channel %s.\n",DEFAULT_CHANNEL);
    
    raw_mode();

    printf(">");
    fflush(stdout);
    
    while(keep_running){
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sock, &readfds);

        rv = select(n, &readfds, NULL, NULL, &tv);
        if(rv == -1){
            //error
        }
        else if (rv ==0){
            //timed out
        }
        else{
            if(FD_ISSET(sock, &readfds)){
                recsize = recvfrom(sock,(void*)rcv_buff,sizeof(rcv_buff),0,&to,&fromlen);
                if(recsize<0){
                    fprintf(stderr,"%s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }

                //clear the terminal prompt, what is there is stored in out_buff
                int i, len = strlen(out_buff);
                for(i=len;i>=0;i--)
                    printf("\b");

                display_server_message((Txt*)rcv_buff);

                //reprint what the user had already typed
                printf(">%s",out_buff);
                fflush(stdout);
            }

            if(FD_ISSET(STDIN_FILENO, &readfds)){
                if(get_input(out_buff+offset)){
                    interpret_userinput(out_buff, request, &to);
                    fflush(stdin);
                    printf(">");
                    fflush(stdout);
                    *out_buff = '\0';
                    offset=0;
                }else{
                    offset++;
                }
            }
        }
    }

    printf("Exiting DuckChat client.\n");
    
    return 0;
}
