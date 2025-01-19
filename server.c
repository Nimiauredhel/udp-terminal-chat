#include "server.h"

static int udp_rx_socket;
static int rx_port_int;
static char rx_port[ADDRESS_BUFF_LENGTH];

static int udp_tx_socket;

static ClientsData_t clients = {0};

static struct sockaddr_in local_address = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY };

static int8_t get_free_client_index()
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
            && clients.addresses[i].sin_addr.s_addr == address->sin_addr.s_addr)
        {
            return i;
        }
    }

    return -1;
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

            clients.connected[index] = true;
            clients.addresses[index] = *address;

            char port[ADDRESS_BUFF_LENGTH * 2 + 4];
            char *name;
            uint16_t port_int;
            strcpy(port, join_message);
            strtok_r(port, ":", &name);
            port_int = atoi(port);

            printf("Token one: %d\n", port_int);
            clients.addresses[index].sin_port = port_int;

            printf("Token two: %s\n", name);
            strcpy(clients.names[index], name);
            printf("%s (%s:%d) has joined the conversation.\n", clients.names[index], inet_ntoa(address->sin_addr), address->sin_port);
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
        printf("%s left the conversation.\n", clients.names[index]);
        clients.connected[index] = false;
    }
}

void forward_message_to_clients(int8_t sender_index, char *message)
{
    size_t message_length = strlen(message) + 1;

    for (int8_t index = 0; index < MAX_CLIENT_COUNT; index++)
    {
        if (/*index == sender_index || */!clients.connected[index]) continue;

        if (0 > sendto(udp_tx_socket, message, message_length, 0,
        (struct sockaddr *)&clients.addresses[sender_index], sizeof(clients.addresses[sender_index])))
        {
            close(udp_rx_socket);
            close(udp_tx_socket);
            perror("Failed to send message");
            exit(EXIT_FAILURE);
        }
    }
}

void server_init(void)
{
    printf("Server Init.\n");

    printf("Receive on port: ");
    fgets(rx_port, address_buff_length, stdin);
    rx_port[strcspn(rx_port, "\n")] = 0;

    if (strlen(rx_port) < 1)
    {
        strcpy(rx_port, "8080");
    }

    rx_port_int = atoi(rx_port);
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

    //bind the socket to the address/port
    int result = bind(udp_rx_socket, (struct sockaddr *)&local_address, sizeof(local_address));

    if (result < 0)
    {
        perror("Could not bind socket");
        exit(EXIT_FAILURE);
    }
}

void server_loop(void)
{
    Message_t incoming_message;
    char outgoing_buffer[MSG_BUFF_LENGTH + ADDRESS_BUFF_LENGTH + 4];

    static struct sockaddr_in client_address = { .sin_family = AF_INET };
    socklen_t client_address_length = sizeof(client_address);
    int8_t client_index = -1;

    printf("Server Loop.\n");

    while(true)
    {
        memset(&incoming_message, 0, sizeof(incoming_message));
        int bytes_received = recvfrom(udp_rx_socket, &incoming_message, sizeof(incoming_message), 0, (struct sockaddr*)&client_address, &client_address_length);

        if (bytes_received <= 0)
        {
            close(udp_rx_socket);
            close(udp_tx_socket);
            perror("Failed to receive bytes");
            exit(EXIT_FAILURE);
        }

        switch (incoming_message.header)
        {
            case MESSAGE_UNDEFINED:
                printf("Received a packet from %s:%d -- Message: %s\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port), incoming_message.body);
                break;
            case MESSAGE_JOIN:
                handle_client_join(incoming_message.body, &client_address);
                break;
            case MESSAGE_QUIT:
                handle_client_quit(&client_address);
                break;
            case MESSAGE_TEXT:
                client_index = get_matching_client_index(&client_address);

                if (client_index == -1)
                {
                    printf("Unknown: %s\n", incoming_message.body);
                }
                else
                {
                    printf("%s: %s\n", clients.names[client_index], incoming_message.body);
                }
                break;
        }
    }

    close(udp_rx_socket);
    close(udp_tx_socket);
    exit(EXIT_SUCCESS);
}
