#include "include/protocol.h"
#include <string.h>

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

Message message_init(const MessageType type, const char *content) {
    Message message;
    message.type = type;
    strncpy(message.content, content, sizeof(message.content) - 1);
    message.content[sizeof(message.content) - 1] = '\0'; // Ensure null-termination
    return message;
}

ssize_t protocol_send(const int socket_fd, const Message *message, const Address *destination) {
    char buffer[256];
    if (message_serialize(message, buffer) != 0) {
        //     perror("Failed to serialize message.");
        //     close(socket_fd);
        //     return EXIT_FAILURE;
        return -1;
    }

    return sendto(socket_fd, buffer, strlen(buffer) + 1, 0, (struct sockaddr *)destination,
                  sizeof(*destination));
}

ssize_t protocol_receive(const int socket_fd, Message *message, Address *source) {
    char buffer[256];
    Address source_addr;
    socklen_t addr_len = sizeof(source_addr);
    ssize_t recv_len =
        recvfrom(socket_fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&source_addr, &addr_len);
    if (recv_len < 0) {
        // perror("Failed to receive message.");
        return -1;
    }

    if (message_deserialize(buffer, message) != 0) {
        // perror("Failed to deserialize message.");
        return -1;
    }

    if (source != NULL) {
        *source = source_addr;
    }

    return recv_len;
}

ssize_t protocol_wait(const int socket_fd, const Message *request, Message *response,
                      const MessageType type, const int timeout) {
    ssize_t send_len = protocol_send(socket_fd, request, NULL);

    if (send_len < 0) {
        // perror("Failed to send response");
        return -1;
    }

    while (1) {
        // Implement a timeout mechanism here if needed, using select() or poll() to wait for data
        // on the socket with a timeout.
        ssize_t recv_len = protocol_receive(socket_fd, response, NULL);

        if (recv_len < 0) {
            // perror("Failed to receive message");
            return -1;
        }

        if (response->type == MESSAGE_PLAYER_CONNECTED || response->type == MESSAGE_ERROR) {
            break; // Success: received the expected message type or an error message
        }
    }
    return 0;
}