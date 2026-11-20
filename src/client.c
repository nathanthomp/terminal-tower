#include "include/game.h"
#include "include/protocol.h"
#include "include/queue.h"
#include <arpa/inet.h>
#include <linux/input.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

typedef enum { STATUS_CONNECTING, STATUS_CONNECTED, STATUS_DISCONNECTED, STATUS_ERRORED } Status;

typedef struct {
    int width;
    int height;

    char *previous_screen;
    char *current_screen;
} Terminal;

typedef struct {
    bool running;
    int socket;
    Address server;
    Game game;

    Status status;
    char id[MAX_PLAYER_ID_LENGTH];

    pthread_t network_handler;
    pthread_t input_handler;
    pthread_mutex_t mutex;
    pthread_cond_t condition;

    char input_queue_elements[QUEUE_CAPACITY];
    Queue input_queue;
} Client;

bool client_running(Client *client) {
    pthread_mutex_lock(&client->mutex);
    bool running = client->running;
    pthread_mutex_unlock(&client->mutex);
    return running;
}

void client_stop(Client *client) {
    pthread_mutex_lock(&client->mutex);
    client->running = false;
    pthread_mutex_unlock(&client->mutex);
}

void client_get_status(Client *client, Status *status) {
    pthread_mutex_lock(&client->mutex);
    *status = client->status;
    pthread_mutex_unlock(&client->mutex);
}

void client_set_status(Client *client, const Status status) {
    pthread_mutex_lock(&client->mutex);
    client->status = status;
    pthread_mutex_unlock(&client->mutex);
}

void client_get_game(Client *client, Game *game) {
    pthread_mutex_lock(&client->mutex);
    *game = client->game;
    pthread_mutex_unlock(&client->mutex);
}

void client_set_game(Client *client, const Game *game) {
    pthread_mutex_lock(&client->mutex);
    client->game = *game;
    pthread_mutex_unlock(&client->mutex);
}

void *client_network_handler(void *arg) {
    Client *client = (Client *)arg;

    Message connect_request = message_init(MESSAGE_PLAYER_CONNECT, client->id);
    protocol_send(client->socket, &connect_request, &client->server);

    Message connect_response;
    Result connection_result = protocol_wait(client->socket, &connect_response, &client->server,
                                             MESSAGE_PLAYER_CONNECT_ACK, PROTOCOL_TIMEOUT_SECONDS);

    if (connection_result < 0) {
        client_set_status(client, STATUS_ERRORED);
        printf("[ERROR] Unable to connect to server\n");
        client_stop(client);
        return NULL;
    }

    Message game_response;
    Result game_result = protocol_wait(client->socket, &game_response, &client->server,
                                       MESSAGE_GAME_STATE, PROTOCOL_TIMEOUT_SECONDS);

    if (game_result < 0) {
        client_set_status(client, STATUS_ERRORED);
        printf("[ERROR] Unable to get game state from server\n");
        client_stop(client);
        return NULL;
    }

    client_set_status(client, STATUS_CONNECTED);

    Game game;
    game_deserialize(&game, game_response.content);
    client_set_game(client, &game);

    while (client_running(client)) {
        Message message;
        Result result = protocol_receive(client->socket, &message, &client->server);

        if (result == RESULT_MESSAGE) {
            switch (message.type) {
            case MESSAGE_GAME_STATE: {
                Game game;
                game_deserialize(&game, message.content);
                client_set_game(client, &game);
                break;
            }
            case MESSAGE_PLAYER_DISCONNECTED: {
                break;
            }
            case MESSAGE_PING: {
                Message pong = message_init(MESSAGE_PONG, "Pong");
                protocol_send(client->socket, &pong, &client->server);
                break;
            }
            case MESSAGE_ERROR: {
                printf("[ERROR] Server error: %s\n", message.content);
                client_stop(client);
                break;
            }
            default:
                break;
            }
        }
    }

    client_set_status(client, STATUS_DISCONNECTED);

    Message disconnect = message_init(MESSAGE_PLAYER_DISCONNECT, "Disconnecting");
    protocol_send(client->socket, &disconnect, &client->server);

    return NULL;
}

