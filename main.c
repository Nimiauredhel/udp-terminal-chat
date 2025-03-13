#include "common.h"
#include "client.h"
#include "server.h"

#define MODES_COUNT 2
#define MODES_MAXLENGTH 8

int8_t get_selection_from_args(int argc, char *argv[]);
void print_usage(const char* process_name);

const char modes[MODES_COUNT][MODES_MAXLENGTH] =
{
    "server", "client"
};


int main(int argc, char *argv[])
{
    switch (get_selection_from_args(argc, argv))
    {
        case -1:
            print_usage(argv[0]);
            printf("Terminating.\n");
            return EINVAL;
        case 0:
            server_start();
            break;
        case 1:
            client_start();
    }

    return EXIT_SUCCESS;
}

int8_t get_selection_from_args(int argc, char *argv[])
{
    int8_t selection = -1;

    if (argc > 1)
    {
        for (uint8_t i = 0; i < MODES_COUNT; i++)
        {
            if (0 == strcmp(argv[1], modes[i]))
            {
                selection = i;
                break;
            }
        }
    }

    return selection;
}

void print_usage(const char* process_name)
{
    printf("Usage: %s <mode>\nAvailable modes:\n", process_name);

    for (uint8_t i = 0; i < MODES_COUNT; i++)
    {
        printf("  [%s]\n", modes[i]);
    }
}
