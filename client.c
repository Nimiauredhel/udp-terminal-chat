#include "client.h"

#define MSG_MAX_LENGTH 1024
#define INPUT_MAX_LENGTH 32
static const uint16_t msg_max_length = MSG_MAX_LENGTH;

static int udp_socket;
static int peer_port;
static char peer_ip[INPUT_MAX_LENGTH];
static char in_buffer[INPUT_MAX_LENGTH];
static char out_buffer[MSG_MAX_LENGTH];

static struct sockaddr_in peer_address =
{
    .sin_family = AF_INET
};

void client_init(void)
{
    printf("Client Init.\n");

    printf("Peer IP: ");
    fgets(peer_ip, INPUT_MAX_LENGTH, stdin);
    peer_ip[strcspn(peer_ip, "\n")] = 0;

    printf("Peer port: ");
    fgets(in_buffer, INPUT_MAX_LENGTH, stdin);
    in_buffer[strcspn(in_buffer, "\n")] = 0;
    peer_port = atoi(in_buffer);
    peer_address.sin_port = htons(peer_port);

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
        fgets(out_buffer, msg_max_length, stdin);
        out_buffer[strcspn(out_buffer, "\n")] = 0;
        msg_length = strlen(out_buffer);

        if (msg_length == 0) break;

        if (0 > sendto(udp_socket, out_buffer, msg_length + 1, 0,
        (struct sockaddr *)&peer_address, sizeof(peer_address)))
        {
            close(udp_socket);
            perror("Failed to send message");
            exit(EXIT_FAILURE);
        }

        printf("Sent message: %s\nTo %s:%d\n", out_buffer, peer_ip, peer_port);
    }

    close(udp_socket);
    printf("Goodbye!\n");
    exit(EXIT_SUCCESS);
}
