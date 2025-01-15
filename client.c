#include "client.h"

static int udp_socket;
static int peer_port_int;
static char peer_ip[ADDRESS_BUFF_LENGTH];
static char peer_port[ADDRESS_BUFF_LENGTH];
static char msg_buffer[MSG_BUFF_LENGTH];

static struct sockaddr_in peer_address =
{
    .sin_family = AF_INET
};

void client_init(void)
{
    printf("Client Init.\n");

    printf("Peer IP: ");
    fgets(peer_ip, address_buff_length, stdin);
    peer_ip[strcspn(peer_ip, "\n")] = 0;

    printf("Peer port: ");
    fgets(peer_port, address_buff_length, stdin);
    peer_port[strcspn(peer_port, "\n")] = 0;
    peer_port_int = atoi(peer_port);
    peer_address.sin_port = htons(peer_port_int);

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
}

void client_loop(void)
{
    uint16_t msg_length;

    while(true)
    {
        printf("Message: ");
        fgets(msg_buffer, msg_buff_length, stdin);
        msg_buffer[strcspn(msg_buffer, "\n")] = 0;
        msg_length = strlen(msg_buffer);

        if (msg_length == 0) break;

        if (0 > sendto(udp_socket, msg_buffer, msg_length + 1, 0,
        (struct sockaddr *)&peer_address, sizeof(peer_address)))
        {
            close(udp_socket);
            perror("Failed to send message");
            exit(EXIT_FAILURE);
        }

        printf("Sent message: %s\nTo %s:%d\n", msg_buffer, peer_ip, peer_port_int);
    }

    close(udp_socket);
    printf("Goodbye!\n");
    exit(EXIT_SUCCESS);
}
