#ifndef SERVER_H
#define SERVER_H

#include "common.h"
#include "networking_common.h"

#define MAX_CLIENT_COUNT 16
#define CLIENT_TIMEOUT 2

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
    char clients_names[MAX_CLIENT_COUNT][ADDRESS_BUFF_LENGTH];
} ServerSideData_t;

void server_start(void);

#endif
