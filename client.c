#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <sys/select.h>
#include "raw.h"
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
        printf("ERR: Missing arguments\nUSAGE: ./%s hostname port username\n", argv[0]);
        return -1;
    }

    for(i=1;i<argc;i++){
        switch(i){
            case 1:
                //arg[1] is servername
                strcpy(sname,argv[i]);
                break;
            case 2:
                //arg[2] is port
                port = atoi(argv[i]);
                if(port>MAX_PORT || port<1){
                    printf("ERR: %d is an invalid port number.\n",port);
                    return -1;   
                }
                else
                    port_num = port;
                break;
            case 3:
                //arg[3] is username
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
    /* capture each keystroke so we can store it in case
     * data from server comes in while typing input */
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

void init_duckchat_client(SockAddrIn *to, struct timeval *tv){
    ch_list.num_channels = 0;

    //setup socket for IPv4 with UDP
    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);    
    if(sock == -1){
        printf("Error Creating Socket");
        exit(EXIT_FAILURE);
    }

    struct hostent *h;
    if((h=gethostbyname(sname)) == NULL){
        herror("Can't resolve hostname.\n");
        exit(EXIT_FAILURE);
    }

    //initialize the addressing socket
    memset(to, 0, sizeof(*to));                         //zero out socket address
    to->sin_family = AF_INET;                           //use ipv4
    to->sin_addr = *((struct in_addr *)h->h_addr);      //have to cast to pointer and de-ref.
    to->sin_port = htons(port_num);                     //set port

    /* initialize request, all requests sent to server will
     * use this space to hold the data that is sent. Used
     * sizeof(ReqSay) because it is the largest.*/
    request = malloc(sizeof(ReqSay));
    if(!request){
        printf("ERR: couldn't malloc enough memory.\n");
        exit(EXIT_FAILURE);
    }
    memset(request,0,sizeof(ReqSay));


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
    int n, rv, bytes_sent=0, offset=0;
    ssize_t recsize;
    socklen_t fromlen;
    fd_set readfds;
    struct timeval tv;

    //checks correct usage of client and sets values
    if(parse_args(argc,argv)<0){
        //incorrect usage, exit
        exit(EXIT_FAILURE);
    }

    atexit(run_on_exit);

    //this is all of the initializations, including the socket,
    //timeval for select(), and malloc-ing space.
    init_duckchat_client(&to,&tv);
    n = sock + 1; //n is for select()
    
    fromlen = sizeof(to);
        
    //protocal is to send login, then join channel Common.
    request_login((ReqLogin*)request, &to);
    printf("Connected to %s:%d with username: %s\n",sname,port_num,uname);
    printf("Joined channel %s.\n", DEFAULT_CHANNEL);
    request_join((ReqJoin*)request, DEFAULT_CHANNEL, &to);
    
    //set terminal to raw mode;
    raw_mode();

    //print the terminal input
    printf(">");
    fflush(stdout);
    
    while(keep_running){
        //must setup readfds each loop to listen to STDIN_FILENO and sock
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sock, &readfds);

        rv = select(n, &readfds, NULL, NULL, &tv);
        if(rv == -1){
            //error
            fprintf(stdout,"select() err: %s\n", strerror(errno));
        }
        else if (rv ==0){
            //timed out nothing to do.
        }
        else{
            //check if data from sock
            if(FD_ISSET(sock, &readfds)){
                recsize = recvfrom(sock,(void*)rcv_buff,sizeof(rcv_buff),0,&to,&fromlen);
                if(recsize<0){
                    fprintf(stderr,"%s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }

                //clear the current terminal line (its saved elsewhere) 
                int i, len = strlen(out_buff);
                for(i=len;i>=0;i--)
                    printf("\b");


                display_server_message((Txt*)rcv_buff);

                //reprint partial input the user had already typed
                //this is only a perceptional issue for user, the input was still captured
                printf(">%s",out_buff);
                fflush(stdout);
            }

            //check if data from STDIN_FILENO
            if(FD_ISSET(STDIN_FILENO, &readfds)){
                if(get_input(out_buff+offset)){
                    //user finished cmd so process it and flush stdin
                    interpret_userinput(out_buff, request, &to);
                    fflush(stdin);
                    
                    //redisplay prompt
                    printf(">");
                    fflush(stdout);
                    
                    //reset input gatherers
                    memset(out_buff,'\0',OUT_BUFFER_SIZE);
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
