#include "include/protocol.h"
#include <errno.h>
#include <stdio.h> // For debugging purposes, remove this in production
#include <string.h>

Message message_init(const Type type, const char *content) {
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
    message->type = (Type)buffer[0];
    // Bytes 1-127: Content
    strncpy(message->content, buffer + 1, sizeof(message->content) - 1);
    return 0;
}

Result protocol_send(const int socket_fd, const Message *message, const Address *destination) {
    char buffer[256];

    if (message_serialize(message, buffer) != 0) {
        printf("[ERROR] Failed to serialize message\n");
    }

    ssize_t bytes_sent = sendto(socket_fd, buffer, strlen(buffer) + 1, 0,
                                (struct sockaddr *)destination, sizeof(*destination));

    if (bytes_sent == -1) {
        printf("[ERROR] Failed to send message\n");
        return RESULT_ERROR;
    }

    printf("[DEBUG] Sent message to %s:%d: (%d) %s\n", inet_ntoa(destination->sin_addr),
           ntohs(destination->sin_port), (Type)message->type, message->content);
    return RESULT_SUCCESS;
}

Result protocol_receive(const int socket_fd, Message *message, Address *source) {
    char buffer[256];

    Address source_addr = {0};
    socklen_t addr_len = sizeof(source_addr);
    ssize_t recv_len =
        recvfrom(socket_fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&source_addr, &addr_len);

    if (recv_len == -1) {
        printf("[ERROR] Failed to receive message\n");
        return RESULT_ERROR;
    }

    if (message_deserialize(buffer, message) != 0) {
        printf("[ERROR] Failed to deserialize message\n");
        return RESULT_ERROR;
    }

    printf("[DEBUG] Received message from %s:%d: (%d) %s\n", inet_ntoa(source_addr.sin_addr),
           ntohs(source_addr.sin_port), (Type)message->type, message->content);

    if (source != NULL) {
        *source = source_addr;
    }

    return RESULT_SUCCESS;
}

Result protocol_wait(const int socket_fd, Message *message, Address *source, const Type type,
                     const int timeout) {
    char buffer[256];

    struct timeval timeout_value = {.tv_sec = timeout, .tv_usec = 0};
    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout_value, sizeof(timeout_value));

    socklen_t source_len = sizeof(*source);
    ssize_t recv_len =
        recvfrom(socket_fd, buffer, sizeof(buffer), 0, (struct sockaddr *)source, &source_len);

    if (recv_len < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return RESULT_TIMEOUT;
        }

        return RESULT_ERROR;
    }

    if (message_deserialize(buffer, message) != 0) {
        return RESULT_ERROR;
    }

    if (message->type != type && type != MESSAGE_ANY) {
        return RESULT_ERROR;
    }

    printf("[DEBUG] Received expected message from %s:%d: (%d) %s\n", inet_ntoa(source->sin_addr),
           ntohs(source->sin_port), (Type)message->type, message->content);

    timeout_value.tv_sec = 0;
    timeout_value.tv_usec = 0;
    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout_value, sizeof(timeout_value));

    return RESULT_SUCCESS;
}