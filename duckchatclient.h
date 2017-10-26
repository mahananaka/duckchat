#ifndef DUCKCHAT_CLIENT_H
#define DUCKCHAT_CLIENT_H

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

#define IN_BUFFER_SIZE 2048
#define OUT_BUFFER_SIZE 128
#define REQ_ARG_COUNT 4
#define USERNAME_MAX 32
#define CHANNEL_MAX 32
#define SAY_MAX 64
#define MAX_PORT 65535
#define SERVERNAME_MAX 255
#define MAX_NO_CHANNELS 64

extern char sname[SERVERNAME_MAX];
extern char uname[USERNAME_MAX];
extern char achannel[CHANNEL_MAX];
extern int port_num;
extern int sock;

typedef struct _channel {
    char cname[CHANNEL_MAX+1];
} Channel;

typedef struct _channel_list{
    int num_channels;
    Channel list[MAX_NO_CHANNELS];
} ChannelList;

void send_request(SockAddrIn* to, char* msg, int len);
void request_login(ReqLogin* rl, SockAddrIn* to);
void request_join(ReqJoin* rl, char* channel_name, SockAddrIn* to);
void process_command(Req* r, char* input, SockAddrIn* to);
void request_say(ReqSay* rs, char* msg, char* channel, SockAddrIn* to);

#endif
