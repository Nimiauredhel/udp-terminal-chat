#ifndef SERVER_H
#define SERVER_H

#include "common.h"
#include "networking_common.h"

typedef enum ServerForwardingScope
{
    SFORWARD_UNDEFINED = 0,
    SFORWARD_ALL = 1, // forward to all connected clients
    SFORWARD_INDIVIDUAL = 2, // forward to the subject only
    SFORWARD_OTHERS = 3, // forward to everyone *except* the subject
} ServerForwardingScope_t;

typedef struct ServerClientsList
{
    pthread_mutex_t lock;
    uint8_t count;
    UserStatus_t status_flags[MAX_CLIENT_COUNT];
    uint8_t status_timers[MAX_CLIENT_COUNT];
    uint8_t connection_timers[MAX_CLIENT_COUNT];
    struct sockaddr_in addresses[MAX_CLIENT_COUNT];
    char names[MAX_CLIENT_COUNT][NAME_BUFF_LENGTH];
} ServerClientsList_t;

typedef struct ServerSideData
{
    int udp_rx_socket;
    int udp_tx_socket;
    struct sockaddr_in local_address;
    pthread_t monitor_thread;
    ServerClientsList_t clients;

} ServerSideData_t;

void server_start(void) __attribute__ ((__noreturn__));

#endif
