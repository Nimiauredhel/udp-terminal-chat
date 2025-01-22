#define _GNU_SOURCE
#include "server.h"

static const char *msg_format_text = "[%s] %s (%s:%u):\n ~ %s";
static const char *msg_format_join = "[%s] %s (%s:%u) has joined the conversation.";
static const char *msg_format_quit = "[%s] %s (%s:%u) has left the conversation.";

static bool should_terminate = false;

static void server_init(ServerSideData_t *data);
static void server_create_monitor_thread(ServerSideData_t *data);
static void *server_monitor_loop(void *arg);
static void server_loop(ServerSideData_t *data) __attribute__ ((__noreturn__));
static void handle_client_join(ServerSideData_t *data, char *join_message, struct sockaddr_in *address, int8_t index);
static void handle_client_quit(ServerSideData_t *data, int8_t index);
static void forward_message_to_clients(ServerSideData_t *data, int8_t subject_client_index, char *message, const char *format, bool to_subject, bool local_print);
static void server_terminate(ServerSideData_t *data, int exit_value) __attribute__ ((__noreturn__));
static int8_t get_free_client_index(ServerSideData_t *data);
static int8_t get_matching_client_index(ServerSideData_t* data, struct sockaddr_in *address);
static void timedate_to_string(time_t timedate, char *buff);
static void server_error_negative(int test_value, int exit_value, char *context_message, ServerSideData_t *data);
static void server_error_zero(int test_value, int exit_value, char *context_message, ServerSideData_t *data);

void server_start(void)
{
    ServerSideData_t *server_side_data = malloc(sizeof(ServerSideData_t));
    server_init(server_side_data);
    server_create_monitor_thread(server_side_data);
    server_loop(server_side_data);
}

static void server_signal_handler(int signum)
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

static void server_init(ServerSideData_t *data)
{
    int rx_port_int;
    char rx_port[PORT_BUFF_LENGTH];

    should_terminate = false;

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = server_signal_handler;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGHUP, &action, NULL);

    data->local_address.sin_family = AF_INET;
    data->local_address.sin_addr.s_addr = INADDR_ANY;

    printf("Server mode initializing.\nInput Rx port (leave blank for default): ");

    fgets(rx_port, PORT_BUFF_LENGTH, stdin);
    rx_port[strcspn(rx_port, "\n")] = 0;

    if (strlen(rx_port) <= 1024)
    {
        rx_port_int = PORT_MIN;
    }
    else
    {
        rx_port_int = atoi(rx_port);
    }

    data->local_address.sin_port = htons(rx_port_int);

    data->udp_rx_socket = socket(AF_INET, SOCK_DGRAM, 0);
    data->udp_tx_socket = socket(AF_INET, SOCK_DGRAM, 0);

    server_error_zero(data->udp_rx_socket, EXIT_FAILURE, "Failed to create rx socket", data);
    server_error_zero(data->udp_tx_socket, EXIT_FAILURE, "Failed to create tx socket", data);

    //bind the rx socket to the local address/port
    server_error_negative(bind(data->udp_rx_socket, (struct sockaddr *)&(data->local_address), sizeof(data->local_address)), EXIT_FAILURE, "Could not bind rx socket", data);

    struct ifaddrs *addresses_head;

    server_error_negative(getifaddrs(&addresses_head), EXIT_FAILURE, "Could not acquire addresses list", data);

    printf("Server initialization complete.\nAddresses:\n");

    struct ifaddrs *addresses_current = addresses_head;
    char host[NI_MAXHOST];
    
    while(addresses_current != NULL)
    {
        if (addresses_current->ifa_addr != NULL
            && addresses_current->ifa_addr->sa_family == AF_INET)
        {
            server_error_negative(
                getnameinfo(addresses_current->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST),
                EXIT_FAILURE, "getnameinfo failed", data);
            printf("    %s\n", host);
        }

        addresses_current = addresses_current->ifa_next;
    }

    freeifaddrs(addresses_head);
}

