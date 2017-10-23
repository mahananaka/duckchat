#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h> /* for close() for socket */ 
#include <stdlib.h>
#include "duckchat.h"
#include "duckchatserver.h"

int sock;
ChannelList c_list;
UserList u_list;

void init_duckchat(){
    c_list.num_channels = 0;
    u_list.num_users = 0;
    int max_users = MAX_NO_CHANNELS*MAX_USERS_PER_CHANNEL;

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

void cleanup(void){
    if(sock)
        close(sock);
}

int main(void)
{
  SockAddrIn sa; 
  char buffer[2048];
  ssize_t recsize;
  socklen_t fromlen;

  init_duckchat();

  memset(buffer, 0, sizeof(buffer));

  atexit(cleanup);

  //setup inbound socket
  memset(&sa, 0, sizeof sa);
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl(INADDR_ANY);
  sa.sin_port = htons(7654);
  fromlen = sizeof sa;

  //make socket and check for failure
  sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (bind(sock, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
    perror("error bind failed");
    close(sock);
    exit(EXIT_FAILURE);
  }

  //main program loop
  for (;;) {
    printf("waiting for request...\n");
    recsize = recvfrom(sock, (void*)buffer, sizeof(buffer), 0, (struct sockaddr*)&sa, &fromlen);
    if (recsize < 0) {
      fprintf(stderr, "%s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }

    //majority of work done here
    handle_request(buffer, &sa);
  }

  return 0;
}
