#include "server.h"

static void server_init(ServerSideData_t *data);
static void server_create_monitor_thread(ServerSideData_t *data);
static void *server_monitor_loop(void *arg);
static void server_loop(ServerSideData_t *data) __attribute__ ((__noreturn__));
static void handle_client_join(ServerSideData_t *data, char *join_message, struct sockaddr_in *address, int8_t index);
static void handle_client_quit(ServerSideData_t *data, int8_t index);
static void forward_message_to_clients(ServerSideData_t *data, Message_t *message, int8_t subject_client_index, ServerForwardingScope_t scope, bool local_print);
static void server_terminate(ServerSideData_t *data, int exit_value) __attribute__ ((__noreturn__));
static int8_t get_free_client_index(ServerSideData_t *data);
static int8_t get_matching_client_index(ServerSideData_t* data, struct sockaddr_in *address);
static void server_error_negative(int test_value, int exit_value, char *context_message, ServerSideData_t *data);
static void server_error_zero(int test_value, int exit_value, char *context_message, ServerSideData_t *data);

void server_start(void)
{
    initialize_signal_handler();

    ServerSideData_t *server_side_data = malloc(sizeof(ServerSideData_t));
    server_init(server_side_data);
    server_create_monitor_thread(server_side_data);
    server_loop(server_side_data);
}

