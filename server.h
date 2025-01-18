#ifndef SERVER_H
#define SERVER_H

#include "common.h"
#include "networking_common.h"

#define MAX_CLIENT_COUNT 16

typedef struct ClientsData
{
    bool connected[MAX_CLIENT_COUNT];
    struct sockaddr_in addresses[MAX_CLIENT_COUNT];
    char names[MAX_CLIENT_COUNT][ADDRESS_BUFF_LENGTH];
} ClientsData_t;

void server_init(void);
void server_loop(void);

#endif