static int8_t get_free_client_index(ServerSideData_t *data)
{
    for (uint8_t i = 0; i < MAX_CLIENT_COUNT; i++)
    {
        if (!data->clients_connected[i]) return i;
    }

    return -1;
}

static int8_t get_matching_client_index(ServerSideData_t* data, struct sockaddr_in *address)
{
    for (uint8_t i = 0; i < MAX_CLIENT_COUNT; i++)
    {
        if (data->clients_connected[i]
            && data->clients_addresses[i].sin_addr.s_addr == address->sin_addr.s_addr
            && data->clients_addresses[i].sin_port == address->sin_port)
        {
            return i;
        }
    }

    return -1;
}

static void forward_message_to_clients(ServerSideData_t *data, int8_t subject_client_index, char *message, const char *format, bool to_subject, bool local_print)
{
    char timestamp[32];
    char full_message[MSG_BUFF_LENGTH];
    size_t message_length;
    socklen_t peer_address_length;

    timedate_to_string(time(NULL), timestamp);

    if (message != NULL && format != NULL)
    {
        sprintf(full_message, format, timestamp,
                subject_client_index == -1 ? "?" : data->clients_names[subject_client_index],
                inet_ntoa(data->clients_addresses[subject_client_index].sin_addr),
                ntohs(data->clients_addresses[subject_client_index].sin_port),
                message);
    }
    else if (format != NULL)
    {
        sprintf(full_message, format, timestamp,
                subject_client_index == -1 ? "?" : data->clients_names[subject_client_index],
                inet_ntoa(data->clients_addresses[subject_client_index].sin_addr),
                ntohs(data->clients_addresses[subject_client_index].sin_port));
    }
    else if (message != NULL)
    {
        sprintf(full_message, "%s ~ %s", timestamp, message);
    }
    else
    {
        // TODO: this function sucks, make it not suck
        return;
    }

    message_length = strlen(full_message) + 1;

    for (int8_t index = 0; index < MAX_CLIENT_COUNT; index++)
    {
        if (!data->clients_connected[index]
            || (to_subject && index != subject_client_index)
            || (!to_subject && index == subject_client_index))
            continue;

        peer_address_length = sizeof(data->clients_addresses[index]);

        if (0 > sendto(data->udp_tx_socket, full_message, message_length, 0,
        (struct sockaddr *)&(data->clients_addresses[index]),
        peer_address_length))
        {
            printf("Failed to forward message to %s:%u",
                    inet_ntoa(data->clients_addresses[index].sin_addr),
                    ntohs(data->clients_addresses[index].sin_port)); 
        }

        //printf("Forwarded message to %s (%s:%u).\n", clients.names[index], inet_ntoa(clients.addresses[index].sin_addr), ntohs(clients.addresses[index].sin_port));
    }

    if (local_print)
    {
        printf("%s\n", full_message);
    }
}

static void handle_client_join(ServerSideData_t *data, char *join_message, struct sockaddr_in *address, int8_t index)
{
    // new client, add to list if has room
    if (index == -1)
    {
        index = get_free_client_index(data);

        // no room, too bad
        // TODO: handle client rejection
        if (index == -1)
        {
            printf("Client rejected due to reaching max number of clients.\n");
        }
        // found room, add client to list
        else
        {
            char welcome_message[MSG_BUFF_LENGTH] = {0};
            char temp_buff[64];
            char port[ADDRESS_BUFF_LENGTH];
            char name[ADDRESS_BUFF_LENGTH];
            char *token;
            char *search = ":";
            uint16_t port_int;

            sprintf(welcome_message, "Welcome! There are currently %d participants.\n", data->clients_count);

            if (data->clients_count > 0)
            {
                for (uint8_t i = 0; i < MAX_CLIENT_COUNT; i++)
                {
                    if (data->clients_connected[i])
                    {
                        sprintf(temp_buff, "  %s\n", data->clients_names[i]);
                        strcat(welcome_message, temp_buff);
                    }
                }
            }

            strcat(welcome_message, "--------\n");

            data->clients_addresses[index] = *address;
            data->clients_connected[index] = true;
            data->clients_timers[index] = CLIENT_TIMEOUT;

            token = strtok(join_message, search);
            sprintf(port, "%s", token);
            port_int = atoi(port);
            token = strtok(NULL, search);
            sprintf(name, "%s", token);

            data->clients_addresses[index].sin_port = htons(port_int);
            sprintf(data->clients_names[index], "%s", name);
            data->clients_count++;

            forward_message_to_clients(data, index, welcome_message, NULL, true, false);
            forward_message_to_clients(data, index, NULL, msg_format_join, false, true);
        }
    }
    // existing client, reset timeout
    // TODO: implement client timeout ...
    else
    {
        data->clients_timers[index] = CLIENT_TIMEOUT;
    }
}

