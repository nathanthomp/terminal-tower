#include "include/game.h"
#include "include/protocol.h"
#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define CLIENT_ADDRESS "0.0.0.0"
#define CLIENT_PORT 23456

size_t client_process_message(const Message *message, Message *responses) {
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
    case MESSAGE_PING: {
        // responses[0] = message_init(MESSAGE_PONG, "Pong");
        // return 1;
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
        return client_process_message(&response, NULL);
    }

    return -1;
}

int client_disconnect(int socket_fd, const char *player_id, Address *server) {
    Message request = message_init(MESSAGE_PLAYER_DISCONNECT, "Disconnecting");
    protocol_send(socket_fd, &request, server);

    return 0;
}

/**
 * Lets try to create a thread that listens for messages from the server and prints them to
 * the console. This thread will run in the background and will not block the main thread.
 * The main thread will listen for user input and will send messages to the server. The main
 * thread will also handle the heartbeat mechanism.
 */

// Message request = {0};
// protocol_receive(socket_fd, &request, NULL);

// size_t response_len = client_process_message(&request, responses);
// if (response_len < 0) {
//     running = false;
// }

// for (int i = 0; i < response_len; i++) {
//     protocol_send(socket_fd, &responses[i], &server);
// }
void *network_listener(void *arg) {
    int socket_fd = *(int *)arg;

    printf("[INFO] Network listener thread started\n");

    while (1) {
        Message request = {0};
        protocol_receive(socket_fd, &request, NULL);

        Message responses[10];
        size_t response_len = client_process_message(&request, responses);
        if (response_len < 0) {
            break;
        }

        for (int i = 0; i < response_len; i++) {
            // Send the response back to the server
            Message response = responses[i];
            printf("[WARNING] Not sending response to server: (%d) %s\n",
                   (MessageType)response.type, response.content);
            // protocol_send(socket_fd, &response, &server);
        }
    }

    printf("[INFO] Network listener thread ended\n");

    return NULL;
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
    // char player_id[MAX_PLAYER_ID_LENGTH];
    // printf("Enter player id: ");
    // if (fgets(player_id, sizeof(player_id), stdin) == NULL) {
    //     perror("Failed to read player id");
    //     close(socket_fd);
    //     return EXIT_FAILURE;
    // }

    // player_id[strcspn(player_id, "\n")] = 0;

    // if (client_connect(socket_fd, player_id, &server) != 0) {
    //     printf("Error: Could not connect to server\n");
    //     close(socket_fd);
    //     return EXIT_FAILURE;
    // }

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

    /**
     * Create a thread that listens for messages from the server and prints them to the console.
     */
    pthread_t network_thread;
    int result = pthread_create(&network_thread, NULL, network_listener, &socket_fd);
    if (result != 0) {
        perror("Failed to create network listener thread");
        close(socket_fd);
        return EXIT_FAILURE;
    }
    printf("[INFO] Network listener thread created\n");

    bool running = true;
    while (running) {
        /**
         * 1. Process all messages (network)
         *
         * EXAMPLE: If the server sends
         */
        /**
         * 2. Process all events (input)
         */
        /**
         * 3. Update game
         */
        /**
         * 4. Render game
         */
    }

    // client_disconnect(socket_fd, player_id, &server);
    close(socket_fd);
    return 0;
}