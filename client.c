#include "client.h"

static void client_init(ClientSideData_t *data);
static void client_create_rx_thread(ClientSideData_t *data);
static void client_tx_loop(ClientSideData_t *data) __attribute__ ((__noreturn__));
static void* client_rx_loop(void *arg);
static void client_send_message(ClientSideData_t *data);
static void client_error_negative(int test_value, int exit_value, char *context_message, ClientSideData_t *data);
static void client_error_zero(int test_value, int exit_value, char *context_message, ClientSideData_t *data);
static void client_terminate(ClientSideData_t *data, int exit_value) __attribute__ ((__noreturn__));

void client_start(void)
{
    ClientSideData_t *client_side_data = malloc(sizeof(ClientSideData_t));
    client_side_data->outgoing_message = malloc(MSG_ALLOC_SIZE);

    client_init(client_side_data);
    client_create_rx_thread(client_side_data);
    client_tx_loop(client_side_data);
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
        printf("Defaulting to port %d.\n", PORT_MIN);
        server_port_int = PORT_MIN;
    }

    data->server_address.sin_port = htons(server_port_int);
    client_error_zero(inet_pton(AF_INET, server_ip, &(data->server_address.sin_addr)), EINVAL, "Bad IP address", data);

    data->udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    client_error_negative(data->udp_socket, EINVAL, "Error in socket creation", data);

    //bind the socket to the address/port
    uint32_t rx_port = PORT_MIN - 1;
    int bind_result = -1;

    // try range of usable ports
    while(bind_result < 0 && rx_port <= PORT_MAX)
    {
        rx_port++;
        data->local_address.sin_port = htons(rx_port);
        bind_result = bind(data->udp_socket, (struct sockaddr *)&(data->local_address), sizeof(data->local_address));
    }

    // if all 16384 ports fail, give up
    client_error_negative(bind_result, EXIT_FAILURE, "Could not bind socket", data);

    // set the socket rx timeout
    struct timeval socket_timeout;
    socket_timeout.tv_sec = 0;
    socket_timeout.tv_usec = 500000;
    client_error_negative(setsockopt(data->udp_socket, SOL_SOCKET, SO_RCVTIMEO, &socket_timeout, sizeof(socket_timeout)),
            EXIT_FAILURE, "Failed to set socket rx timeout", data);

    printf("Client will receive on port %d.\nInput client name: ", rx_port);

    fgets(data->client_name, NAME_BUFF_LENGTH, stdin);
    data->client_name[strcspn(data->client_name, "\n")] = 0;

    if (strlen(data->client_name) < 1)
    {
        strcpy(data->client_name, "Client");
    }

    printf("\n%s (port %u) connecting to server (%s:%u).\n",
            data->client_name, ntohs(data->local_address.sin_port),
            inet_ntoa(data->server_address.sin_addr),
            ntohs(data->server_address.sin_port));
}

static void client_create_rx_thread(ClientSideData_t *data)
{
    pthread_attr_t rx_thread_attributes;
    pthread_attr_init(&rx_thread_attributes);
    pthread_attr_setstacksize(&rx_thread_attributes, PTHREAD_STACK_MIN);
    pthread_create(&(data->rx_thread), &rx_thread_attributes, &client_rx_loop, data);
}

static void client_tx_loop(ClientSideData_t *data)
{
    fd_set input_set;
    int has_input;
    struct timeval timeout;

    // initialize the message data structure & text buffer
    memset(data->outgoing_message, 0, MSG_ALLOC_SIZE);
    data->outgoing_message->header.encoding_version = htons(ENCODING_VERSION);

    // send initial server join message
    memset(data->outgoing_message->body, 0, MSG_BUFF_LENGTH);
    data->outgoing_message->header.message_type = htons(MESSAGE_JOIN);
    client_send_message(data);

    printf("Client initialized. Enter a message or Ctrl-C to quit.\nMessage: ");
    fflush(stdout);

    while(!should_terminate)
    {
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;
        FD_ZERO(&input_set);
        FD_SET(STDIN_FILENO, &input_set);

        memset(data->outgoing_message->body, 0, MSG_BUFF_LENGTH);

        // wait for user input until timeout
        has_input = select(1, &input_set, NULL, NULL, &timeout);

        if (should_terminate) break;

        // if input detected, prepare to send it as a chat message
        if (has_input)
        {
            fgets(data->outgoing_message->body, MSG_BUFF_LENGTH, stdin);
            data->outgoing_message->body[strcspn(data->outgoing_message->body, "\n")] = 0;
            data->outgoing_message->header.message_type = htons(MESSAGE_CHAT);
        }
        // if timed out, send a "stay" message to let the server know we are still online
        else
        {
            data->outgoing_message->header.message_type = htons(MESSAGE_STAY);
        }

        client_send_message(data);

        if (has_input)
        {
            printf("Message: ");
            fflush(stdout);
        }
    }

    client_terminate(data, EXIT_SUCCESS);
}

