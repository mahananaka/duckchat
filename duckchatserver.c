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

/* NOTE: u_list, c_list, and sock are all declared externally in
 * duckchatserver.h and defined by a seperate source file. In this
 * case it is defined in server.c */

void handle_login(ReqLogin* rl, SockAddrIn* requester){
    if(get_user_index(requester)>=0){
        return handle_error("ERR: Already logged in.",requester);
    }

    if(u_list.num_users >= MAX_NO_CHANNELS*MAX_USERS_PER_CHANNEL){
        return handle_error("ERR: Server is at max user capacity.",requester);
    }

    int i = u_list.num_users;
    strcpy(u_list.users[i].uname,rl->req_username);
    memcpy(&(u_list.users[i].addr), requester, sizeof(SockAddrIn));
    u_list.num_users++;

    printf("request: login by %s\n",u_list.users[i].uname);
}

void handle_logout(ReqLogout* rl, SockAddrIn* requester){
    int ind = get_user_index(requester);

    if(ind<0){
        //user not logged in
        handle_error("ERR: No record of your login.",requester);
        return;
    }

    printf("request: logout by %s\n",u_list.users[ind].uname);

    //remove user from each of the channels.
    int i, num = u_list.users[ind].num_subs;
    for(i=0;i<num;i++){
        rem_user_from_channel(requester, u_list.users[ind].subbed_channels[i]);
    }

    //remove user from u_list
    num = u_list.num_users;
    for(i=ind;i<num;i++){
        if(i<num-1){
            u_list.users[i] = u_list.users[i+1];
        }
    }
    u_list.num_users--;
}

void handle_join(ReqJoin* rj, SockAddrIn* requester){
    int ind = get_user_index(requester);
    int cid = get_cid(rj->req_channel);

    if(ind<0){
        return handle_error("ERR: No record of your login.", requester);
    }

    if(strlen(rj->req_channel)>CHANNEL_MAX){
        return handle_error("ERR: Channel name exceeds max length.", requester);
    }

    if(cid<0){
        //channel doesn't exist yet, so need to make it.
        cid = c_list.num_channels;
        if(cid>=MAX_NO_CHANNELS){
            return handle_error("ERR: At max channel capacity.", requester);
        }
        
        strcpy(c_list.list[cid].cname,rj->req_channel);
        c_list.list[cid].num_subers = 0;
        c_list.num_channels++;
    }

    if(!is_double_join(ind,cid)){    
        //add them to channel
        Channel *c = &(c_list.list[cid]);
        int subid = c->num_subers;
        memcpy(&(c->subers[subid]),requester,sizeof(SockAddrIn));
        c->num_subers++;

        //add this channel to there subscription list
        int i = u_list.users[ind].num_subs;
        u_list.users[ind].subbed_channels[i] = cid;
        u_list.users[ind].num_subs++;
    }

    printf("request: %s joins %s\n",u_list.users[ind].uname, c_list.list[cid].cname);
}

void handle_leave(ReqLeave* rl, SockAddrIn* requester){
    int uid = get_user_index(requester);
    int cid = get_cid(rl->req_channel);

    if(uid<0){
        return handle_error("ERR: No record of your login.", requester);
    }

    if(cid<0){
        return handle_error("ERR: No such channel.", requester);
    }

    //remove the user from the channel
    rem_user_from_channel(requester,cid);

    //also remove channel from the user
    User *u = &(u_list.users[uid]);
    int i, found = 0,  num = u->num_subs;
    for(i=0;i<num;i++){
        if(u->subbed_channels[i] == cid){
            found = 1;
        }

        if(found && (i<num-1)){
            u->subbed_channels[i] = u->subbed_channels[i+1];
        }
    }

    //now lower subscribed count
    u->num_subs--;
    printf("request: %s left %s\n",u_list.users[uid].uname,rl->req_channel);
}

void handle_say(ReqSay* rs, SockAddrIn* requester){
    int uid = get_user_index(requester);
    int cid = get_cid(rs->req_channel);

    if(uid<0){
        return handle_error("ERR: No record of your login.", requester);
    }

    if(cid<0){
        return handle_error("ERR: No such channel.", requester);
    }
    
    if(is_user_in_channel(requester,cid) < 0){
        return handle_error("ERR: You must be in a channel to talk there.", requester);
    }

    int len = sizeof(TxtSay);
    TxtSay* ts = (TxtSay*)malloc(len);
    ts->txt_type = TXT_SAY;
    strcpy(ts->txt_channel, rs->req_channel);
    strcpy(ts->txt_username, u_list.users[uid].uname);
    strcpy(ts->txt_text, rs->req_text);

    SockAddrIn* to;
    int i, n = c_list.list[cid].num_subers;
    for(i=0;i<n;i++){
        to = &(c_list.list[cid].subers[i]);
        sendreply(to,ts,len);
    }

    printf("request: %s speaks in %s\n", ts->txt_username, ts->txt_channel);

    free(ts);
}

