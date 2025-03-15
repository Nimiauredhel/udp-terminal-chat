#include "client.h"

static void client_init(ClientSideData_t *data);
static void client_msglist_init(ClientMsgList_t *list);
static void client_msglist_push(ClientMsgList_t *list, Message_t *msg);
static void client_peerlist_init(ClientPeerList_t *peers);
static void client_peerlist_update(ClientPeerList_t *peers, Message_t *update_msg);
static void client_create_rx_thread(ClientSideData_t *data);
static void client_create_render_thread(ClientSideData_t *data);
static void client_tx_loop(ClientSideData_t *data) __attribute__ ((__noreturn__));
static void* client_rx_loop(void *arg);
static void* client_render_loop(void *arg);
static void client_input_render(ClientInputState_t *input, ClientTUIData_t *tui);
static void client_session_render(ClientSessionData_t *session, ClientTUIData_t *tui);
static void client_peerlist_render(ClientPeerList_t *peers, ClientTUIData_t *tui);
static void client_msglist_render(ClientMsgList_t *list, ClientTUIData_t *tui);
static void client_send_message(ClientSideData_t *data);
static void client_error_negative(int test_value, int exit_value, char *context_message, ClientSideData_t *data);
static void client_error_zero(int test_value, int exit_value, char *context_message, ClientSideData_t *data);
static void client_terminate(ClientSideData_t *data, int exit_value) __attribute__ ((__noreturn__));

void client_start(void)
{
    initscr();
    initialize_signal_handler();

    ClientSideData_t *client_side_data = malloc(sizeof(ClientSideData_t));

    client_init(client_side_data);
    client_msglist_init(&client_side_data->msg_list);
    client_peerlist_init(&client_side_data->peer_list);

    usleep(500);

    client_create_render_thread(client_side_data);
    client_create_rx_thread(client_side_data);
    client_tx_loop(client_side_data);
}

