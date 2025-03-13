#ifndef CLIENT_H
#define CLIENT_H

#include "common.h"
#include "networking_common.h"

#define CLIENT_MAX_VISIBLE_MSGS 16

typedef struct ClientMsgList
{
    bool dirty;
    int8_t head;
    int8_t tail;
    pthread_mutex_t lock;
    char msgs[CLIENT_MAX_VISIBLE_MSGS][MSG_REP_MAX_CHARS];
} ClientMsgList_t;

typedef struct ClientInputState
{
    bool dirty;
    int16_t idx;
    pthread_mutex_t lock;
    char buff[MSG_MAX_CHARS];
} ClientInputState_t;

typedef struct ClientTUIData
{
    bool dirty_users;
    bool dirty_session;
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
    ClientTUIData_t tui;
} ClientSideData_t;

void client_start(void) __attribute__ ((__noreturn__));

#endif
