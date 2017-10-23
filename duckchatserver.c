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
    if(get_userid(requester)>=0){
        return handle_error("ERR: Already logged in.",requester);
    }

    if(u_list.num_users >= MAX_NO_CHANNELS*MAX_USERS_PER_CHANNEL){
        return handle_error("ERR: Server is at max user capacity.",requester);
    }

    int uid = u_list.num_users;
    strcpy(u_list.users[uid].uname,rl->req_username);
    memcpy(&(u_list.users[uid].addr), requester, sizeof(SockAddrIn));
    u_list.num_users++;

    printf("Login by %s\n",u_list.users[uid].uname);
}

void handle_logout(ReqLogout* rl, SockAddrIn* requester){
    int uid = get_userid(requester);

    if(uid<0){
        //user not logged in
        handle_error("ERR: No record of your login.",requester);
        return;
    }

    printf("Logout by %s\n",u_list.users[uid].uname);

    //remove user from each of the channels.
    int i, num = u_list.users[uid].num_subs;
    for(i=0;i<num;i++){
        rem_user_from_channel(uid, u_list.users[uid].subbed_channels[i]);
    }

    //remove user from u_list
    num = u_list.num_users;
    for(i=uid;i<num;i++){
        if(i<num-1){
            u_list.users[uid] = u_list.users[uid+1];
        }
    }
    u_list.num_users--;
}

void handle_join(ReqJoin* rj, SockAddrIn* requester){
    int uid = get_userid(requester);
    int cid = get_cid(rj->req_channel);

    if(uid<0){
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
        c_list.num_channels++;
    }
    
    //add them to channel
    Channel *c = &(c_list.list[cid]);
    int subid = c->num_subers;
    c->subers[subid] = uid;
    c->num_subers++;

    //add this channel to there subscription list
    int i = u_list.users[uid].num_subs;
    u_list.users[uid].subbed_channels[i] = cid;
    u_list.users[uid].num_subs++;

    printf("%s joins %s\n",u_list.users[uid].uname, c_list.list[cid].cname);
}

void handle_leave(ReqLeave* rl, SockAddrIn* requester){
    int uid = get_userid(requester);
    int cid = get_cid(rl->req_channel);

    if(uid<0){
        return handle_error("ERR: No record of your login.", requester);
    }

    if(cid<0){
        return handle_error("ERR: No such channel.", requester);
    }

    //remove the user from the channel
    rem_user_from_channel(uid,cid);

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
    printf("user %s left %s\n",u_list.users[uid].uname,rl->req_channel);
}

void handle_say(ReqSay* rs, SockAddrIn* requester){
    int uid = get_userid(requester);
    int cid = get_cid(rs->req_channel);

    if(uid<0){
        return handle_error("ERR: No record of your login.", requester);
    }

    if(cid<0){
        return handle_error("ERR: No such channel.", requester);
    }
    
    if(is_user_in_channel(uid,cid) < 0){
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
        to = &(u_list.users[i].addr);
        sendreply(to,ts,len);
    }

    free(ts);
}

void handle_list(ReqList* rl, SockAddrIn* to){
    int num_channels = c_list.num_channels;

    int len = num_channels*CHANNEL_MAX+sizeof(TxtList);
    TxtList* tl = (TxtList*)malloc(len);
    tl->txt_type = TXT_LIST;
    tl->txt_nchannels = num_channels;

    int i;
    for(i=0;i<num_channels;i++){
        strcpy(tl->txt_channels[i].ch_channel,c_list.list[i].cname);
    }

    sendreply(to,tl,len);
}

void handle_who(ReqWho* rw, SockAddrIn* to){
    if(strlen(rw->req_channel)>CHANNEL_MAX){
        return handle_error("ERR: Channel name exceeds max length.", to);
    }

    int cid = get_cid(rw->req_channel);

    if(cid < 0){
        return handle_error("ERR: Channel does not exist.", to);
    }

    Channel c = c_list.list[cid];
    int num_subscribers = c.num_subers;
    
    int len = num_subscribers*USERNAME_MAX+sizeof(TxtWho);
    TxtWho* tw = (TxtWho*)malloc(len);
    tw->txt_type = TXT_WHO;
    tw->txt_nusernames = num_subscribers;
    strcpy(tw->txt_channel,c.cname);

    int i;
    for(i=0;i<num_subscribers;i++){
        strcpy(tw->txt_users[i].us_username,u_list.users[c.subers[i]].uname);
    }

    sendreply(to,tw,len);
    free(tw);
}

void handle_error(char* msg, SockAddrIn* to){
    int len = sizeof(TxtErr);
    TxtErr* te = (TxtErr*)malloc(len);
    
    te->txt_type = TXT_ERROR;
    strcpy(te->txt_error,msg);

    sendreply(to,te,len);
    free(te);
}

void handle_keep_alive(ReqKeepAlive* rka){
    printf("user doing keep alive\n");
}

int sendreply(SockAddrIn* to, char* msg, int len){
    int ammount_sent = 0;

    while(ammount_sent<len){
        ammount_sent += sendto(sock, (msg+ammount_sent), len, 0, to, sizeof(*to));
    }
}

int is_user_in_channel(int uid, int cid){
    int i, num_subers = c_list.list[cid].num_subers;

    for(i=0;i<num_subers;i++){
        if(c_list.list[cid].subers[i] == uid)
            return i;
    }

    return -1;
}

int rem_user_from_channel(int uid, int cid){
    Channel *c = &(c_list.list[cid]);
    int found = 0;
    int i, num = c->num_subers;

    //go through the list of subscribers, once found just shift everyone beyond
    //this point to the left once.
    for(i=0;i<num;i++){
        if(c->subers[i] == uid){
            found = 1;
        }

        if(found && (i<num-1)){
            c->subers[i] = c->subers[i+1];
        }
    }

    //lower subscriber counter
    c->num_subers--;

    if(c->num_subers == 0){
        //have to remove channel
        num = c_list.num_channels;
        for(i=cid;i<num;i++){
            if(i<num-1){
                c_list.list[i] = c_list.list[i+1];
            }
        }
        c_list.num_channels--;
    }
}

int get_userid(SockAddrIn *who){
    int i, max_users = MAX_NO_CHANNELS*MAX_USERS_PER_CHANNEL;
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
