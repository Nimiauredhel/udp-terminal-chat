#include "client.h"

static int udp_tx_socket;
static int udp_rx_socket;

static char client_name[ADDRESS_BUFF_LENGTH];

static struct sockaddr_in server_address = { .sin_family = AF_INET };
static struct sockaddr_in local_address = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY };

static void* client_listen(void *arg)
{
    struct sockaddr_in incoming_address = { .sin_family = AF_INET };
    char incoming_buffer[ADDRESS_BUFF_LENGTH + MSG_BUFF_LENGTH + 4];
    socklen_t incoming_address_length = sizeof(incoming_address);

    while (true)
    {
        memset(&incoming_buffer, 0, sizeof(incoming_buffer));
        int bytes_received = recvfrom(udp_rx_socket, &incoming_buffer, sizeof(incoming_buffer), 0, (struct sockaddr*)&incoming_address, &incoming_address_length);

        if (bytes_received <= 0)
        {
            close(udp_rx_socket);
            close(udp_tx_socket);
            perror("Failed to receive bytes");
            exit(EXIT_FAILURE);
        }

        printf("\b\b\b\b\b\b\b\b\b%s\nMessage: ", incoming_buffer);
        fflush(stdout);
    }
}

void client_init(void)
{
    char server_ip[ADDRESS_BUFF_LENGTH];
    char server_port[ADDRESS_BUFF_LENGTH];
    uint16_t server_port_int;

    printf("Client mode initializing.\nInput server IP: ");

    fgets(server_ip, address_buff_length, stdin);
    server_ip[strcspn(server_ip, "\n")] = 0;

    if (strlen(server_ip) < 7)
    {
        sprintf(server_ip, "%s", "127.0.0.1");
    }

    printf("Input server port: ");

    fgets(server_port, address_buff_length, stdin);
    server_port[strcspn(server_port, "\n")] = 0;
    server_port_int = atoi(server_port);

    if (server_port_int <= 1024)
    {
        printf("Defaulting to port 8080.\n");
        server_port_int = 8080;
    }

    server_address.sin_port = htons(server_port_int);

    if (inet_pton(AF_INET, server_ip, &(server_address.sin_addr)) <= 0)
    {
        perror("Bad IP address");
        exit(EINVAL);
    }

    udp_tx_socket = socket(AF_INET, SOCK_DGRAM, 0);

    if (udp_tx_socket < 0)
    {
        perror("Error in socket creation");
        exit(EINVAL);
    }

    udp_rx_socket = socket(AF_INET, SOCK_DGRAM, 0);

    if (udp_rx_socket < 0)
    {
        perror("Error in socket creation");
        exit(EINVAL);
    }


    //bind the socket to the address/port
    uint16_t rx_port = 8079;
    int bind_result = -1;

    // try range between 8080 and 8100
    while(bind_result < 0 && rx_port <= 8100)
    {
        rx_port++;
        local_address.sin_port = htons(rx_port);
        bind_result = bind(udp_rx_socket, (struct sockaddr *)&local_address, sizeof(local_address));
    }

    // if all 20 ports fail, give up
    if (bind_result < 0)
    {
        perror("Could not bind socket");
        exit(EXIT_FAILURE);
    }

    printf("Client name: ");
    fgets(client_name, address_buff_length, stdin);
    client_name[strcspn(client_name, "\n")] = 0;

    if (strlen(client_name) < 1)
    {
        sprintf(client_name, "%s", "Client");
    }

    printf("\n%s (%s:%u) connecting to server (%s:%u).\n",
            client_name, inet_ntoa(local_address.sin_addr),
            ntohs(local_address.sin_port),
            inet_ntoa(server_address.sin_addr),
            ntohs(server_address.sin_port));
}

void client_loop(void)
{
    Message_t outgoing_message;
    fd_set input_set;
    int has_input;
    struct timeval timeout;
    uint16_t msg_length;

    printf("Message: ");
    fflush(stdout);

    pthread_t rx_thread;
    pthread_create(&rx_thread, NULL, &client_listen, NULL);

    while(true)
    {
        timeout.tv_sec = 0;
        timeout.tv_usec = 250000;
        FD_ZERO(&input_set);
        FD_SET(STDIN_FILENO, &input_set);

        memset(&outgoing_message, 0, sizeof(outgoing_message));

        has_input = select(1, &input_set, NULL, NULL, &timeout);

        if (has_input)
        {
            fgets(outgoing_message.body, msg_buff_length, stdin);
            outgoing_message.body[strcspn(outgoing_message.body, "\n")] = 0;
            outgoing_message.header = MESSAGE_TEXT;
        }
        else
        {
            sprintf(outgoing_message.body, "%u:%s", local_address.sin_port, client_name);
            outgoing_message.header = MESSAGE_JOIN;
        }

        msg_length = strlen(outgoing_message.body) + 4;

        if (msg_length <= 4) break;

        if (0 > sendto(udp_rx_socket, &outgoing_message, msg_length, 0,
        (struct sockaddr *)&server_address, sizeof(server_address)))
        {
            close(udp_tx_socket);
            close(udp_rx_socket);
            perror("Failed to send message");
            exit(EXIT_FAILURE);
        }

        if (has_input)
        {
            printf("Message: ");
            fflush(stdout);
        }
    }

    // send "leaving" message
    memset(&outgoing_message, 0, sizeof(outgoing_message));
    sprintf(outgoing_message.body, "%s", client_name);
    msg_length = strlen(outgoing_message.body) + 4;
    outgoing_message.header = MESSAGE_QUIT;

    pthread_cancel(rx_thread);

    sendto(udp_tx_socket, &outgoing_message, msg_length, 0,
    (struct sockaddr *)&server_address, sizeof(server_address));

    close(udp_tx_socket);
    close(udp_rx_socket);
    printf("Goodbye!\n");
    exit(EXIT_SUCCESS);
}
