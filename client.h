#ifndef CLIENT_H
#define CLIENT_H

#include "common.h"
#include "networking_common.h"

#define CLIENT_MAX_VISIBLE_MSGS 16

typedef struct ClientMsgList
{
    bool dirty;
    pthread_mutex_t lock;
    int8_t head;
    int8_t tail;
    char msgs[CLIENT_MAX_VISIBLE_MSGS][MSG_REP_MAX_CHARS];
} ClientMsgList_t;

typedef struct ClientPeerList
{
    bool dirty;
    pthread_mutex_t lock;
    bool connected[MAX_CLIENT_COUNT];
    char names[MAX_CLIENT_COUNT][NAME_BUFF_LENGTH];
} ClientPeerList_t;

typedef struct ClientInputState
{
    bool dirty;
    pthread_mutex_t lock;
    int16_t idx;
    char buff[MSG_MAX_CHARS];
} ClientInputState_t;

typedef struct ClientTUIData
{
    bool dirty_size;
    int rows;
    int cols;
    int prev_rows;
    int prev_cols;
    WINDOW *win_messages;
    WINDOW *win_input;
    WINDOW *win_users;
    WINDOW *win_session;
} ClientTUIData_t;

typedef struct ClientSessionData
{
    bool dirty;
    pthread_mutex_t lock;
} ClientSessionData_t;

typedef struct ClientSideData
{
    int udp_socket;
    pthread_t rx_thread;
    pthread_t render_thread;
    struct sockaddr_in server_address;
    struct sockaddr_in local_address;

    char client_name[NAME_BUFF_LENGTH];
    Message_t outgoing_message;

    ClientInputState_t input;
    ClientMsgList_t msg_list;
    ClientPeerList_t peer_list;
    ClientTUIData_t tui;
    ClientSessionData_t session;
} ClientSideData_t;

void client_start(void) __attribute__ ((__noreturn__));

#endif
