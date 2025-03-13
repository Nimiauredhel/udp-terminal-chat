#ifndef NETWORKING_COMMON_H
#define NETWORKING_COMMON_H

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <bits/pthread_stack_min.h>

#include "common.h"

#define ENCODING_VERSION (1)

#define PORT_BUFF_LENGTH (8)
#define ADDRESS_BUFF_LENGTH (40)
#define NAME_BUFF_LENGTH (16)
#define MSG_MAX_CHARS (64)
#define MSG_REP_MAX_CHARS (MSG_MAX_CHARS * 2)

#define PORT_MIN (49152)
#define PORT_MAX (65535)

#define MAX_CLIENT_COUNT 16
#define CLIENT_TIMEOUT 16

#define STATUS_HOT_TIMEOUT 1
#define STATUS_TYPING_TIMEOUT 1
#define STATUS_ACTIVE_TIMEOUT 9
#define STATUS_NEW_TIMEOUT 5

typedef enum MessageType
{
    MESSAGE_UNDEFINED = 0,
    MESSAGE_JOIN = 1,
    MESSAGE_QUIT = 2,
    MESSAGE_CHAT = 3,
    MESSAGE_RAW = 4,
    MESSAGE_STAY = 5,
    MESSAGE_ERROR = 6,
    MESSAGE_USERDATA = 7,
} MessageType_t;

typedef enum UserStatus
{
    USTATUS_NONE = 0, // undefined or vacated user slot
    USTATUS_IDLE = 1, // recently inactive
    USTATUS_NEW = 2, // recently joined
    USTATUS_ACTIVE = 3, // recently typed or sent a message
    USTATUS_TYPING = 4, // just typed
    USTATUS_HOT = 5, // just sent a message
} UserStatus_t;

#pragma pack(push, 2)

typedef struct MessageHeader
{
    uint16_t encoding_version;
    uint16_t body_length;
    time_t timestamp;
    MessageType_t message_type;
    uint8_t sender_index;
    char sender_name[NAME_BUFF_LENGTH];
} MessageHeader_t;

typedef struct Message
{
    MessageHeader_t header;
    char body[MSG_MAX_CHARS];
} Message_t;

#pragma pack(pop)

extern const char *msg_format_text;
extern const char *msg_format_join;
extern const char *msg_format_quit;

const char *get_format_by_message_type(MessageType_t msg_type);
void format_message(const char *message, const char *sender, time_t timedate, const char *format, char *output);

#endif
