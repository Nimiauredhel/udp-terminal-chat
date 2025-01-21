#ifndef CLIENT_H
#define CLIENT_H

#include "common.h"
#include "networking_common.h"

typedef struct ClientSideData
{
    int udp_socket;
    struct sockaddr_in server_address;
    struct sockaddr_in local_address;
    char client_name[NAME_BUFF_LENGTH];
} ClientSideData_t;

void client_start(void);

#endif
