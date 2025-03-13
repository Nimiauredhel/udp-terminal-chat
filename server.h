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

typedef struct ServerSideData
{
    int udp_rx_socket;
    int udp_tx_socket;
    struct sockaddr_in local_address;
    pthread_t monitor_thread;

    uint8_t clients_count;
    bool clients_connected[MAX_CLIENT_COUNT];
    uint8_t clients_timers[MAX_CLIENT_COUNT];
    struct sockaddr_in clients_addresses[MAX_CLIENT_COUNT];
    char clients_names[MAX_CLIENT_COUNT][NAME_BUFF_LENGTH];
} ServerSideData_t;

void server_start(void) __attribute__ ((__noreturn__));

#endif
