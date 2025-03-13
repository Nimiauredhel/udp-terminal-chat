#include "networking_common.h"

const char *msg_format_text = "[%s] %s: %s";
const char *msg_format_join = "[%s] %s has joined the conversation.";
const char *msg_format_quit = "[%s] %s has left the conversation.";

const char *get_format_by_message_type(MessageType_t msg_type)
{
    switch (msg_type)
    {
        case MESSAGE_JOIN:
            return msg_format_join;
        case MESSAGE_QUIT:
            return msg_format_quit;
        case MESSAGE_CHAT:
            return msg_format_text;
        case MESSAGE_UNDEFINED:
        case MESSAGE_RAW:
        case MESSAGE_STAY:
        case MESSAGE_ERROR:
        default:
            return NULL;
    }
}

void format_message(const char *message, const char *sender, time_t timedate, const char *format, char *output)
{
    char timestamp[32];
    timedate_to_timestring(timedate, timestamp);

    if (message != NULL && format != NULL)
    {
        sprintf(output, format, timestamp, sender == NULL ? "?" : sender, message);
    }
    else if (format != NULL)
    {
        sprintf(output, format, timestamp, sender == NULL ? "?" : sender);
    }
    else if (message != NULL)
    {
        sprintf(output, "%s ~ %s", timestamp, message);
    }
    else
    {
        // TODO: this function sucks, make it not suck
        sprintf(output, "%s ~ null message", timestamp);
    }
}
