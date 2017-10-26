#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <signal.h>
#include "raw.h"
#include "duckchat.h"
#include "duckchatclient.h"

#define REQ_ARG_COUNT 4
#define DEFAULT_CHANNEL "Common"

/* some global variables */
char input[OUT_BUFFER_SIZE];
char sname[SERVERNAME_MAX], uname[USERNAME_MAX], achannel[CHANNEL_MAX];
int port_num = 0;
int sock = 0;
void* request;

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

void get_input(char* store){
    printf(">");
    fflush(stdout);
    char* c = store;
    while (read(STDIN_FILENO, c, 1) == 1 && *c != '\n') {
        write(STDOUT_FILENO, c, 1);
        c++;
    }
    write(STDOUT_FILENO,c,1);
    c++;
    *c = '\0';
}


void interpret_userinput(char* input, void* request, SockAddrIn* to){
    if(strlen(input)<1)
        return;

    switch(*input){
        case '/':
            process_command(request,input,to);
            break;
        default:
            request_say(request,input,achannel,to);
            break;
    }
}

void run_on_exit(){
        cooked_mode();
        free(request);
}

int main (int argc, char** argv){
    SockAddrIn to;
    char rcv_buff[IN_BUFFER_SIZE];
    int bytes_sent, keep_running=1;
    ssize_t recsize;
    socklen_t fromlen;
    pid_t pid;

    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);    
    if(sock == -1){
        printf("Error Creating Socket");
        exit(EXIT_FAILURE);
    }

    if(parse_args(argc,argv)<0){
        exit(EXIT_FAILURE);
    }

    /* setup the socket address for the desired connection */
    printf("server: %s\nport: %d\nusername: %s\n",sname,port_num,uname);

    memset(&to, 0, sizeof(to));                     //zero out socket address
    to.sin_family = AF_INET;                        //use ipv4
    to.sin_addr.s_addr = inet_addr(sname);          //set servername
    to.sin_port = htons(port_num);                  //set port
    /* finished setup */

    request = malloc(sizeof(ReqSay));     //ReqSay is the largest request we can make.
    if(!request){
        printf("ERR: couldn't malloc enough memory.\n");
        exit(EXIT_FAILURE);
    }

    pid = fork();

    if(pid==0){
        request_login((ReqLogin*)request, &to);
    
        request_join((ReqJoin*)request, DEFAULT_CHANNEL, &to);
        strcpy(achannel,DEFAULT_CHANNEL);
    
        raw_mode();
        while(keep_running>0){
            get_input(input);
            interpret_userinput(input, request, &to);
        }
    }
    else
    {
        while(keep_running>0){
            recsize = recvfrom(sock,(void*)rcv_buff,sizeof(rcv_buff),0,&to,&fromlen);
            if(recsize<0){
                fprintf(stderr,"%s\n", strerror(errno));
                kill(pid,SIGTERM);
                exit(EXIT_FAILURE);
            }

            TxtSay* ts = (TxtSay*)rcv_buff;
            if(ts->txt_type == TXT_SAY){
                printf("[%s][%s]%s",ts->txt_channel,ts->txt_username,ts->txt_text);
            }
        }
    }
    return 0;
}
