#ifndef NETWORKING_COMMON_H
#define NETWORKING_COMMON_H

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <bits/pthread_stack_min.h>

#include "common.h"

#define ENCODING_VERSION 0

#define PORT_BUFF_LENGTH 8
#define ADDRESS_BUFF_LENGTH 40
#define NAME_BUFF_LENGTH 32
#define MSG_MAX_CHARS 64

#define PORT_MIN 49152
#define PORT_MAX 65535

typedef enum MessageType
{
    MESSAGE_UNDEFINED = 0,
    MESSAGE_JOIN = 1,
    MESSAGE_QUIT = 2,
    MESSAGE_CHAT = 3,
    MESSAGE_RAW = 4,
    MESSAGE_STAY = 5,
    MESSAGE_ERROR = 6,
} MessageType_t;

#pragma pack(push, 2)

typedef struct MessageHeader
{
    uint16_t encoding_version;
    uint16_t body_length;
    time_t timestamp;
    MessageType_t message_type;
} MessageHeader_t;

typedef struct Message
{
    MessageHeader_t header;
    char body[MSG_MAX_CHARS];
} Message_t;

#pragma pack(pop)

#endif
