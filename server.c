#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h> /* for close() for socket */ 
#include <stdlib.h>
#include "duckchat.h"

typedef struct request req;
typedef struct request_login req_login;
typedef struct request_logout req_logout;
typedef struct request_join req_join;
typedef struct request_leave req_leave;
typedef struct request_say req_say;
typedef struct request_list req_list;
typedef struct request_who req_who;
typedef struct request_keep_alive req_keep_alive;

int sock;

void init_duckchat(){

}

void handle_request(req* r){
    r->req_type = ntohl(r->req_type);

    switch(r->req_type){
        case REQ_LOGIN:
            handle_login((req_login*)r);
            break;
        case REQ_LOGOUT:
            handle_logout((req_logout*)r);
            break;
        case REQ_JOIN:
            handle_join((req_join*)r);
            break;
        case REQ_LEAVE:
            handle_leave((req_leave*)r);
            break;
        case REQ_SAY:
            handle_say((req_say*)r);
            break;
        case REQ_LIST:
            handle_list((req_list*)r);
            break;
        case REQ_WHO:
            handle_who((req_who*)r);
            break;
        case REQ_KEEP_ALIVE:
            handle_keep_alive((req_keep_alive*)r);
            break;

    }
}

void handle_login(req_login* rl){
    printf("%s\n",((struct request_login*)rl)->req_username);
}

void handle_logout(req_logout* rl){

}

void handle_join(req_join* rj){

}

void handle_leave(req_leave* rl){

}

void handle_say(req_say* rs){

}

void handle_list(req_list* rl){

}

void handle_who(req_who* rw){

}

void handle_keep_alive(req_keep_alive* rka){

}

void cleanup(void){
    if(sock)
        close(sock);
}

int main(void)
{
  int sock;
  struct sockaddr_in sa; 
  char buffer[1024];
  ssize_t recsize;
  socklen_t fromlen;
  req *r;

  r = (req*)malloc(sizeof(req));
  memset(buffer, 0, sizeof(buffer));

  atexit(cleanup);

  memset(&sa, 0, sizeof sa);
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl(INADDR_ANY);
  sa.sin_port = htons(7654);
  fromlen = sizeof sa;

  sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (bind(sock, (struct sockaddr *)&sa, sizeof sa) == -1) {
    perror("error bind failed");
    close(sock);
    exit(EXIT_FAILURE);
  }

  for (;;) {
    recsize = recvfrom(sock, (void*)buffer, sizeof(buffer), 0, (struct sockaddr*)&sa, &fromlen);
    if (recsize < 0) {
      fprintf(stderr, "%s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
    printf("recsize: %d\n ", (int)recsize);
    sleep(1);

    handle_request(buffer);
    
    printf("datagram: %s\n",buffer);
  }

  if(r->req_type ==0)
      return 0;
}