static void* client_rx_loop(void *arg)
{
    ClientSideData_t *data = (ClientSideData_t *)arg;

    struct sockaddr_in incoming_address = { .sin_family = AF_INET };
    char incoming_buffer[sizeof(Message_t) + MSG_BUFF_LENGTH];
    socklen_t incoming_address_length = sizeof(incoming_address);

    while (!should_terminate)
    {
        memset(&incoming_buffer, 0, sizeof(incoming_buffer));
        int bytes_received = recvfrom(data->udp_socket, &incoming_buffer, sizeof(incoming_buffer), 0, (struct sockaddr*)&incoming_address, &incoming_address_length);

        if (should_terminate) break;

        if ((incoming_address.sin_addr.s_addr != data->server_address.sin_addr.s_addr)
            ||(bytes_received <= 0 && errno == EWOULDBLOCK))
            continue;

        client_error_zero(bytes_received, EXIT_FAILURE, "Failed to receive bytes", data);

        if (should_terminate) break;
        if (strlen(incoming_buffer) < 2) continue;

        printf("\b\b\b\b\b\b\b\b\b%s\nMessage: ", incoming_buffer);
        fflush(stdout);
    }

    return NULL;
}

static void client_send_message(ClientSideData_t *data)
{
    uint16_t msg_body_length = 0;

    switch (ntohs(data->outgoing_message->header.message_type)) 
    {
    case MESSAGE_UNDEFINED:
        printf("*** Attempted sending message with undefined type. Aborting.");
        break;
    case MESSAGE_ERROR:
    case MESSAGE_RAW:
    case MESSAGE_CHAT:
        msg_body_length = strlen(data->outgoing_message->body);

        if (msg_body_length <= 0)
        {
            // empty buffer, move the cursor a line up and do not send a message
            printf( "\x1B[1F");
            return;
        }
        else
        {
            break;
        }
    case MESSAGE_JOIN:
    case MESSAGE_QUIT:
    case MESSAGE_STAY:
        sprintf(data->outgoing_message->body, "%u:%s", ntohs(data->local_address.sin_port), data->client_name);
        msg_body_length = strlen(data->outgoing_message->body);
        break;
    }

    data->outgoing_message->header.body_length = htons(msg_body_length);
    data->outgoing_message->header.timestamp = htons(time(NULL));

    if (data->outgoing_message->header.message_type == MESSAGE_QUIT)
    {
        sendto(data->udp_socket, data->outgoing_message, sizeof(Message_t) + msg_body_length + 1, 0,
        (struct sockaddr *)&(data->server_address), sizeof(data->server_address));
    }
    else
    {
        client_error_negative(sendto(data->udp_socket, data->outgoing_message, sizeof(Message_t) + msg_body_length + 1, 0,
        (struct sockaddr *)&(data->server_address), sizeof(data->server_address)), EXIT_FAILURE, "Failed to send message", data);
    }
}

static void client_terminate(ClientSideData_t *data, int exit_value)
{
    // settings the flag just in case,
    // and waiting for the thread to exit
    should_terminate = true;
    pthread_join(data->rx_thread, NULL);

    // print message if terminated by user
    if (exit_value == 0)
    {
        printf("Client terminated by user. Goodbye!\n");
    }

    // send a quit message
    data->outgoing_message->header.message_type = htons(MESSAGE_QUIT);
    client_send_message(data);

    // cleanup and exit
    close(data->udp_socket);
    free(data->outgoing_message);
    free(data);
    exit(exit_value);
}

static void client_error_negative(int test_value, int exit_value, char *context_message, ClientSideData_t *data)
{
    if (test_value < 0)
    {
        perror(context_message);
        client_terminate(data, exit_value);
    }
}

static void client_error_zero(int test_value, int exit_value, char *context_message, ClientSideData_t *data)
{
    client_error_negative(test_value - 1, exit_value, context_message, data);
}
