#include "shared/message.h"
#include "shared/protocol.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define CLIENT_ADDRESS "0.0.0.0"
#define CLIENT_PORT 23456

#define SERVER_ADDRESS "0.0.0.0"
#define SERVER_PORT 65432

int main() {
    /**
     * Create a UDP socket
     */
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        perror("Socket creation failed");
        return -1;
    }

    /**
     * Bind the socket to a specific address and port
     */
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(CLIENT_PORT);

    if (inet_pton(AF_INET, CLIENT_ADDRESS, &client_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(socket_fd);
        return -1;
    }

    int bind_result = bind(socket_fd, (struct sockaddr *)&client_addr, sizeof(client_addr));
    if (bind_result < 0) {
        perror("Bind failed");
        close(socket_fd);
        return -1;
    }

    printf("Client started at %s:%d.\n", CLIENT_ADDRESS, CLIENT_PORT);

    /**
     * Send a message to the server
     */
    Message message;
    message.type = MESSAGE_HELLO;
    strcpy(message.content, "Hello, World!!");

    char buffer[256];
    if (serialize(&message, buffer) != 0) {
        perror("Failed to serialize message.");
        close(socket_fd);
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_ADDRESS, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    ssize_t bytes_sent = sendto(socket_fd, buffer, strlen(buffer) + 1, 0,
                                (struct sockaddr *)&server_addr, sizeof(server_addr));

    if (bytes_sent < 0) {
        perror("Message send failed.");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    printf("Message sent: (%d) %s\n", message.type, message.content);

    /**
     * Receive a response from the server
     */
    ssize_t recv_len = recvfrom(socket_fd, buffer, sizeof(buffer) - 1, 0, NULL, NULL);

    if (recv_len < 0) {
        perror("Failed to receive message");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    Message response;
    if (deserialize(buffer, &response) == 0) {
        printf("Message received: (%d) %s\n", response.type, response.content);
    } else {
        perror("Message received but failed to deserialize");
    }

    close(socket_fd);
    printf("Client has stopped.\n");
    return 0;
}