static void handle_client_quit(ServerSideData_t *data, int8_t index)
{
    if (index != -1)
    {
        data->clients_connected[index] = false;
        data->clients_count--;
        forward_message_to_clients(data, index, NULL, msg_format_quit, false, true);
    }
}

static void server_create_monitor_thread(ServerSideData_t *data)
{
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setstacksize(&attributes, PTHREAD_STACK_MIN);
    pthread_create(&(data->monitor_thread), &attributes, &server_monitor_loop, data);
}

static void server_loop(ServerSideData_t *data)
{
    printf ("Server online and receiving on port %u.\n", ntohs(data->local_address.sin_port));

    static struct sockaddr_in client_address = { .sin_family = AF_INET };
    socklen_t client_address_length = sizeof(client_address);
    int8_t client_index = -1;
    uint16_t msg_alloc_size = sizeof(Message_t) + MSG_BUFF_LENGTH;

    Message_t *incoming_message = malloc(msg_alloc_size);

    while(!should_terminate)
    {
        memset(incoming_message, 0, msg_alloc_size);
        ssize_t bytes_received = recvfrom(data->udp_rx_socket, incoming_message, msg_alloc_size, 0, (struct sockaddr*)&client_address, &client_address_length);

        if (should_terminate) break;

        server_error_zero(bytes_received, EXIT_FAILURE, "Failed to receive bytes", data);

        client_index = get_matching_client_index(data, &client_address);

        switch (ntohs(incoming_message->header.message_type))
        {
            case MESSAGE_UNDEFINED:
                printf("Received a packet from %s:%d -- Message: %s\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port), incoming_message->body);
                break;
            case MESSAGE_JOIN:
            case MESSAGE_STAY:
                handle_client_join(data, incoming_message->body, &client_address, client_index);
                break;
            case MESSAGE_QUIT:
                handle_client_quit(data, client_index);
                break;
            case MESSAGE_CHAT:
                forward_message_to_clients(data, client_index, incoming_message->body, msg_format_text, false, true);
                break;
        }
    }

    server_terminate(data, EXIT_SUCCESS);
}

static void *server_monitor_loop(void *arg)
{
    ServerSideData_t *data = (ServerSideData_t *)arg;

    while (!should_terminate)
    {
        sleep(1);

        for (uint8_t i = 0; !should_terminate && i < MAX_CLIENT_COUNT; i++)
        {
            if (data->clients_connected[i])
            {
                if (data->clients_timers[i] <= 0)
                {
                    handle_client_quit(data, i);
                }
                else
                {
                    data->clients_timers[i] -= 1;
                }
            }
        }
    }

    return NULL;
}

static void server_terminate(ServerSideData_t *data, int exit_value)
{
    printf("Terminating.\n");

    pthread_join(data->monitor_thread, NULL);
    close(data->udp_rx_socket);
    close(data->udp_tx_socket);
    free(data);
    exit(exit_value);
}

static void timedate_to_string(time_t timedate, char *buff)
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


static void server_error_negative(int test_value, int exit_value, char *context_message, ServerSideData_t *data)
{
    if (test_value < 0)
    {
        perror(context_message);
        server_terminate(data, exit_value);
    }
}

static void server_error_zero(int test_value, int exit_value, char *context_message, ServerSideData_t *data)
{
    server_error_negative(test_value - 1, exit_value, context_message, data);
}

