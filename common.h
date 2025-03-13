#ifndef COMMON_H
#define COMMON_H

#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <pthread.h>
#include <signal.h>
#include <ncurses.h>

extern bool should_terminate;

void initialize_signal_handler(void);
void signal_handler(int signum);
void timedate_to_timestring(time_t timedate, char *buff);
void timedate_to_timedatestring(time_t timedate, char *buff);

#endif
