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
#include <signal.h>
#include <sys/poll.h>
#include "raw.h"
#include "duckchat.h"
#include "duckchatclient.h"

#define REQ_ARG_COUNT 4
#define DEFAULT_CHANNEL "Common"

/* some global variables */
//char in_buff[OUT_BUFFER_SIZE];
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
    char* c = store;
    while (read(STDIN_FILENO, c, 1) == 1 && *c != '\n') {
        write(STDOUT_FILENO, c, 1);
        c++;
        *c = '\0';
    }
    write(STDOUT_FILENO,c,1);
    c++;
    *c = '\0';
}


void interpret_userinput(char* input, void* request, SockAddrIn* to){
    if((strlen(input))<1) //subtract one cause we capture the newline as well
        return;

    chop_off_newline(input);

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

    char * out_buff = mmap(NULL, OUT_BUFFER_SIZE, PROT_READ | PROT_WRITE, 
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    
    request_login((ReqLogin*)request, &to);
    
    request_join((ReqJoin*)request, DEFAULT_CHANNEL, &to);
    strcpy(achannel,DEFAULT_CHANNEL);
    
    raw_mode();

    printf(">");
    fflush(stdout);

    pid = fork();

    if(pid==0){
        //child process gets user input from the terminal
        while(keep_running>0){
            get_input(out_buff);
            interpret_userinput(out_buff, request, &to);
            *out_buff = '\0';
        }
    }
    else
    {
        //parent process polls the socket looking for available data
        struct pollfd pollsock;
        pollsock.fd = sock;
        pollsock.events = POLLIN;
        int rv;

        while(keep_running>0){
            rv = poll(&pollsock,1,500); //poll set to giveup after 1/2 a second
            if(rv==-1){
                //error
            } else if(rv ==0){
                // no data available
            } else {
                // something waiting to be processed
                recsize = recvfrom(sock,(void*)rcv_buff,sizeof(rcv_buff),0,&to,&fromlen);
                if(recsize<0){
                    fprintf(stderr,"%s\n", strerror(errno));
                    kill(pid,SIGTERM); //terminate child if problems occur
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
        }
        munmap(out_buff, OUT_BUFFER_SIZE);
    }
    return 0;
}
