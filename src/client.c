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
        printf("%s\n", message->content);
        break;
    }
    case MESSAGE_PLAYER_DISCONNECTED: {
        printf("Player disconnected: %s\n", message->content);
        break;
    }
    case MESSAGE_GAME_WAIT: {
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
    Message connect_message = message_init(MESSAGE_PLAYER_CONNECT, player_id);
    ssize_t bytes_sent = protocol_send(socket_fd, &connect_message, server);

    if (bytes_sent < 0) {
        perror("Error: Failed to send message.");
        close(socket_fd);
        return -1;
    }

    printf("[DEBUG] Sent message: (%d) %s\n", connect_message.type, connect_message.content);

    Message response_message;
    ssize_t bytes_received = protocol_receive(socket_fd, &response_message, NULL);

    if (bytes_received < 0) {
        perror("Error: Failed to receive message.");
        close(socket_fd);
        return -1;
    }

    printf("[DEBUG] Received message: (%d) %s\n", response_message.type, response_message.content);

    if (response_message.type == MESSAGE_PLAYER_CONNECTED ||
        response_message.type == MESSAGE_ERROR) {
        return client_process_message(&response_message);
    }

    return -1;
}

int client_disconnect(int socket_fd, const char *player_id, Address *server) {
    Message disconnect_message = message_init(MESSAGE_PLAYER_DISCONNECT, "Disconnecting");
    ssize_t bytes_sent = protocol_send(socket_fd, &disconnect_message, server);

    if (bytes_sent < 0) {
        perror("Error: Failed to send disconnect message.");
        close(socket_fd);
        return -1;
    }

    return 0;
}

int main() {
    char *server_addr = "0.0.0.0";
    char *server_port = "65432";

    Address client;
    memset(&client, 0, sizeof(client));
    client.sin_family = AF_INET;
    client.sin_port = htons(CLIENT_PORT);

    if (inet_pton(AF_INET, CLIENT_ADDRESS, &client.sin_addr) <= 0) {
        perror("Invalid client address");
        return EXIT_FAILURE;
    }

    Address server;
    memset(&server, 0, sizeof(server));
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

    int bind_result = bind(socket_fd, (struct sockaddr *)&client, sizeof(client));
    if (bind_result < 0) {
        perror("Bind failed");
        close(socket_fd);
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
     * Receive a response from the server
     */
    while (1) {
        Message message;
        ssize_t recv_len = protocol_receive(socket_fd, &message, NULL);

        if (recv_len < 0) {
            perror("Failed to receive message");
            close(socket_fd);
            break;
        }

        printf("[DEBUG] Received message: (%d) %s\n", message.type, message.content);

        if (client_process_message(&message) != 0) {
            break;
        }
    }

    client_disconnect(socket_fd, player_id, &server);
    close(socket_fd);
    return 0;
}