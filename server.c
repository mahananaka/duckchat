#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h> /* for close() for socket */ 
#include <stdlib.h>
#include "duckchat.h"

#define MAX_NO_CHANNELS 64
#define MAX_USERS_PER_CHANNEL 64

typedef struct request Req;
typedef struct request_login ReqLogin;
typedef struct request_logout ReqLogout;
typedef struct request_join ReqJoin;
typedef struct request_leave ReqLeave;
typedef struct request_say ReqSay;
typedef struct request_list ReqList;
typedef struct request_who ReqWho;
typedef struct request_keep_alive ReqKeepAlive;
typedef struct text Txt;
typedef struct text_say TxtSay;
typedef struct channel_info ChInfo;
typedef struct text_list TxtList;
typedef struct user_info UserInfo;
typedef struct text_who TxtWho;
typedef struct text_error TxtErr;
typedef struct sockaddr_in SockAddrIn;

typedef struct _user {
    char uname[USERNAME_MAX];
    SockAddrIn addr;
    int num_subs;
    int subbed_channels[MAX_NO_CHANNELS];
} User;

typedef struct _channel {
    char cname[CHANNEL_MAX];
    int num_subers;
    unsigned int subers[MAX_USERS_PER_CHANNEL];
} Channel;

typedef struct _user_list {
    int num_users;
    User users[MAX_NO_CHANNELS*MAX_USERS_PER_CHANNEL];
} UserList;

typedef struct _channel_list{
    int num_channels;
    Channel list[MAX_NO_CHANNELS];
} ChannelList;


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

void handle_login(ReqLogin* rl,SockAddrIn* requester){
    printf("New user requesting name: %s\n",rl->req_username);

    int i = u_list.num_users;
    strcpy(u_list.users[i].uname,rl->req_username);
    memcpy(&(u_list.users[i].addr), requester, sizeof(SockAddrIn));
    u_list.num_users++;
}

void handle_logout(ReqLogout* rl,SockAddrIn* requester){
    int uid = get_userid(requester);

    if(uid<0){
        //error
    }

    printf("User %s",u_list.users[uid].uname);

    //remove user from each of the channels.
    int i, num = u_list.users[uid].num_subs;
    for(i=0;i<num;i++){
        rem_user_from_channel(uid, u_list.users[uid].subbed_channels[i]);
    }
    //removed from all channels

    //remove user from u_list
    num = u_list.num_users;
    int found = 0;
    for(i=uid;i<num;i++){
        if(i<num-1){
            u_list.users[uid] = u_list.users[uid+1];
        }
    }
    u_list.num_users--;
    //gone from u_list

    printf(" logs out\n");
}

void handle_join(ReqJoin* rj, SockAddrIn* requester){
    int uid = get_userid(requester);
    int cid = get_cid(rj->req_channel);

    if(uid<0){
        //err
    }

    if(cid<0){
        //channel doesn't exist yet, so need to make it.
        cid = c_list.num_channels;
        if(cid>=MAX_NO_CHANNELS){
            //err can't make at max channels
            return 0;
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

    printf("user %s left %s\n",u_list.users[uid].uname,rl->req_channel);

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
}

void handle_say(ReqSay* rs, int uid, TxtSay** ts, int* len){
    *len = sizeof(TxtSay);
    *ts = (TxtSay*)malloc(*len);
    (*ts)->txt_type = TXT_SAY;
    strcpy((*ts)->txt_channel, rs->req_channel);
    strcpy((*ts)->txt_username, u_list.users[uid].uname);
    strcpy((*ts)->txt_text, rs->req_text);
}

void handle_list(ReqList* rl, TxtList** tl, int* len){
    int num_channels = c_list.num_channels;

    *len = num_channels*CHANNEL_MAX+sizeof(TxtList);
    *tl = (TxtList*)malloc(*len);
    (*tl)->txt_type = TXT_LIST;
    (*tl)->txt_nchannels = num_channels;

    int i;
    for(i=0;i<num_channels;i++){
        strcpy((*tl)->txt_channels[i].ch_channel,c_list.list[i].cname);
    }
}

void handle_who(ReqWho* rw, TxtWho** tw, int* len){
    printf("user wants to know who is in channel %s\n", rw->req_channel);
    int cid = get_cid(rw->req_channel);
    Channel c = c_list.list[cid];
    int num_subscribers = c.num_subers;
    
    *len = num_subscribers*USERNAME_MAX+sizeof(TxtWho);
    *tw = (TxtWho*)malloc(*len);
    (*tw)->txt_type = TXT_WHO;
    (*tw)->txt_nusernames = num_subscribers;
    strcpy((*tw)->txt_channel,c.cname);

    int i;
    for(i=0;i<num_subscribers;i++){
        strcpy((*tw)->txt_users[i].us_username,u_list.users[c.subers[i]].uname);
    }
}

void handle_keep_alive(ReqKeepAlive* rka){
    printf("user doing keep alive\n");
}

int sendreply(int sock, SockAddrIn* to, char* msg, int len){
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

    if(c->num_subers ==0){
        //have to remove channel
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

void handle_request(Req* r, int sock, SockAddrIn *to){
    int uid, cid, i, n;
    int len = 0;
    void* reply;

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
            uid = get_userid(to);
            cid = get_cid(((ReqSay*)r)->req_channel);

            if(cid < 0 || is_user_in_channel(uid,cid) < 0){
                //handle error not in channel or channel doesn't exist
            }

            handle_say((ReqSay*)r, uid, &reply, &len);
            n = c_list.list[cid].num_subers;
            for(i=0;i<n;i++){
                to = &(u_list.users[i].addr);
                sendreply(sock,to,reply,len);
            }
            
            break;
        case REQ_LIST:
            handle_list((ReqList*)r, &reply, &len);
            sendreply(sock,to,reply,len);
            sleep(0);
            free(reply);
            break;
        case REQ_WHO:
            handle_who((ReqWho*)r, &reply, &len);
            sendreply(sock,to,reply,len);
            sleep(0);
            free(reply);
            break;
        case REQ_KEEP_ALIVE:
            handle_keep_alive((ReqKeepAlive*)r);
            break;
        default:
            ;//nothing
            break;
    }
}

void cleanup(void){
    if(sock)
        close(sock);
}

int main(void)
{
  int sock;
  SockAddrIn sa; 
  char buffer[2048];
  ssize_t recsize;
  socklen_t fromlen;

  init_duckchat();

  memset(buffer, 0, sizeof(buffer));

  atexit(cleanup);

  memset(&sa, 0, sizeof sa);
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl(INADDR_ANY);
  sa.sin_port = htons(7654);
  fromlen = sizeof sa;

  sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (bind(sock, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
    perror("error bind failed");
    close(sock);
    exit(EXIT_FAILURE);
  }

  for (;;) {
    printf("waiting for request...\n");
    recsize = recvfrom(sock, (void*)buffer, sizeof(buffer), 0, (struct sockaddr*)&sa, &fromlen);
    if (recsize < 0) {
      fprintf(stderr, "%s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
    handle_request(buffer,sock,&sa);
  }

  return 0;
}
