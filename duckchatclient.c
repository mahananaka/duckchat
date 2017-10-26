#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "duckchat.h"
#include "duckchatclient.h"

send_request(SockAddrIn*  to, char* msg, int len){
    int ammount_sent=0;

    while(ammount_sent<len){
        ammount_sent += sendto(sock, (msg+ammount_sent), len, 0, to, sizeof(*to));
    }
}

void request_login(ReqLogin* rl, SockAddrIn* to){
    rl->req_type = REQ_LOGIN;
    strcpy(rl->req_username,uname);

    send_request(to, rl, sizeof(ReqLogin));
}

void request_join(ReqJoin* rj, char *channel_name, SockAddrIn* to){
    if(strlen(channel_name)<=CHANNEL_MAX){
        rj->req_type = REQ_JOIN;
        strcpy(rj->req_channel,channel_name);
        send_request(to, rj, sizeof(ReqJoin));
    }
    else{
        printf("ERR: channel name too long.\n");
    }
}

void process_command(Req* r, char *input, SockAddrIn* to){
    
}

void request_say(ReqSay* rs, char* msg, char*channel, SockAddrIn* to){
    if(strlen(msg)>SAY_MAX){
        printf("Err: messages are limited to %d characters.\n",SAY_MAX);
        return;
    }
    rs->req_type = REQ_SAY;
    strcpy(rs->req_channel,channel);
    strcpy(rs->req_text,msg);

    send_request(to,rs,sizeof(ReqSay));
}



