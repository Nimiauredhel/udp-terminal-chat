#include "client.h"

static void client_error_negative(int test_value, int exit_value, char *context_message, ClientSideData_t *data)
{
    if (test_value < 0)
    {
        perror(context_message);
        close(data->udp_socket);
        free(data);
        exit(exit_value);
    }
}

static void client_error_zero(int test_value, int exit_value, char *context_message, ClientSideData_t *data)
{
    client_error_negative(test_value - 1, exit_value, context_message, data);
}

static void* client_listen(void *arg)
{
    ClientSideData_t *data = (ClientSideData_t *)arg;

    struct sockaddr_in incoming_address = { .sin_family = AF_INET };
    char incoming_buffer[sizeof(Message_t) + MSG_BUFF_LENGTH];
    socklen_t incoming_address_length = sizeof(incoming_address);

    while (true)
    {
        pthread_testcancel();

        memset(&incoming_buffer, 0, sizeof(incoming_buffer));
        int bytes_received = recvfrom(data->udp_socket, &incoming_buffer, sizeof(incoming_buffer), 0, (struct sockaddr*)&incoming_address, &incoming_address_length);

        pthread_testcancel();

        if (incoming_address.sin_addr.s_addr != data->server_address.sin_addr.s_addr)
            continue;

        client_error_zero(bytes_received, EXIT_FAILURE, "Failed to receive bytes", data);

        if (strlen(incoming_buffer) < 2) continue;

        pthread_testcancel();

        printf("\b\b\b\b\b\b\b\b\b%s\nMessage: ", incoming_buffer);
        fflush(stdout);
    }

    return NULL;
}

static void client_init(ClientSideData_t *data)
{
    char server_ip[ADDRESS_BUFF_LENGTH];
    char server_port[PORT_BUFF_LENGTH];

    uint16_t server_port_int;

    data->server_address.sin_family = AF_INET;
    data->local_address.sin_family = AF_INET;
    data->local_address.sin_addr.s_addr = INADDR_ANY;

    printf("Client mode initializing.\nInput server IP: ");

    fgets(server_ip, ADDRESS_BUFF_LENGTH, stdin);
    server_ip[strcspn(server_ip, "\n")] = 0;

    if (strlen(server_ip) < 7)
    {
        sprintf(server_ip, "%s", "127.0.0.1");
    }

    printf("Input server port: ");

    fgets(server_port, PORT_BUFF_LENGTH, stdin);
    server_port[strcspn(server_port, "\n")] = 0;
    server_port_int = atoi(server_port);

    if (server_port_int <= 1024)
    {
        printf("Defaulting to port 8080.\n");
        server_port_int = 8080;
    }

    data->server_address.sin_port = htons(server_port_int);
    client_error_zero(inet_pton(AF_INET, server_ip, &(data->server_address.sin_addr)), EINVAL, "Bad IP address", data);

    data->udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    client_error_negative(data->udp_socket, EINVAL, "Error in socket creation", data);

    //bind the socket to the address/port
    uint16_t rx_port = 8079;
    int bind_result = -1;

    // try range between 8080 and 8100
    while(bind_result < 0 && rx_port <= 8100)
    {
        rx_port++;
        data->local_address.sin_port = htons(rx_port);
        bind_result = bind(data->udp_socket, (struct sockaddr *)&(data->local_address), sizeof(data->local_address));
    }

    // if all 20 ports fail, give up
    client_error_negative(bind_result, EXIT_FAILURE, "Could not bind socket", data);

    printf("Client name: ");
    char client_name[NAME_BUFF_LENGTH];

    fgets(client_name, NAME_BUFF_LENGTH, stdin);
    client_name[strcspn(client_name, "\n")] = 0;

    if (strlen(client_name) < 1)
    {
        strncpy(client_name, "Client", NAME_BUFF_LENGTH);
    }

    strcpy(data->client_name, client_name);

    printf("\n%s (port %u) connecting to server (%s:%u).\n",
            data->client_name, ntohs(data->local_address.sin_port),
            inet_ntoa(data->server_address.sin_addr),
            ntohs(data->server_address.sin_port));
}

static void client_loop(ClientSideData_t *data)
{
    fd_set input_set;
    int has_input;
    struct timeval timeout;
    uint16_t msg_body_length;

    Message_t *outgoing_message = malloc(MSG_ALLOC_SIZE);

    printf("Message: ");
    fflush(stdout);

    pthread_t rx_thread;
    pthread_create(&rx_thread, NULL, &client_listen, data);

    while(true)
    {
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;
        FD_ZERO(&input_set);
        FD_SET(STDIN_FILENO, &input_set);

        memset(outgoing_message, 0, MSG_ALLOC_SIZE);

        has_input = select(1, &input_set, NULL, NULL, &timeout);

        if (has_input)
        {
            fgets(outgoing_message->body, MSG_BUFF_LENGTH, stdin);
            outgoing_message->body[strcspn(outgoing_message->body, "\n")] = 0;
            outgoing_message->header.message_type = MESSAGE_CHAT;
        }
        else
        {
            sprintf(outgoing_message->body, "%u:%s", data->local_address.sin_port, data->client_name);
            outgoing_message->header.message_type = MESSAGE_JOIN;
        }

        msg_body_length = strlen(outgoing_message->body);

        if (msg_body_length <= 0) break;

        outgoing_message->header.encoding_version = ENCODING_VERSION;
        outgoing_message->header.body_length = msg_body_length;
        outgoing_message->header.timestamp = time(NULL);

        client_error_negative(sendto(data->udp_socket, outgoing_message, sizeof(Message_t) + msg_body_length + 1, 0,
        (struct sockaddr *)&(data->server_address), sizeof(data->server_address)), EXIT_FAILURE, "Failed to send message", data);

        if (has_input)
        {
            printf("Message: ");
            fflush(stdout);
        }
    }

    // termination by user
    printf("Goodbye!\n");

    // terminate listen thread
    pthread_cancel(rx_thread);
    pthread_join(rx_thread, NULL);

    // send "leaving" message
    memset(outgoing_message, 0, MSG_ALLOC_SIZE);
    strncpy(outgoing_message->body, data->client_name, NAME_BUFF_LENGTH);
    msg_body_length = strlen(outgoing_message->body);

    outgoing_message->header.encoding_version = ENCODING_VERSION;
    outgoing_message->header.body_length = msg_body_length;
    outgoing_message->header.timestamp = time(NULL);
    outgoing_message->header.message_type = MESSAGE_QUIT;

    sendto(data->udp_socket, outgoing_message, sizeof(Message_t) + msg_body_length + 1, 0,
    (struct sockaddr *)&(data->server_address), sizeof(data->server_address));

    // cleanup and exit
    free(outgoing_message);
    close(data->udp_socket);
    free(data);
    exit(EXIT_SUCCESS);
}

void client_start(void)
{
    ClientSideData_t *client_side_data = malloc(sizeof(ClientSideData_t));
    client_init(client_side_data);
    client_loop(client_side_data);
}
