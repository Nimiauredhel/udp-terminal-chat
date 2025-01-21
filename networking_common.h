#ifndef NETWORKING_COMMON_H
#define NETWORKING_COMMON_H

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>

#define ENCODING_VERSION 0

#define PORT_BUFF_LENGTH 8
#define ADDRESS_BUFF_LENGTH 40
#define NAME_BUFF_LENGTH 32
#define MSG_BUFF_LENGTH 512
#define MSG_ALLOC_SIZE (sizeof(Message_t) + MSG_BUFF_LENGTH)

typedef enum MessageType
{
    MESSAGE_ERROR = -1,
    MESSAGE_UNDEFINED = 0,
    MESSAGE_JOIN = 1,
    MESSAGE_QUIT = 2,
    MESSAGE_CHAT = 3,
    MESSAGE_RAW = 4,
    MESSAGE_STAY = 5,
} MessageType_t;

#pragma pack(push, 2)

typedef struct MessageHeader
{
    uint16_t encoding_version;
    uint16_t body_length;
    int64_t timestamp;
    MessageType_t message_type;
} MessageHeader_t;

typedef struct Message
{
    MessageHeader_t header;
    char body[];
} Message_t;

#pragma pack(pop)

#endif
