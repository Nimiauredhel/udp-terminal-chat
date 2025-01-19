#include "client.h"
#include "sys/time.h"
#include "sys/types.h"
#include "pthread.h"

static int udp_socket;
static int peer_port_int;
static char peer_ip[ADDRESS_BUFF_LENGTH];
static char peer_port[ADDRESS_BUFF_LENGTH];
static char client_name[ADDRESS_BUFF_LENGTH];
static Message_t outgoing_message;
static char incoming_buffer[ADDRESS_BUFF_LENGTH + MSG_BUFF_LENGTH + 4];

static struct sockaddr_in peer_address = { .sin_family = AF_INET };
static struct sockaddr_in local_address = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY };
socklen_t peer_address_length = sizeof(peer_address);

static void* client_listen(void *arg)
{
    while (true)
    {
        memset(&incoming_buffer, 0, sizeof(incoming_buffer));
        int bytes_received = recvfrom(udp_socket, &incoming_buffer, sizeof(incoming_buffer), 0, (struct sockaddr*)&peer_address, &peer_address_length);

        if (bytes_received <= 0)
        {
            close(udp_socket);
            perror("Failed to receive bytes");
            exit(EXIT_FAILURE);
        }

        printf("\b\b\b\b\b\b\b\n%s\nMessage:", incoming_buffer);
    }
}

void client_init(void)
{
    printf("Client Init.\n");

    printf("Peer IP: ");
    fgets(peer_ip, address_buff_length, stdin);
    peer_ip[strcspn(peer_ip, "\n")] = 0;

    if (strlen(peer_ip) < 7)
    {
        strcpy(peer_ip, "127.0.0.1");
    }

    printf("Peer port: ");
    fgets(peer_port, address_buff_length, stdin);
    peer_port[strcspn(peer_port, "\n")] = 0;

    if (strlen(peer_port) < 1)
    {
        strcpy(peer_port, "8080");
    }

    peer_port_int = atoi(peer_port);
    peer_address.sin_port = htons(peer_port_int);
    local_address.sin_port = htons(peer_port_int);

    if (inet_pton(AF_INET, peer_ip, &(peer_address.sin_addr)) <= 0)
    {
        perror("Bad IP address");
        exit(EINVAL);
    }

    udp_socket = socket(AF_INET, SOCK_DGRAM, 0);

    if (udp_socket < 0)
    {
        perror("Error in socket creation");
        exit(EINVAL);
    }

    //bind the socket to the address/port
    if (bind(udp_socket, (struct sockaddr *)&local_address, sizeof(local_address)) < 0)
    {
        perror("Could not bind socket");
        exit(EXIT_FAILURE);
    }

    printf("Client name: ");
    fgets(client_name, address_buff_length, stdin);
    client_name[strcspn(client_name, "\n")] = 0;

    if (strlen(client_name) < 1)
    {
        strcpy(client_name, "Client");
    }
}

void client_loop(void)
{
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
            msg_length = strlen(outgoing_message.body) + 4;
            outgoing_message.header = MESSAGE_TEXT;
        }
        else
        {
            strcpy(outgoing_message.body, client_name);
            msg_length = strlen(client_name) + 4;
            outgoing_message.header = MESSAGE_JOIN;
        }

        if (msg_length <= 4) break;

        if (0 > sendto(udp_socket, &outgoing_message, msg_length + 1, 0,
        (struct sockaddr *)&peer_address, sizeof(peer_address)))
        {
            close(udp_socket);
            perror("Failed to send message");
            exit(EXIT_FAILURE);
        }

        if (has_input)
        {
            printf("Message: ");
            fflush(stdout);
        }

        //printf("Sent message: %s\nTo %s:%d\nMessage:", msg_buffer, peer_ip, peer_port_int);
    }

    // send "leaving" message
    memset(&outgoing_message, 0, sizeof(outgoing_message));
    strcpy(outgoing_message.body, client_name);
    msg_length = strlen(client_name) + 4;
    outgoing_message.header = MESSAGE_QUIT;
    sendto(udp_socket, &outgoing_message, msg_length + 1, 0,
    (struct sockaddr *)&peer_address, sizeof(peer_address));

    close(udp_socket);
    printf("Goodbye!\n");
    exit(EXIT_SUCCESS);
}
