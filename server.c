#define _GNU_SOURCE
#include "server.h"

static const char *msg_format_text = "[%s] %s (%s:%u):\n ~ %s";
static const char *msg_format_join = "[%s] %s (%s:%u) has joined the conversation.";
static const char *msg_format_quit = "[%s] %s (%s:%u) has left the conversation.";

static int udp_rx_socket;
static int udp_tx_socket;

static ServerSideClientsData_t clients = {0};

static struct sockaddr_in local_address = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY };

void get_current_timedate_string(char *buff)
{
    time_t now = time(NULL);
    struct tm *now_structured_ptr = localtime(&now);

    sprintf(buff, "%02d:%02d:%02d %02d/%02d/%d",
            now_structured_ptr->tm_hour,
            now_structured_ptr->tm_min,
            now_structured_ptr->tm_sec,
            now_structured_ptr->tm_mday,
            now_structured_ptr->tm_mon + 1,
            now_structured_ptr->tm_year + 1900);
}

static int8_t get_free_client_index(void)
{
    for (uint8_t i = 0; i < MAX_CLIENT_COUNT; i++)
    {
        if (!clients.connected[i]) return i;
    }

    return -1;
}

static int8_t get_matching_client_index(struct sockaddr_in *address)
{
    for (uint8_t i = 0; i < MAX_CLIENT_COUNT; i++)
    {
        if (clients.connected[i]
            && clients.addresses[i].sin_addr.s_addr == address->sin_addr.s_addr
            && clients.addresses[i].sin_port == address->sin_port)
        {
            return i;
        }
    }

    return -1;
}

void forward_message_to_clients(int8_t subject_client_index, char *message, const char *format, bool to_subject, bool local_print)
{
    char timestamp[32];
    char full_message[MSG_BUFF_LENGTH];
    size_t message_length;
    socklen_t peer_address_length;

    get_current_timedate_string(timestamp);

    if (message != NULL && format != NULL)
    {
        sprintf(full_message, format, timestamp,
                subject_client_index == -1 ? "?" : clients.names[subject_client_index],
                inet_ntoa(clients.addresses[subject_client_index].sin_addr),
                ntohs(clients.addresses[subject_client_index].sin_port),
                message);
    }
    else if (format != NULL)
    {
        sprintf(full_message, format, timestamp,
                subject_client_index == -1 ? "?" : clients.names[subject_client_index],
                inet_ntoa(clients.addresses[subject_client_index].sin_addr),
                ntohs(clients.addresses[subject_client_index].sin_port));
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
        if (!clients.connected[index]
            || (to_subject && index != subject_client_index)
            || (!to_subject && index == subject_client_index))
            continue;

        peer_address_length = sizeof(clients.addresses[index]);

        if (0 > sendto(udp_tx_socket, full_message, message_length, 0,
        (struct sockaddr *)&clients.addresses[index],
        peer_address_length))
        {
            printf("Failed to forward message to %s:%u",
                    inet_ntoa(clients.addresses[index].sin_addr),
                    ntohs(clients.addresses[index].sin_port)); 
        }

        //printf("Forwarded message to %s (%s:%u).\n", clients.names[index], inet_ntoa(clients.addresses[index].sin_addr), ntohs(clients.addresses[index].sin_port));
    }

    if (local_print)
    {
        printf("%s\n", full_message);
    }
}

static void handle_client_join(char *join_message, struct sockaddr_in *address)
{
    int8_t index = get_matching_client_index(address);

    // new client, add to list if has room
    if (index == -1)
    {
        index = get_free_client_index();

        // no room, too bad
        // TODO: handle client rejection
        if (index == -1)
        {
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

            sprintf(welcome_message, "Welcome! There are currently %d participants.\n", clients.count);

            if (clients.count > 0)
            {
                for (uint8_t i = 0; i < MAX_CLIENT_COUNT; i++)
                {
                    if (clients.connected[i])
                    {
                        sprintf(temp_buff, "  %s\n", clients.names[i]);
                        strcat(welcome_message, temp_buff);
                    }
                }
            }

            strcat(welcome_message, "--------\n");

            clients.addresses[index] = *address;
            clients.connected[index] = true;

            token = strtok(join_message, search);
            sprintf(port, "%s", token);
            port_int = atoi(port);
            token = strtok(NULL, search);
            sprintf(name, "%s", token);

            clients.addresses[index].sin_port = port_int;
            sprintf(clients.names[index], "%s", name);
            clients.count++;

            forward_message_to_clients(index, welcome_message, NULL, true, false);
            forward_message_to_clients(index, NULL, msg_format_join, false, true);
        }
    }
    // existing client, reset timeout
    // TODO: implement client timeout ...
    else
    {
    }
}

