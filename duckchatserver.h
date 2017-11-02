#ifndef DUCKCHAT_SERVER_H
#define DUCKCHAT_SERVER_H

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

#define MAX_NO_CHANNELS 32
#define MAX_USERS_PER_CHANNEL 32

typedef struct _user {
    char uname[USERNAME_MAX+1];
    SockAddrIn addr;
    int num_subs;
    int subbed_channels[MAX_NO_CHANNELS];
} User;

typedef struct _channel {
    char cname[CHANNEL_MAX+1];
    int num_subers;
    SockAddrIn subers[MAX_USERS_PER_CHANNEL];
} Channel;

typedef struct _user_list {
    int num_users;
    User users[MAX_NO_CHANNELS*MAX_USERS_PER_CHANNEL];
} UserList;

typedef struct _channel_list{
    int num_channels;
    Channel list[MAX_NO_CHANNELS];
} ChannelList;

extern int sock;
extern ChannelList c_list;
extern UserList u_list;

/* event handlers */
void handle_login(ReqLogin* rl, SockAddrIn* requester);
void handle_logout(ReqLogout* rl, SockAddrIn* requester);
void handle_join(ReqJoin* rj, SockAddrIn* requester);
void handle_leave(ReqLeave* rl, SockAddrIn* requester);
void handle_say(ReqSay* rs, SockAddrIn* requester);
void handle_list(ReqList* rl, SockAddrIn* to);
void handle_who(ReqWho* rw, SockAddrIn* to);
void handle_error(char* msg, SockAddrIn* to);
void handle_keep_alive(ReqKeepAlive* rka);

/* reply function */
int sendreply(SockAddrIn* to, char* msg, int len);

/* helper functions */
int is_double_join(int uid, int cid);
int is_user_in_channel(SockAddrIn *requester, int cid);
int rem_user_from_channel(SockAddrIn *requester, int cid);
int get_userid(SockAddrIn *who);
int get_cid(char *name);
void replace_channel(int what, int with);

#endif
