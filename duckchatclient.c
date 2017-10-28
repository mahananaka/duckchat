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

void request_login(ReqLogin* rl, SockAddrIn* to){
    rl->req_type = REQ_LOGIN;
    strcpy(rl->req_username,uname);

    send_request(to, rl, sizeof(ReqLogin));
}

void request_join(ReqJoin* rj, char *channel, SockAddrIn* to){
    int len = strlen(channel);

    if(len<1)
        return;

    if(len>CHANNEL_MAX)
        printf("ERR: channel name too long.\n");

    int i,n = ch_list.num_channels;
    for(i=0;i<n;i++){
        if(strcmp(ch_list.list[i].cname,channel)==0){
            printf("You are already subscribed to channel \"%s\".\n",channel);
            switch_achannel(channel);
            return;
        }
    }

    rj->req_type = REQ_JOIN;
    strcpy(rj->req_channel,channel);
    send_request(to, rj, sizeof(ReqJoin));

    strcpy(ch_list.list[n].cname,channel);
    ch_list.num_channels++;
    switch_achannel(channel);
}

void request_leave(ReqLeave* rl, char* channel, SockAddrIn* to){
    int len = strlen(channel);

    if(len<1)
        return;

    if(len>CHANNEL_MAX){
        printf("Channel's are max %d characters long.\n",CHANNEL_MAX);
        return;
    }

    int i,n = ch_list.num_channels;
    for(i=0;i<n;i++){
        if(strcmp(ch_list.list[i].cname,channel)==0){
            rl->req_type = REQ_LEAVE;
            strcpy(rl->req_channel,channel);
            send_request(to,rl,sizeof(ReqLeave));
            remove_channel(i);
            return;
        }
    }
    
    printf("You can't leave a channel you're not subscribed to. No request sent to server.\n");
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
    keep_running = 0;
}

void request_say(ReqSay* rs, char* msg, char*channel, SockAddrIn* to){
    int len = strlen(msg);
    if(len>SAY_MAX){
        printf("Err: messages are limited to %d characters.\n",SAY_MAX);
        return;
    }

    if(strlen(achannel)==0){
        printf("You are not in a channel. Join one before speaking.\n");
        return;
    }
    
    rs->req_type = REQ_SAY;
    strcpy(rs->req_channel,channel);
    strcpy(rs->req_text,msg);

    send_request(to,rs,sizeof(ReqSay));
}

void switch_achannel(char* new){
    int i, n=ch_list.num_channels;

    for(i=0;i<n;i++){
        if(strcmp(ch_list.list[i].cname,new)==0){
            strcpy(achannel,new);
            printf("Active channel set to %s.\n",new);
            return;
        }
    }

    printf("Can't switch to a channel you are not subscribed to.\n");
}

void remove_channel(int cid){
    int i, rem_active_channel=0, n=ch_list.num_channels;

    if(strcmp(achannel,ch_list.list[cid].cname)==0){
        rem_active_channel=1;
    }

    for(i=cid;i<n;i++){
        if(i<n-1){
            ch_list.list[i] = ch_list.list[i+1];
        }
    }

    ch_list.num_channels--;

    if(rem_active_channel){
        if(ch_list.num_channels>0){
            strcpy(achannel,ch_list.list[0].cname);
            printf("Active channel set to %s.\n");
        }
        else{
            strcpy(achannel,"");
            printf("You are not in any channels, join one so you can speak.\n");
        }
    }
}

void send_request(SockAddrIn* to, char* msg, int len){
    int ammount_sent = 0;

    while(ammount_sent<len && ammount_sent >= 0){
        ammount_sent += sendto(sock, (msg+ammount_sent), len, 0, to, sizeof(*to));
    }

    if(ammount_sent<0){
        printf("Err: sending request - %s\n", strerror(errno));
    }
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
        switch_achannel(input+len+1);
    }
    else if(strncmp(word,CMD_JOIN,len)==0){
        request_join((ReqJoin*)r,input+len+1,to);
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
        printf("Unrecognized command, no action taken.\n");
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