void handle_client_quit(struct sockaddr_in *address)
{
    int8_t index = get_matching_client_index(address);
    
    if (index != -1)
    {
        clients.connected[index] = false;
        clients.count--;
        forward_message_to_clients(index, NULL, msg_format_quit, false, true);
    }
}

void server_init(void)
{
    int rx_port_int;
    char rx_port[PORT_BUFF_LENGTH];

    printf("Server mode initializing.\n");

    printf("Input Rx port (leave blank for 8080): ");
    fgets(rx_port, PORT_BUFF_LENGTH, stdin);
    rx_port[strcspn(rx_port, "\n")] = 0;

    if (strlen(rx_port) < 1)
    {
        rx_port_int = 8080;
    }
    else
    {
        rx_port_int = atoi(rx_port);
    }

    local_address.sin_port = htons(rx_port_int);

    if ((udp_rx_socket = socket(AF_INET, SOCK_DGRAM, 0)) <= 0)
    {
        perror("Failed to create rx socket");
        exit(EXIT_FAILURE);
    }

    if ((udp_tx_socket = socket(AF_INET, SOCK_DGRAM, 0)) <= 0)
    {
        perror("Failed to create tx socket");
        exit(EXIT_FAILURE);
    }

    //bind the rx socket to the local address/port
    int result = bind(udp_rx_socket, (struct sockaddr *)&local_address, sizeof(local_address));

    if (result < 0)
    {
        perror("Could not bind rx socket");
        exit(EXIT_FAILURE);
    }

    struct ifaddrs *addresses_head;

    if (getifaddrs(&addresses_head) < 0)
    {
        perror("Could not acquire addresses list.");
        exit(EXIT_FAILURE);
    }

    printf("Server initialization complete.\nAddresses:\n");

    struct ifaddrs *addresses_current = addresses_head;
    char host[NI_MAXHOST];
    
    while(addresses_current != NULL)
    {
        if (addresses_current->ifa_addr != NULL
            && addresses_current->ifa_addr->sa_family == AF_INET)
        {
            if(getnameinfo(addresses_current->ifa_addr, sizeof(struct sockaddr_in),
                host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST) < 0)
            {
                perror("getnameinfo failed");
                exit(EXIT_FAILURE);
            }

            printf("    %s\n", host);
        }

        addresses_current = addresses_current->ifa_next;
    }

    freeifaddrs(addresses_head);
}

void server_loop(void)
{
    printf ("Server online.\n");

    static struct sockaddr_in client_address = { .sin_family = AF_INET };
    socklen_t client_address_length = sizeof(client_address);
    int8_t client_index = -1;
    uint16_t msg_alloc_size = sizeof(Message_t) + MSG_BUFF_LENGTH;

    Message_t *incoming_message = malloc(msg_alloc_size);

    while(true)
    {
        memset(incoming_message, 0, msg_alloc_size);
        ssize_t bytes_received = recvfrom(udp_rx_socket, incoming_message, msg_alloc_size, 0, (struct sockaddr*)&client_address, &client_address_length);

        if (bytes_received <= 0)
        {
            close(udp_rx_socket);
            close(udp_tx_socket);
            perror("Failed to receive bytes");
            exit(EXIT_FAILURE);
        }

        switch (incoming_message->header.message_type)
        {
            case MESSAGE_UNDEFINED:
                printf("Received a packet from %s:%d -- Message: %s\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port), incoming_message->body);
                break;
            case MESSAGE_JOIN:
                handle_client_join(incoming_message->body, &client_address);
                break;
            case MESSAGE_QUIT:
                handle_client_quit(&client_address);
                break;
            case MESSAGE_CHAT:
                client_index = get_matching_client_index(&client_address);
                forward_message_to_clients(client_index, incoming_message->body, msg_format_text, false, true);
                break;
        }
    }

    printf("Terminating.\n");

    close(udp_rx_socket);
    close(udp_tx_socket);
    exit(EXIT_SUCCESS);
}
