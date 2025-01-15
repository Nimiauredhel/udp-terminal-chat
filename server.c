#include "server.h"

#define MSG_MAX_LENGTH 1024
#define INPUT_MAX_LENGTH 32
static const uint16_t msg_max_length = MSG_MAX_LENGTH;

static int udp_rx_socket;
static int rx_port;
static char in_buffer[INPUT_MAX_LENGTH];
static char out_buffer[MSG_MAX_LENGTH];

static struct sockaddr_in peer_address =
{
    .sin_family = AF_INET
};

static struct sockaddr_in local_address =
{
    .sin_family = AF_INET,
    .sin_addr.s_addr = INADDR_ANY
};

void server_init(void)
{
    printf("Server Init.\n");

    printf("Receive on port: ");
    fgets(in_buffer, INPUT_MAX_LENGTH, stdin);
    in_buffer[strcspn(in_buffer, "\n")] = 0;
    rx_port = atoi(in_buffer);
    local_address.sin_port = htons(rx_port);

    if ((udp_rx_socket = socket(AF_INET, SOCK_DGRAM, 0)) <= 0)
    {
        perror("Failed to create socket");
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
    printf("Server Loop.\n");

    socklen_t peer_address_length = sizeof(peer_address);

    while(true)
    {
        int bytes_received = recvfrom(udp_rx_socket, out_buffer, msg_max_length, 0, (struct sockaddr*)&peer_address, &peer_address_length);

        if (bytes_received <= 0)
        {
            close(udp_rx_socket);
            perror("Failed to receive bytes");
            exit(EXIT_FAILURE);
        }

        printf("Received a packet from %s:%d -- Message: %s\n", inet_ntoa(peer_address.sin_addr), ntohs(peer_address.sin_port), out_buffer);
    }

    close(udp_rx_socket);
    exit(EXIT_SUCCESS);
}
