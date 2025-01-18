#ifndef NETWORKING_COMMON_H
#define NETWORKING_COMMON_H

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#define ADDRESS_BUFF_LENGTH 16
#define MSG_BUFF_LENGTH 511

typedef enum MessageHeader
{
    MESSAGE_UNDEFINED = 0,
    MESSAGE_JOIN = 1,
    MESSAGE_QUIT = 2,
    MESSAGE_TEXT = 3,
} MessageHeader_t;

#pragma pack(2)
typedef struct Message
{
    MessageHeader_t header;
    char body[MSG_BUFF_LENGTH];
} Message_t;


extern const uint16_t address_buff_length;
extern const uint16_t msg_buff_length;

#endif