void *client_input_handler(void *arg) {
    Client *client = (Client *)arg;

    struct termios original;
    struct termios modified;

    if (tcgetattr(STDIN_FILENO, &original) == -1) {
        perror("tcgetattr");
        return NULL;
    }

    modified = original;
    modified.c_lflag &= ~(ICANON | ECHO);
    modified.c_cc[VMIN] = 0;
    modified.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &modified) == -1) {
        perror("tcsetattr");
        return NULL;
    }

    while (client_running(client)) {
        char input;
        ssize_t bytes = read(STDIN_FILENO, &input, 1);
        if (bytes == 1) {
            pthread_mutex_lock(&client->mutex);
            queue_enqueue(&client->input_queue, &input);
            pthread_mutex_unlock(&client->mutex);
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &original);

    return NULL;
}

int client_resolve_address(const char *server_name, const char *server_port, Address *server) {
    /**
     * Commented out for testing purposes.
     */
    // struct addrinfo hints = {0};
    // hints.ai_family = AF_INET;
    // hints.ai_socktype = SOCK_DGRAM;
    // hints.ai_protocol = IPPROTO_UDP;

    // struct addrinfo *result = NULL;
    // int status = getaddrinfo(server_name, server_port, &hints, &result);
    // if (status != 0) {
    //     printf("[ERROR] Failed to resolve server address: %s\n", gai_strerror(status));
    //     return EXIT_FAILURE;
    // }

    // *server = *(struct sockaddr_in *)result->ai_addr;
    // freeaddrinfo(result);

    server->sin_family = AF_INET;
    server->sin_port = htons(atoi(server_port));
    if (inet_pton(AF_INET, "0.0.0.0", &server->sin_addr) <= 0) {
        printf("[ERROR] Invalid server address: %s\n", server_name);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int client_init(Client *client, Address *server) {
    pthread_mutex_init(&client->mutex, NULL);
    pthread_cond_init(&client->condition, NULL);

    client->running = true;
    client->server = *server;

    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd == -1) {
        printf("[ERROR] Socket failed\n");
        return EXIT_FAILURE;
    }

    client->socket = socket_fd;

    char player_id[MAX_PLAYER_ID_LENGTH];
    printf("Enter player id: ");
    if (fgets(player_id, sizeof(player_id), stdin) == NULL) {
        printf("[ERROR] Bad input\n");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    player_id[strcspn(player_id, "\n")] = 0;
    if (player_id[0] == '\0') {
        printf("[ERROR] Player ID cannot be empty\n");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    strncpy(client->id, player_id, sizeof(client->id) - 1);
    client->id[sizeof(client->id) - 1] = '\0';

    if (pthread_create(&client->network_handler, NULL, client_network_handler, client) != 0) {
        printf("[ERROR] Failed to create network listener thread\n");
        close(client->socket);
        return EXIT_FAILURE;
    }

    queue_init(&client->input_queue, &client->input_queue_elements, sizeof(char));

    if (pthread_create(&client->input_handler, NULL, client_input_handler, client) != 0) {
        printf("[ERROR] Failed to create input listener thread\n");
        close(client->socket);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

void client_terminal_init(Terminal *terminal, int width, int height, char *previous_screen,
                          char *current_screen) {
    terminal->width = width;
    terminal->height = height;

    memset(previous_screen, ' ', width * height);
    memset(current_screen, 0, width * height);

    terminal->previous_screen = previous_screen;
    terminal->current_screen = current_screen;

    printf("\033[2J");
    printf("\033[H");
    printf("\033[?25l");

    fflush(stdout);
}

void client_terminal_render(Client *client, Terminal *terminal) {
    memset(terminal->current_screen, ' ', terminal->width * terminal->height);

    // Draw border and corners
    for (int x = 0; x < terminal->width; x++) {
        terminal->current_screen[x] = '-';
        terminal->current_screen[(terminal->height - 1) * terminal->width + x] = '-';
    }
    for (int y = 0; y < terminal->height; y++) {
        terminal->current_screen[y * terminal->width] = '|';
        terminal->current_screen[y * terminal->width + terminal->width - 1] = '|';
    }
    terminal->current_screen[0] = '+';
    terminal->current_screen[terminal->width - 1] = '+';
    terminal->current_screen[(terminal->height - 1) * terminal->width] = '+';
    terminal->current_screen[(terminal->height - 1) * terminal->width + terminal->width - 1] = '+';

    /**
     * Draw header
     * Client Status | Game State
     */
    char header[64];

    Status status;
    client_get_status(client, &status);

    char client_status[16];
    switch (status) {
    case STATUS_CONNECTING:
        snprintf(client_status, sizeof(client_status), "Connecting");
        break;
    case STATUS_CONNECTED:
        snprintf(client_status, sizeof(client_status), "Connected");
        break;
    case STATUS_DISCONNECTED:
        snprintf(client_status, sizeof(client_status), "Disconnected");
        break;
    case STATUS_ERRORED:
        snprintf(client_status, sizeof(client_status), "Error");
        break;
    default:
        snprintf(client_status, sizeof(client_status), "Unknown");
        break;
    }

    Game game;
    client_get_game(client, &game);

    char game_state[16];
    switch (game.state) {
    case STATE_WAITING:
        snprintf(game_state, sizeof(game_state), "Waiting");
        break;
    case STATE_READY:
        snprintf(game_state, sizeof(game_state), "Ready");
        break;
    case STATE_PLAYING:
        snprintf(game_state, sizeof(game_state), "Playing");
        break;
    case STATE_PAUSED:
        snprintf(game_state, sizeof(game_state), "Paused");
        break;
    case STATE_OVER:
        snprintf(game_state, sizeof(game_state), "Over");
        break;
    default:
        snprintf(game_state, sizeof(game_state), "Unknown");
        break;
    }

    snprintf(header, sizeof(header), "%s | %s", client_status, game_state);
    int header_length = strlen(header);
    int header_x = (terminal->width - header_length) / 2;
    int header_y = 1;
    for (int i = 0; i < header_length; i++) {
        terminal->current_screen[header_y * terminal->width + header_x + i] = header[i];
    }

    // Render
    for (int y = 0; y < terminal->height; y++) {
        for (int x = 0; x < terminal->width; x++) {
            int index = y * terminal->width + x;
            if (terminal->current_screen[index] != terminal->previous_screen[index]) {
                printf("\033[%d;%dH", y + 1, x + 1);
                putchar(terminal->current_screen[index]);
                terminal->previous_screen[index] = terminal->current_screen[index];
            }
        }
    }

    fflush(stdout);
}

void client_terminal_clear(Terminal *terminal) {
    printf("\033[2J");
    printf("\033[H");
    printf("\033[?25h");
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    if ((argc > 1 && strcmp(argv[1], "-h") == 0) || argc < 5) {
        printf("Usage: %s -i <server_name> -p <server_port>\n", argv[0]);
        return EXIT_SUCCESS;
    }

    char *server_name = argv[2];
    char *server_port = argv[4];

    Address server = {0};
    if (client_resolve_address(server_name, server_port, &server) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    Client client = {0};
    if (client_init(&client, &server) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    struct winsize size;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == -1) {
        printf("[ERROR] Failed to get terminal size\n");
        return EXIT_FAILURE;
    }

    Terminal terminal = {0};
    char previous_screen[size.ws_col * size.ws_row];
    char current_screen[size.ws_col * size.ws_row];
    client_terminal_init(&terminal, size.ws_col, size.ws_row, previous_screen, current_screen);

    while (client_running(&client)) {
        /**
         * 1. Process all messages (network)
         */

        /**
         * 2. Process all events (input)
         */
        while (true) {
            char input;
            if (!queue_dequeue(&client.input_queue, &input)) {
                break;
            }

            switch (input) {
            case 'a': {
                Message message = message_init(MESSAGE_PLAYER_MOVE, "left");
                protocol_send(client.socket, &message, &client.server);
                break;
            }
            case 'd': {
                Message message = message_init(MESSAGE_PLAYER_MOVE, "right");
                protocol_send(client.socket, &message, &client.server);
                break;
            }
            case '\n': {
                Game game;
                client_get_game(&client, &game);
                if (game.state == STATE_READY) {
                    Message message = message_init(MESSAGE_GAME_START, "start");
                    protocol_send(client.socket, &message, &client.server);
                }
                break;
            }
            case 'q':
                client_stop(&client);
                break;
            default:
                break;
            }
        }
        /**
         * 3. Update game state
         */

        /**
         * 4. Render game
         */
        client_terminal_render(&client, &terminal);

        /**
         * 5. Wait for next frame ~30 FPS.
         */
        usleep(33333);
    }

    /**
     * Cleanup
     */
    pthread_join(client.network_handler, NULL);
    pthread_join(client.input_handler, NULL);

    client_terminal_clear(&terminal);

    queue_destroy(&client.input_queue);
    close(client.socket);

    return EXIT_SUCCESS;
}
