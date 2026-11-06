#include "include/protocol.h"
#include <stdio.h> // For debugging purposes, remove this in production
#include <string.h>

Message message_init(const MessageType type, const char *content) {
    Message message;
    message.type = type;
    strncpy(message.content, content, sizeof(message.content) - 1);
    message.content[sizeof(message.content) - 1] = '\0'; // Ensure null-termination
    return message;
}

static int message_serialize(const Message *message, char *buffer) {
    // Byte 0: MessageType
    buffer[0] = (char)message->type;
    // Bytes 1-127: Content
    strncpy(buffer + 1, message->content, sizeof(message->content) - 1);
    return 0;
}

static int message_deserialize(const char *buffer, Message *message) {
    // Byte 0: MessageType
    message->type = (MessageType)buffer[0];
    // Bytes 1-127: Content
    strncpy(message->content, buffer + 1, sizeof(message->content) - 1);
    return 0;
}

void protocol_send(const int socket_fd, const Message *message, const Address *destination) {
    char buffer[256];

    if (message_serialize(message, buffer) != 0) {
        printf("[ERROR] Failed to serialize message");
    }

    ssize_t bytes_sent = sendto(socket_fd, buffer, strlen(buffer) + 1, 0,
                                (struct sockaddr *)destination, sizeof(*destination));

    if (bytes_sent < 0) {
        printf("[ERROR] Failed to send response");
    }

    printf("[DEBUG] Sent message to %s:%d: (%d) %s\n", inet_ntoa(destination->sin_addr),
           ntohs(destination->sin_port), (MessageType)message->type, message->content);
}

void protocol_receive(const int socket_fd, Message *message, Address *source) {
    char buffer[256];

    Address source_addr = {0};
    socklen_t addr_len = sizeof(source_addr);
    ssize_t recv_len =
        recvfrom(socket_fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&source_addr, &addr_len);

    if (recv_len < 0) {
        printf("[ERROR] Failed to receive message");
    }

    if (message_deserialize(buffer, message) != 0) {
        printf("[ERROR] Failed to deserialize message");
    }

    printf("[DEBUG] Received message from %s:%d: (%d) %s\n", inet_ntoa(source_addr.sin_addr),
           ntohs(source_addr.sin_port), (MessageType)message->type, message->content);

    if (source != NULL) {
        *source = source_addr;
    }
}

/**
 * This has not been used yet.
 */
void protocol_wait(const int socket_fd, const Message *request, Message *response,
                   const MessageType type, const int timeout) {
    protocol_send(socket_fd, request, NULL);

    while (1) {
        // Implement a timeout mechanism here if needed, using select() or poll() to wait for data
        // on the socket with a timeout.
        protocol_receive(socket_fd, response, NULL);

        if (response->type == MESSAGE_PLAYER_CONNECTED || response->type == MESSAGE_ERROR) {
            break; // Success: received the expected message type or an error message
        }
    }
}