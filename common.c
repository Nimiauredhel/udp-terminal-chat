#include "common.h"

bool should_terminate = false;

void initialize_signal_handler(void)
{
    should_terminate = false;

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = signal_handler;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGHUP, &action, NULL);
}

void signal_handler(int signum)
{
    switch (signum)
    {
        case SIGINT:
        case SIGTERM:
        case SIGHUP:
            should_terminate = true;
            break;
    }
}

void timedate_to_timestring(time_t timedate, char *buff)
{
    struct tm *now_structured_ptr = localtime(&timedate);

    sprintf(buff, "%02d:%02d:%02d",
            now_structured_ptr->tm_hour,
            now_structured_ptr->tm_min,
            now_structured_ptr->tm_sec);
}

void timedate_to_timedatestring(time_t timedate, char *buff)
{
    struct tm *now_structured_ptr = localtime(&timedate);

    sprintf(buff, "%02d:%02d:%02d %02d/%02d/%d",
            now_structured_ptr->tm_hour,
            now_structured_ptr->tm_min,
            now_structured_ptr->tm_sec,
            now_structured_ptr->tm_mday,
            now_structured_ptr->tm_mon + 1,
            now_structured_ptr->tm_year + 1900);
}