static void client_init(ClientSideData_t *data)
{
    pthread_mutex_init(&data->input.lock, NULL);
    pthread_mutex_init(&data->session.lock, NULL);

    char server_ip[ADDRESS_BUFF_LENGTH];
    char server_port[PORT_BUFF_LENGTH];
    uint16_t server_port_int;

    data->server_address.sin_family = AF_INET;
    data->local_address.sin_family = AF_INET;
    data->local_address.sin_addr.s_addr = INADDR_ANY;

    printw("Client mode initializing.\nInput server IP: ");
    refresh();

    getnstr(server_ip, ADDRESS_BUFF_LENGTH);
    server_ip[strcspn(server_ip, "\n")] = 0;

    if (strlen(server_ip) < 7)
    {
        sprintf(server_ip, "%s", "127.0.0.1");
    }

    printw("Input server port: ");
    refresh();

    getnstr(server_port, PORT_BUFF_LENGTH);
    server_port[strcspn(server_port, "\n")] = 0;
    server_port_int = atoi(server_port);

    if (server_port_int <= 1024)
    {
        printw("Defaulting to port %d.\n", PORT_MIN);
        refresh();
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

    printw("Client will receive on port %d.\nInput client name: ", rx_port);
    refresh();

    getnstr(data->client_name, NAME_BUFF_LENGTH);
    data->client_name[strcspn(data->client_name, "\n")] = 0;

    if (strlen(data->client_name) < 1)
    {
        strcpy(data->client_name, "Client");
    }

    printw("\n%s (port %u) connecting to server (%s:%u).\n",
            data->client_name, ntohs(data->local_address.sin_port),
            inet_ntoa(data->server_address.sin_addr),
            ntohs(data->server_address.sin_port));
    refresh();
}

static void client_msglist_init(ClientMsgList_t *list)
{
    // msg list is always initialized with a dummy head,
    // to completely avoid a single-use init branch in the push function
    explicit_bzero(list, sizeof(ClientMsgList_t));

    pthread_mutex_init(&list->lock, NULL);
    list->head = 0;
    list->tail = 0;

    // "hard" dummy
    strcpy(list->msgs[0], "~~~~--------~~~~");

    // "soft" test dummy
    Message_t test_msg =
    {
        .header =
        {
            .message_type = htons(MESSAGE_CHAT),
            .timestamp = htonl(time(NULL)),
            .body_length = 16,
            .encoding_version = htons(ENCODING_VERSION),
            .sender_name = "APP",
            .sender_index = 255
        },
        .body = "testTESTtestTEST"
    };

    client_msglist_push(list, &test_msg);
}

static void client_msglist_push(ClientMsgList_t *list, Message_t *msg)
{
    int new_idx = list->tail + 1;
    pthread_mutex_lock(&list->lock);

    // no need to check if head/tail exist since we always init with a dummy

    // ring buffer wrapping around when reaching end of array
    if (new_idx >= CLIENT_MAX_VISIBLE_MSGS) new_idx = 0;

    // full ring-buffer: tail overwriting and incrementing head 
    if (new_idx == list->head)
    {
        list->head++;
        if (list->head >= CLIENT_MAX_VISIBLE_MSGS) list->head = 0;            
    }

    list->tail = new_idx;
    explicit_bzero(list->msgs[new_idx], MSG_REP_MAX_CHARS);
    format_message(msg->body, msg->header.sender_name, ntohl(msg->header.timestamp), get_format_by_message_type(ntohs(msg->header.message_type)),
            list->msgs[new_idx]);
    list->user_indices[new_idx] = msg->header.sender_index;
    list->dirty = true;

    pthread_mutex_unlock(&list->lock);
}

static void client_peerlist_init(ClientPeerList_t *peers)
{
    explicit_bzero(peers, sizeof(ClientPeerList_t));
    pthread_mutex_init(&peers->lock, NULL);
}

static void client_peerlist_update(ClientPeerList_t *peers, Message_t *update_msg)
{
    UserStatus_t new_status = USTATUS_NONE;

    pthread_mutex_lock(&peers->lock);

    switch((MessageType_t)ntohs(update_msg->header.message_type))
    {
        case MESSAGE_JOIN:
        case MESSAGE_USERDATA:
            new_status = (UserStatus_t)atoi(update_msg->body);
            strncpy(peers->names[update_msg->header.sender_index], update_msg->header.sender_name, NAME_BUFF_LENGTH);
            break;
        case MESSAGE_QUIT:
            new_status = USTATUS_NONE;
            explicit_bzero(peers->names[update_msg->header.sender_index], NAME_BUFF_LENGTH);
            break;
        case MESSAGE_UNDEFINED:
        case MESSAGE_CHAT:
        case MESSAGE_RAW:
        case MESSAGE_STAY:
        case MESSAGE_ERROR:
            break;
    }

    peers->status_flags[update_msg->header.sender_index] = new_status;
    peers->dirty = true;

    pthread_mutex_unlock(&peers->lock);
}

static void client_input_render(ClientInputState_t *input, ClientTUIData_t *tui)
{
    pthread_mutex_lock(&input->lock);

    if (input->dirty || tui->dirty_size)
    {
        input->dirty = false;
        mvwin(tui->win_input, tui->rows*0.85f, tui->cols*0.1f); // whole bottom
        wresize(tui->win_input, tui->rows*0.15f, tui->cols*0.8f);
        wclear(tui->win_input);
        box(tui->win_input, 0, 0);
        mvwprintw(tui->win_input, 0, 0, "Input");

        if (input->idx > 0)
        {
            mvwprintw(tui->win_input, 1, 1, "%s", input->buff);
        }

        wrefresh(tui->win_input);
    }

    pthread_mutex_unlock(&input->lock);
}

static void client_session_render(ClientSessionData_t *session, ClientTUIData_t *tui)
{
    pthread_mutex_lock(&session->lock);

    if (session->dirty || tui->dirty_size)
    {
        session->dirty = false;
        mvwin(tui->win_session, tui->rows*0.05f, tui->cols*0.02f); // top left
        wresize(tui->win_session, tui->rows*0.20f, tui->cols*0.22f);
        wclear(tui->win_session);
        box(tui->win_session, 0, 0);
        mvwprintw(tui->win_session, 0, 0, "Session");
        wrefresh(tui->win_session);
    }

    pthread_mutex_unlock(&session->lock);
}

static void client_peerlist_render(ClientPeerList_t *peers, ClientTUIData_t *tui)
{
    pthread_mutex_lock(&peers->lock);

    if (peers->dirty || tui->dirty_size)
    {
        peers->dirty = false;
        mvwin(tui->win_users, tui->rows*0.25f, tui->cols*0.02f); // middle to bottom left
        wresize(tui->win_users, tui->rows*0.45f, tui->cols*0.22f);
        wclear(tui->win_users);
        box(tui->win_users, 0, 0);
        mvwprintw(tui->win_users, 0, 0, "Users");

        uint8_t row = 1;

        for (uint8_t i = 0; i < MAX_CLIENT_COUNT; i++)
        {
            if (peers->status_flags[i] > USTATUS_NONE)
            {
                short pair = peers->status_flags[i];
                wattron(tui->win_users, COLOR_PAIR(pair));
                mvwprintw(tui->win_users, row, 2*peers->status_flags[i],
                peers->status_flags[i] == USTATUS_TYPING ? "%s..."
                : peers->status_flags[i] == USTATUS_HOT ?  "%s!"
                : peers->status_flags[i] == USTATUS_IDLE ?  "%s zZz"
                : "%s", peers->names[i]);
                wattroff(tui->win_users, COLOR_PAIR(pair));
                row++;
            }
        }

        wrefresh(tui->win_users);
    }

    pthread_mutex_unlock(&peers->lock);
}

static void client_msglist_render(ClientMsgList_t *list, ClientTUIData_t *tui)
{
    pthread_mutex_lock(&list->lock);
    
    if (tui->dirty_size || list->dirty)
    {
        list->dirty = false;
        mvwin(tui->win_messages, tui->rows*0.05f, tui->cols*0.25f); // middle to left
        wresize(tui->win_messages, tui->rows*0.8f, tui->cols*0.75f);
        wclear(tui->win_messages);
        box(tui->win_messages, 0, 0);
        mvwprintw(tui->win_messages, 0, 0, "Messages");

        // starting at head minus one since loop increments before use
        int current = list->head - 1;
        // failsafe counter in case we have a loop!
        int counter = 0;
        int row = 0;
        int rows = getmaxy(tui->win_messages);

        do
        {
            current++;
            row++;
            counter++;
            if (current >= CLIENT_MAX_VISIBLE_MSGS) current = 0;
            wattron(tui->win_messages, COLOR_PAIR(100+list->user_indices[current]));
            mvwprintw(tui->win_messages, row, 1, "%s", list->msgs[current]);
            wattroff(tui->win_messages, COLOR_PAIR(100+list->user_indices[current]));
        }
        while (current != list->tail && counter < CLIENT_MAX_VISIBLE_MSGS
                && row < rows);

        wrefresh(tui->win_messages);
    }

    pthread_mutex_unlock(&list->lock);
}

static void client_create_rx_thread(ClientSideData_t *data)
{
    pthread_attr_t rx_thread_attributes;
    pthread_attr_init(&rx_thread_attributes);
    pthread_attr_setstacksize(&rx_thread_attributes, PTHREAD_STACK_MIN);
    pthread_create(&(data->rx_thread), &rx_thread_attributes, &client_rx_loop, data);
}

static void client_create_render_thread(ClientSideData_t *data)
{
    pthread_attr_t render_thread_attributes;
    pthread_attr_init(&render_thread_attributes);
    pthread_attr_setstacksize(&render_thread_attributes, PTHREAD_STACK_MIN);
    pthread_create(&(data->render_thread), &render_thread_attributes, &client_render_loop, data);
}

static void client_tx_loop(ClientSideData_t *data)
{
    static const uint8_t typing_status_threshold = 16;

    uint8_t typing_status_counter = 0;
    int input_ch = ERR;

    // initialize the message data structure & text buffer
    explicit_bzero(&data->input.buff, MSG_MAX_CHARS);
    explicit_bzero(&data->outgoing_message, sizeof(Message_t));
    data->outgoing_message.header.encoding_version = htons(ENCODING_VERSION);

    // send initial server join message
    data->outgoing_message.header.message_type = htons(MESSAGE_JOIN);
    strncpy(data->outgoing_message.header.sender_name, data->client_name, NAME_BUFF_LENGTH);
    client_send_message(data);

    noecho();
    timeout(2000);

    while(!should_terminate)
    {
        // wait for user input until timeout
        input_ch = getch();

        if (should_terminate) break;

        switch (input_ch) 
        {
            // if timed out, send a "stay" message to let the server know we are still online
            case ERR:
                typing_status_counter = 0;
                explicit_bzero(data->outgoing_message.body, MSG_MAX_CHARS);
                data->outgoing_message.header.message_type = htons(MESSAGE_STAY);
                client_send_message(data);
                break;
            // if input detected, process it into the input buffer
            // or send it if newline entered
            case '\r':
            case '\n':
                pthread_mutex_lock(&data->input.lock);
                typing_status_counter = 0;

                if (strlen(data->input.buff) > 0)
                {
                    explicit_bzero(data->outgoing_message.body, MSG_MAX_CHARS);
                    data->outgoing_message.header.message_type = htons(MESSAGE_CHAT);
                    strncpy(data->outgoing_message.body, data->input.buff, MSG_MAX_CHARS);
                    client_send_message(data);
                    data->input.idx = 0;
                    explicit_bzero(data->input.buff, MSG_MAX_CHARS);
                }

                data->input.dirty = true;

                pthread_mutex_unlock(&data->input.lock);
                break;
            case '\b':
            case KEY_BACKSPACE:
            case KEY_DC:
            case 127:
                pthread_mutex_lock(&data->input.lock);

                if (data->input.idx > 0)
                {
                    data->input.idx--;
                    data->input.buff[data->input.idx] = 0;
                }

                data->input.dirty = true;

                pthread_mutex_unlock(&data->input.lock);
                break;
            case 27:
                break;
            default:
                pthread_mutex_lock(&data->input.lock);

                if (data->input.idx < MSG_MAX_CHARS - 1)
                {
                    data->input.buff[data->input.idx] = (char)input_ch;
                    data->input.idx++;
                }

                data->input.dirty = true;
                
                if (typing_status_counter == 0)
                {
                    explicit_bzero(data->outgoing_message.body, MSG_MAX_CHARS);
                    data->outgoing_message.header.message_type = htons(MESSAGE_USERDATA);
                    client_send_message(data);
                }
                else if (typing_status_counter >= typing_status_threshold)
                {
                    typing_status_counter = 0;
                }
                else typing_status_counter++;

                pthread_mutex_unlock(&data->input.lock);
                break;
        }
    }

    client_terminate(data, EXIT_SUCCESS);
}

static void* client_rx_loop(void *arg)
{
    ClientSideData_t *data = (ClientSideData_t *)arg;

    struct sockaddr_in incoming_address = { .sin_family = AF_INET };
    Message_t incoming_buffer;
    socklen_t incoming_address_length = sizeof(incoming_address);

    while (!should_terminate)
    {
        explicit_bzero(&incoming_buffer, sizeof(Message_t));
        int bytes_received = recvfrom(data->udp_socket, &incoming_buffer, sizeof(Message_t), 0, (struct sockaddr*)&incoming_address, &incoming_address_length);

        if (should_terminate) break;

        if ((incoming_address.sin_addr.s_addr != data->server_address.sin_addr.s_addr)
            ||(bytes_received <= 0 && errno == EWOULDBLOCK))
            continue;

        client_error_zero(bytes_received, EXIT_FAILURE, "Failed to receive bytes", data);

        if (should_terminate) break;
        if (bytes_received < 2) continue;

        MessageType_t host_order_message_type = ntohs(incoming_buffer.header.message_type);

        switch(host_order_message_type)
        {
            case MESSAGE_CHAT:
            case MESSAGE_RAW:
                client_msglist_push(&data->msg_list, &incoming_buffer);
                break;
            case MESSAGE_USERDATA:
                client_peerlist_update(&data->peer_list, &incoming_buffer);
                break;
            case MESSAGE_JOIN:
            case MESSAGE_QUIT:
                client_peerlist_update(&data->peer_list, &incoming_buffer);
                client_msglist_push(&data->msg_list, &incoming_buffer);
                break;
            case MESSAGE_STAY:
            case MESSAGE_ERROR:
            case MESSAGE_UNDEFINED:
                break;
        }
    }

    return NULL;
}

static void* client_render_loop(void *arg)
{
    ClientSideData_t *data = (ClientSideData_t *)arg;
    ClientTUIData_t *tui = &data->tui;
    ClientMsgList_t *msgs = &data->msg_list;
    ClientPeerList_t *peers = &data->peer_list;
    ClientSessionData_t *session = &data->session;
    ClientInputState_t *input = &data->input;

    start_color();

    init_pair(USTATUS_NONE, COLOR_WHITE, COLOR_BLACK);
    init_pair(USTATUS_IDLE, COLOR_BLUE, COLOR_BLACK);
    init_pair(USTATUS_NEW, COLOR_CYAN, COLOR_BLACK);
    init_pair(USTATUS_ACTIVE, COLOR_GREEN, COLOR_BLACK);
    init_pair(USTATUS_TYPING, COLOR_YELLOW, COLOR_BLACK);
    init_pair(USTATUS_HOT, COLOR_RED, COLOR_YELLOW);

    short color_code = 1;

    for (int i = 0; i < MAX_CLIENT_COUNT; i++)
    {
        init_pair(100+i, color_code, COLOR_BLACK);
        color_code++;
        if (color_code > 7) color_code = 1;
    }

    getmaxyx(curscr, tui->rows, tui->cols);

    tui->win_input = newwin(tui->rows*0.1f, tui->cols*0.8f, tui->rows*0.9f, tui->cols*0.1f);
    tui->win_messages = newwin(tui->rows*0.75f, tui->cols*0.5f, tui->rows*0.1f, tui->cols*0.5f);
    tui->win_users = newwin(tui->rows*0.25f, tui->cols*0.25f, tui->rows*0.1f, tui->cols*0.75f);
    tui->win_session = newwin(tui->rows*0.25f, tui->cols*0.25f, tui->rows*0.1f, tui->cols*0.25f);

    tui->dirty_size = true;

    clear();
    refresh();

    // arbitrary transformation to force first frame draw without complicating the loop
    tui->rows *= 2;
    tui->cols *= 2;

    while (!should_terminate)
    {
        // check if main window has been resized
        tui->prev_rows = tui->rows;
        tui->prev_cols = tui->cols;

        getmaxyx(curscr, tui->rows, tui->cols);

        tui->dirty_size =
            tui->rows != tui->prev_rows
            || tui->cols != tui->prev_cols;

        // check if main window too small
        if (tui->rows < 6 || tui->cols < 32)
        {
            clear();
            printw("Window too small to render.");
            refresh();
            usleep(64000);
            continue;
        }

        if (tui->dirty_size)
        {
            clear();
            refresh();
        }

        // input
        client_input_render(input, tui);

        // msglist
        client_msglist_render(msgs, tui);

        // users
        client_peerlist_render(peers, tui);

        // session
        client_session_render(session, tui);

        // 16ms frame delay to approximate 60fps
        usleep(16000);
    } 

    delwin(tui->win_input);
    delwin(tui->win_users);
    delwin(tui->win_session);
    delwin(tui->win_messages);

    return NULL;
}

static void client_send_message(ClientSideData_t *data)
{
    uint16_t msg_body_length = 0;
    MessageType_t host_order_msg_type = ntohs(data->outgoing_message.header.message_type);

    switch (host_order_msg_type) 
    {
        case MESSAGE_UNDEFINED:
            //printw("*** Attempted sending message with undefined type. Aborting.");
            break;
        case MESSAGE_ERROR:
        case MESSAGE_RAW:
        case MESSAGE_CHAT:
            msg_body_length = strlen(data->outgoing_message.body);

            if (msg_body_length <= 0) return;

            break;
        case MESSAGE_JOIN:
        case MESSAGE_QUIT:
        case MESSAGE_STAY:
            sprintf(data->outgoing_message.body, "%u:%s", ntohs(data->local_address.sin_port), data->client_name);
            msg_body_length = strlen(data->outgoing_message.body);
            break;
        case MESSAGE_USERDATA:
            break;
        }

    data->outgoing_message.header.body_length = htons(msg_body_length);
    data->outgoing_message.header.timestamp = htonl(time(NULL));

    if (host_order_msg_type == MESSAGE_QUIT)
    {
        sendto(data->udp_socket, &data->outgoing_message, sizeof(MessageHeader_t)+msg_body_length+1, 0,
        (struct sockaddr *)&(data->server_address), sizeof(data->server_address));
    }
    else
    {
        client_error_negative(sendto(data->udp_socket, &data->outgoing_message, sizeof(MessageHeader_t)+msg_body_length+1, 0,
        (struct sockaddr *)&(data->server_address), sizeof(data->server_address)), EXIT_FAILURE, "Failed to send message", data);
    }
}

static void client_terminate(ClientSideData_t *data, int exit_value)
{
    // settings the flag just in case,
    // and waiting for the thread to exit
    should_terminate = true;
    pthread_join(data->rx_thread, NULL);
    pthread_join(data->render_thread, NULL);

    // print message if terminated by user
    if (exit_value == 0)
    {
        //printw("Client terminated by user. Goodbye!\n");
        //refresh();
    }

    // send a quit message
    data->outgoing_message.header.message_type = htons(MESSAGE_QUIT);
    client_send_message(data);

    endwin();

    // cleanup and exit
    close(data->udp_socket);
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