void handle_list(ReqList* rl, SockAddrIn* to){
    int num_channels = c_list.num_channels;
    int uid = get_user_index(to);

    if(uid < 0){
        return handle_error("Err: you must login before other requests.", to);
    }

    int len = num_channels*CHANNEL_MAX+sizeof(TxtList);
    TxtList* tl = (TxtList*)malloc(len);
    tl->txt_type = TXT_LIST;
    tl->txt_nchannels = num_channels;

    int i;
    for(i=0;i<num_channels;i++){
        strcpy(tl->txt_channels[i].ch_channel,c_list.list[i].cname);
    }

    sendreply(to,tl,len);
    printf("request: %s lists channels.\n",u_list.users[uid].uname);
    free(tl);
}

void handle_who(ReqWho* rw, SockAddrIn* to){
    int ind = get_user_index(to);

    if(ind < 0){
        return handle_error("Err: you must login before other requests.", to);
    }

    if(strlen(rw->req_channel)>CHANNEL_MAX){
        return handle_error("ERR: Channel name exceeds max length.", to);
    }

    int cid = get_cid(rw->req_channel);

    if(cid < 0){
        return handle_error("ERR: Channel does not exist.", to);
    }

    Channel c = c_list.list[cid];
    int num_subscribers = c.num_subers,num_users = u_list.num_users;
    
    int len = num_subscribers*USERNAME_MAX+sizeof(TxtWho);
    TxtWho* tw = (TxtWho*)malloc(len);
    tw->txt_type = TXT_WHO;
    tw->txt_nusernames = num_subscribers;
    strcpy(tw->txt_channel,c.cname);

    int i,j;
    for(i=0;i<num_subscribers;i++){
        for(j=0;j<num_users;j++){
            if(memcmp(&c.subers[i],&u_list.users[j].addr,sizeof(SockAddrIn))==0){
                strcpy(tw->txt_users[i].us_username,u_list.users[j].uname);
                break;
            }
        }
    }

    sendreply(to,tw,len);
    printf("request: %s lists users in %s.\n",u_list.users[ind].uname, tw->txt_channel);
    free(tw);
}

void handle_error(char* msg, SockAddrIn* to){
    int len = sizeof(TxtErr);
    TxtErr* te = (TxtErr*)malloc(len);
    
    te->txt_type = TXT_ERROR;
    strcpy(te->txt_error,msg);

    sendreply(to,te,len);
    printf("request: error reply \"%s\"\n",msg);
    free(te);
}

void handle_keep_alive(ReqKeepAlive* rka){
    printf("keep_alive not implemented.\n");
}

int is_double_join(int uid, int cid){
    Channel *c = &(c_list.list[cid]);
    int i, n = c->num_subers;


    for(i=0;i<n;i++){
        if(memcmp(&c->subers[i],&u_list.users[uid].addr,sizeof(SockAddrIn))==0)
            return 1;
    }

    return 0;
}

int sendreply(SockAddrIn* to, char* msg, int len){
    int ammount_sent = 0;

    while(ammount_sent<len && ammount_sent >= 0){
        ammount_sent += sendto(sock, (msg+ammount_sent), len, 0, to, sizeof(*to));
    }

    if(ammount_sent<0){
        printf("Err: %s\n", strerror(errno));
    }
}

int is_user_in_channel(SockAddrIn* requester, int cid){
    int i, num_subers = c_list.list[cid].num_subers;

    for(i=0;i<num_subers;i++){
        if(memcmp(&c_list.list[cid].subers[i],requester,sizeof(SockAddrIn))==0)
            return i;
    }

    return -1;
}

int rem_user_from_channel(SockAddrIn* requester, int cid){
    Channel *c = &(c_list.list[cid]);
    int found = 0;
    int i, num = c->num_subers;

    //go through the list of subscribers, once found just shift everyone beyond
    //this point to the left once.
    for(i=0;i<num;i++){
        if(memcmp(&c->subers[i],requester,sizeof(SockAddrIn))==0){
            found = 1;
        }

        if(found && (i<num-1)){
            memcpy(&c->subers[i],&c->subers[i+1],sizeof(SockAddrIn));
        }
    }

    //lower subscriber counter
    c->num_subers--;

    if(c->num_subers == 0){
        //have to remove channel
        num = c_list.num_channels;
        for(i=cid;i<num;i++){
            if(i<num-1){
                replace_channel(i,i+1);
            }
        }
        c_list.num_channels--;
    }
}

int get_user_index(SockAddrIn *who){
    int i, max_users = u_list.num_users;
    in_port_t portkey = who->sin_port;
    uint32_t addrkey = who->sin_addr.s_addr;
    
    //looking for a matching port and address
    for(i=0;i<max_users;i++){
        if(u_list.users[i].addr.sin_addr.s_addr == addrkey && u_list.users[i].addr.sin_port == portkey)
            return i; //found returning the index (which is their userid)
    }

    //not found
    return -1;
}

int get_cid(char *name){
    int i;

    //looking for the channel
    for(i=0;i<c_list.num_channels;i++)
        if(strcmp(name,c_list.list[i].cname) == 0)
            return i; //found return the index

    //not found
    return -1;
}

void replace_channel(int what, int with){
    Channel *l, *r;
    l = &(c_list.list[what]);
    r = &(c_list.list[with]);

    strcpy(l->cname,r->cname);
    l->num_subers = r->num_subers;

    int i, len = r->num_subers;
    for(i=0;i<len;i++){
        memcpy(&(l->subers[i]),&(r->subers[i]),sizeof(SockAddrIn));
    }
}
