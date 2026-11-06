#include "include/game.h"
#include "include/protocol.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define CLIENT_ADDRESS "0.0.0.0"
#define CLIENT_PORT 23456

int client_process_message(const Message *message) {
    switch (message->type) {
    case MESSAGE_PLAYER_CONNECTED: {
        printf("%s connected\n", message->content);
        break;
    }
    case MESSAGE_PLAYER_DISCONNECTED: {
        printf("%s disconnected\n", message->content);
        break;
    }
    case MESSAGE_GAME_WAIT: {
        printf("%s\n", message->content);
        break;
    }
    case MESSAGE_GAME_READY: {
        printf("%s\n", message->content);
        break;
    }
    case MESSAGE_GAME_START: {
        printf("Game has started\n");
        break;
    }
    case MESSAGE_ERROR: {
        printf("Error: %s\n", message->content);
        return -1;
    }
    default:
        printf("Unknown message type: %d\n", message->type);
        return -1;
    }
    return 0;
}

int client_connect(int socket_fd, const char *player_id, Address *server) {
    Message request = message_init(MESSAGE_PLAYER_CONNECT, player_id);
    protocol_send(socket_fd, &request, server);

    Message response;
    protocol_receive(socket_fd, &response, NULL);

    if (response.type == MESSAGE_PLAYER_CONNECTED || response.type == MESSAGE_ERROR) {
        return client_process_message(&response);
    }

    return -1;
}

int client_disconnect(int socket_fd, const char *player_id, Address *server) {
    Message request = message_init(MESSAGE_PLAYER_DISCONNECT, "Disconnecting");
    protocol_send(socket_fd, &request, server);

    return 0;
}

int main(int argc, char *argv[]) {
    /**
     * Parse arguments for the help flag. If provided, print the help message and exit.
     * If not provided, continue with the program.
     */

    /**
     * Parse aguments for server address and port. If not provided, exit with an error message.
     * If provided, validate the address and port. If invalid, exit with an error message.
     */
    char *server_addr = "0.0.0.0";
    char *server_port = "65432";

    Address client = {0};
    client.sin_family = AF_INET;
    client.sin_port = htons(CLIENT_PORT);

    if (inet_pton(AF_INET, CLIENT_ADDRESS, &client.sin_addr) <= 0) {
        perror("Invalid client address");
        return EXIT_FAILURE;
    }

    Address server = {0};
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(server_port));
    if (inet_pton(AF_INET, server_addr, &server.sin_addr) <= 0) {
        perror("Invalid server address");
        return EXIT_FAILURE;
    }

    /**
     * Create and bind to a UDP socket
     */
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd == -1) {
        perror("Socket failed");
        return EXIT_FAILURE;
    }

    /**
     * Connect to the server as a player
     */
    char player_id[MAX_PLAYER_ID_LENGTH];
    printf("Enter player id: ");
    if (fgets(player_id, sizeof(player_id), stdin) == NULL) {
        perror("Failed to read player id");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    player_id[strcspn(player_id, "\n")] = 0;

    if (client_connect(socket_fd, player_id, &server) != 0) {
        printf("Error: Could not connect to server\n");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    /**
     * Wait for a response from the server.
     * protocol_wait(PLAYER_CONNECTED) will block until a response is received or a timeout
     * occurs. If a timeout occurs, the client will exit with an error message. If a response is
     * received, the client will print the response and exit. If the response is not
     * PLAYER_CONNECTED, the client will exit with an error message.
     */

    /**
     * Here, we need to continously do the following:
     * - Listen for messages from the server
     * - Listen for user input
     */
    bool running = true;
    while (running) {
        Message message;
        protocol_receive(socket_fd, &message, NULL);

        if (client_process_message(&message) != 0) {
            running = false;
        }
    }

    client_disconnect(socket_fd, player_id, &server);
    close(socket_fd);
    return 0;
}