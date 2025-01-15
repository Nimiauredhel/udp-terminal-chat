#ifndef NETWORKING_COMMON_H
#define NETWORKING_COMMON_H

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#define ADDRESS_BUFF_LENGTH 16
#define MSG_BUFF_LENGTH 1024

extern const uint16_t address_buff_length;
extern const uint16_t msg_buff_length;

#endif