static void server_init(ServerSideData_t *data)
{
    int rx_port_int;
    char rx_port[PORT_BUFF_LENGTH];

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

static void forward_message_to_clients(ServerSideData_t *data, Message_t *message, int8_t subject_client_index, ServerForwardingScope_t scope, bool local_print)
{
    socklen_t peer_address_length;
    
    switch(scope)
    {
    case SFORWARD_UNDEFINED:
        break;
    case SFORWARD_INDIVIDUAL:
        peer_address_length = sizeof(data->clients_addresses[subject_client_index]);

        if (0 > sendto(data->udp_tx_socket, message, sizeof(Message_t), 0,
            (struct sockaddr *)&(data->clients_addresses[subject_client_index]), peer_address_length))
        {
            printf("Failed to forward message to %s:%u",
                    inet_ntoa(data->clients_addresses[subject_client_index].sin_addr),
                    ntohs(data->clients_addresses[subject_client_index].sin_port)); 
        }
        break;
    case SFORWARD_ALL:
    case SFORWARD_OTHERS:
        for (int8_t index = 0; index < MAX_CLIENT_COUNT; index++)
        {
            if (!data->clients_connected[index] || (scope == SFORWARD_OTHERS && index == subject_client_index))
                continue;

            peer_address_length = sizeof(data->clients_addresses[index]);

            if (0 > sendto(data->udp_tx_socket, message, sizeof(Message_t), 0,
                (struct sockaddr *)&(data->clients_addresses[index]), peer_address_length))
            {
                printf("Failed to forward message to %s:%u",
                        inet_ntoa(data->clients_addresses[index].sin_addr),
                        ntohs(data->clients_addresses[index].sin_port)); 
            }

            //printf("Forwarded message to %s (%s:%u).\n", data->clients_names[index], inet_ntoa(data->clients_addresses[index].sin_addr), ntohs(data->clients_addresses[index].sin_port));
        }
      break;
    }

    if (local_print)
    {
        char full_message[MSG_MAX_CHARS * 2];

        format_message(message->body, subject_client_index == -1 ? NULL : data->clients_names[subject_client_index],
                ntohl(message->header.timestamp), get_format_by_message_type(ntohs(message->header.message_type)), full_message);

        printf("%s\n", full_message);
    }
}

static void handle_client_join(ServerSideData_t *data, char *join_message, struct sockaddr_in *address, int8_t subject_index)
{
    // new client, add to list if has room
    if (subject_index == -1)
    {
        subject_index = get_free_client_index(data);

        // no room, too bad
        // TODO: handle client rejection
        if (subject_index == -1)
        {
            printf("Client rejected due to reaching max number of clients.\n");
        }
        // found room, add client to list
        else
        {
            char port[ADDRESS_BUFF_LENGTH];
            char name[NAME_BUFF_LENGTH];
            char *token;
            char *search = ":";
            uint16_t port_int;

            data->clients_addresses[subject_index] = *address;
            data->clients_connected[subject_index] = true;
            data->clients_timers[subject_index] = CLIENT_TIMEOUT;

            // parse user data from join message
            token = strtok(join_message, search);
            sprintf(port, "%s", token);
            port_int = atoi(port);
            token = strtok(NULL, search);
            sprintf(name, "%s", token);

            data->clients_addresses[subject_index].sin_port = htons(port_int);
            sprintf(data->clients_names[subject_index], "%s", name);
            data->clients_count++;

            // prepare base notification packet
            time_t timedate = time(NULL);
            Message_t notification = { .header = { .sender_index = 255, .sender_name = "SERVER", .timestamp = htonl(timedate), .encoding_version = htons(ENCODING_VERSION) } };

            // send current user data to new user
            for (uint8_t i = 0; i < MAX_CLIENT_COUNT; i++)
            {
                if (data->clients_connected[i])
                {
                    notification.header.sender_index = i;
                    strncpy(notification.header.sender_name, data->clients_names[i], NAME_BUFF_LENGTH);
                    notification.header.message_type = htons(MESSAGE_USERDATA);
                    forward_message_to_clients(data, &notification, subject_index, SFORWARD_INDIVIDUAL, false);
                }
            }

            // notify other users about new join
            notification.header.sender_index = subject_index;
            strncpy(notification.header.sender_name, subject_index < 0 ? "?" : data->clients_names[subject_index], NAME_BUFF_LENGTH);
            notification.header.message_type = htons(MESSAGE_JOIN);
            forward_message_to_clients(data, &notification, subject_index, SFORWARD_OTHERS, true);
        }
    }
    // existing client, reset timeout
    else
    {
        data->clients_timers[subject_index] = CLIENT_TIMEOUT;
    }
}

static void handle_client_quit(ServerSideData_t *data, int8_t index)
{
    if (index != -1)
    {
        // notify clients about quitting user
        time_t timedate = time(NULL);
        Message_t notification = { .header = { .timestamp = timedate, .encoding_version = htons(ENCODING_VERSION) } };

        notification.header.sender_index = index;
        strncpy(notification.header.sender_name, data->clients_names[index], NAME_BUFF_LENGTH);
        notification.header.message_type = htons(MESSAGE_QUIT);
        forward_message_to_clients(data, &notification, index, SFORWARD_OTHERS, true);

        // locally unregister user
        data->clients_connected[index] = false;
        data->clients_count--;
        explicit_bzero(data->clients_names[index], NAME_BUFF_LENGTH);
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
    Message_t incoming_message = {0};

    while(!should_terminate)
    {
        explicit_bzero(&incoming_message, sizeof(Message_t));
        ssize_t bytes_received = recvfrom(data->udp_rx_socket, &incoming_message, sizeof(Message_t), 0, (struct sockaddr*)&client_address, &client_address_length);

        if (should_terminate) break;

        server_error_zero(bytes_received, EXIT_FAILURE, "Failed to receive bytes", data);

        client_index = get_matching_client_index(data, &client_address);

        switch ((MessageType_t)ntohs(incoming_message.header.message_type))
        {
            case MESSAGE_UNDEFINED:
                printf("Received a packet from %s:%d -- Message: %s\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port), incoming_message.body);
                break;
            case MESSAGE_JOIN:
            case MESSAGE_STAY:
                handle_client_join(data, incoming_message.body, &client_address, client_index);
                break;
            case MESSAGE_QUIT:
                handle_client_quit(data, client_index);
                break;
            case MESSAGE_CHAT:
                incoming_message.header.sender_index = client_index;
                forward_message_to_clients(data, &incoming_message, client_index, SFORWARD_ALL, true);
                break;
            case MESSAGE_RAW:
            case MESSAGE_ERROR:
            case MESSAGE_USER_IS_TYPING:
            case MESSAGE_USERDATA:
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

