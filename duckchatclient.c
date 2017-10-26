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

void request_join(ReqJoin* rj, char *channel, SockAddrIn* to){
    int len = strlen(channel);

    if(len<1)
        return;

    if(len<=CHANNEL_MAX){
        rj->req_type = REQ_JOIN;
        strcpy(rj->req_channel,channel);
        send_request(to, rj, sizeof(ReqJoin));

        
    }
    else{
        printf("ERR: channel name too long.\n");
    }
}

void request_leave(ReqLeave* rl, char* channel, SockAddrIn* to){
    int len = strlen(channel);

    if(len<1)
        return;

    if(len>CHANNEL_MAX){

    }

    rl->req_type = REQ_LEAVE;
    strcpy(rl->req_channel,channel);
    send_request(to,rl,sizeof(ReqLeave));
}

void request_list(ReqList* rl, SockAddrIn* to){
    rl->req_type = REQ_LIST;
    send_request(to,rl,sizeof(ReqList));
}

void request_who(ReqWho* rw, char* channel, SockAddrIn* to){
    int len = strlen(channel);

    if(len<1)
        return;

    if(len>CHANNEL_MAX){

    }
    
    rw->req_type = REQ_WHO;
    strcpy(rw->req_channel,channel);
    send_request(to,rw,sizeof(ReqWho));

}

void request_logout(ReqLogout* rl, SockAddrIn* to){
    rl->req_type = REQ_LOGOUT;
    send_request(to,rl,sizeof(ReqLogout));
}

void request_say(ReqSay* rs, char* msg, char*channel, SockAddrIn* to){
    int len = strlen(msg);
    if(len>SAY_MAX){
        printf("Err: messages are limited to %d characters.\n",SAY_MAX);
        return;
    }
    
    rs->req_type = REQ_SAY;
    strcpy(rs->req_channel,channel);
    strcpy(rs->req_text,msg);

    send_request(to,rs,sizeof(ReqSay));
}

void switch_achannel(char* new){
    strcpy(achannel,new); //change active channel
}

void chop_off_newline(char* str){
    int len = strlen(str);
    if(*(str+len-1) == '\n')
        *(str+len-1) = '\0'; //chopping off the newline
}

int get_nth_word(char* word, const char* input, int n){
    int used;
    int total = 0;
    while (n>0 && sscanf(input, "%s%n", word, &used)>0){
        input += used;
        total += used;
        n--;
    }

    return total;
}

void process_command(Req* r, char *input, SockAddrIn* to){
    char word[32];
    int len = get_nth_word(word,input,1);

    if(strncmp(word,CMD_SWITCH,len)==0){
        
    }
    else if(strncmp(word,CMD_JOIN,len)==0){
        request_join((ReqJoin*)r,input+len+1,to);
        switch_achannel(input+len+1); //change active channel
    }
    else if(strncmp(word,CMD_LEAVE,len)==0){
        request_leave((ReqLeave*)r,input+len+1,to);
    }
    else if(strncmp(word,CMD_LIST,len)==0){
        request_list((ReqList*)r,to);
    }
    else if(strncmp(word,CMD_WHO,len)==0){
        request_who((ReqWho*)r,input+len+1,to);
    }
    else if(strncmp(word,CMD_LOGOUT,len)==0){
        request_logout((ReqLogout*)r,to);
    }
    else{
        //invalid command do nothing
    }
}

void display_server_message(Txt* t){
    int i,n;

    //now handle the response from the server
    switch (t->txt_type){
        case TXT_SAY:
            ;
            TxtSay* ts = (TxtSay*)t;
            printf("[%s][%s]: %s\n",ts->txt_channel,ts->txt_username,ts->txt_text);
            break;

        case TXT_LIST:
            ;
            TxtList* tl = (TxtList*)t;
            n=((TxtList*)t)->txt_nchannels;
            printf("Existing Channels:\n");
            for(i=0;i<n;i++){
                printf("    %s\n",tl->txt_channels[i].ch_channel);
            }
            break;

        case TXT_WHO:
            ;
            TxtWho* tw = (TxtWho*)t;
            printf("Users in channel %s\n",tw->txt_channel);
            n=tw->txt_nusernames;
            for(i=0;i<n;i++){
                printf("    %s\n",tw->txt_users[i].us_username);
            }
            break;

        case TXT_ERROR:
            ;
            TxtErr* te = (TxtErr*)t;
            printf("%s\n",te->txt_error);
            break;
            
        default:
            break;
    }
}
