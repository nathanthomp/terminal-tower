#include "shared/message.h"
#include "shared/protocol.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

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
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_ADDRESS, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(socket_fd);
        return -1;
    }

    int bind_result = bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (bind_result < 0) {
        perror("Bind failed");
        close(socket_fd);
        return -1;
    }

    printf("Server started at %s:%d\n", SERVER_ADDRESS, SERVER_PORT);

    /**
     * Receive messages from the client
     */
    while (1) {
        char buffer[256];
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        ssize_t recv_len = recvfrom(socket_fd, buffer, sizeof(buffer) - 1, 0,
                                    (struct sockaddr *)&client_addr, &addr_len);
        if (recv_len < 0) {
            perror("Receive failed");
            break;
        }

        Message message;
        if (deserialize(buffer, &message) != 0) {
            perror("Failed to deserialize message");
            continue;
        }

        printf("Message received: (%d) %s\n", message.type, message.content);

        /**
         * Respond to the client with a message
         */
        if (message.type == MESSAGE_HELLO) {
            Message response;
            response.type = MESSAGE_GOODBYE;
            strcpy(response.content, "Goodbye, World!!");

            if (serialize(&response, buffer) != 0) {
                perror("Failed to serialize message.");
                continue;
            }

            ssize_t bytes_sent = sendto(socket_fd, buffer, sizeof(buffer), 0,
                                        (struct sockaddr *)&client_addr, addr_len);

            if (bytes_sent < 0) {
                perror("Message send failed.");
                close(socket_fd);
                return 1;
            }

            printf("Message sent: (%d) %s\n", response.type, response.content);
        }
    }

    close(socket_fd);
    printf("Server has stopped.\n");
    return 0;
}