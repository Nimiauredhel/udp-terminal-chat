#ifndef CLIENT_H
#define CLIENT_H

#include "common.h"
#include "networking_common.h"

typedef struct ClientSideData
{
    int udp_socket;
    pthread_t rx_thread;
    struct sockaddr_in server_address;
    struct sockaddr_in local_address;
    char client_name[NAME_BUFF_LENGTH];
    Message_t *outgoing_message;
} ClientSideData_t;

void client_start(void) __attribute__ ((__noreturn__));

#endif
