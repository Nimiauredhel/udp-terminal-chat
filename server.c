#include "server.h"

static int udp_rx_socket;
static int rx_port_int;
static char rx_port[ADDRESS_BUFF_LENGTH];
static char msg_buffer[MSG_BUFF_LENGTH];

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
    fgets(rx_port, address_buff_length, stdin);
    rx_port[strcspn(rx_port, "\n")] = 0;
    rx_port_int = atoi(rx_port);
    local_address.sin_port = htons(rx_port_int);

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
        int bytes_received = recvfrom(udp_rx_socket, msg_buffer, msg_buff_length, 0, (struct sockaddr*)&peer_address, &peer_address_length);

        if (bytes_received <= 0)
        {
            close(udp_rx_socket);
            perror("Failed to receive bytes");
            exit(EXIT_FAILURE);
        }

        printf("Received a packet from %s:%d -- Message: %s\n", inet_ntoa(peer_address.sin_addr), ntohs(peer_address.sin_port), msg_buffer);
    }

    close(udp_rx_socket);
    exit(EXIT_SUCCESS);
}
