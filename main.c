#include "common.h"
#include "networking_common.h"

#define MODES_COUNT 2
#define MODES_MAXLENGTH 8

const char modes[MODES_COUNT][MODES_MAXLENGTH] =
{
    "server", "client"
};

int8_t get_selection_from_args(int argc, char *argv[]);
void print_usage(const char* process_name);

int main(int argc, char *argv[])
{

    int8_t selection = get_selection_from_args(argc, argv);

    if (selection < 0)
    {
        print_usage(argv[0]);
        printf("Terminating.\n");
        return EINVAL;
    }

    printf("Hello World.\n");
    printf("You selected %s mode.\n", modes[selection]);

    return EXIT_SUCCESS;
}

int8_t get_selection_from_args(int argc, char *argv[])
{
    int8_t selection = -1;

    if (argc == 2)
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
