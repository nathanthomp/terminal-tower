#include "include/protocol.h"
#include <errno.h>
#include <stdio.h> // For debugging purposes, remove this in production
#include <string.h>

Message message_init(const Type type, const char *content) {
    Message message = {0};
    message.type = type;
    message.session_id = 0;
    /**
     * Should this me memcopy or strncpy? I think strncpy is safer since it will ensure
     * null-termination, but we need to be careful about the size of the content.
     */
    strncpy(message.content, content, sizeof(message.content) - 1);
    message.content[sizeof(message.content) - 1] = '\0'; // Ensure null-termination
    return message;
}

static int message_serialize(const Message *message, char *buffer) {
    memset(buffer, 0, 256);
    // Byte 0: MessageType
    buffer[0] = (char)message->type;
    // Byte 1-2: Session ID
    buffer[1] = (char)((message->session_id >> 8) & 0xFF);
    buffer[2] = (char)(message->session_id & 0xFF);
    // Bytes 3-127: Content
    memcpy(buffer + 3, message->content, sizeof(message->content));
    return 0;
}

static int message_deserialize(Message *message, const char *buffer) {
    memset(message, 0, sizeof(*message));
    // Byte 0: MessageType
    message->type = (Type)buffer[0];
    // Bytes 1-2: Session ID
    message->session_id = ((uint16_t)(buffer[1] & 0xFF) << 8) | (uint16_t)(buffer[2] & 0xFF);
    // Bytes 3-127: Content
    memcpy(message->content, buffer + 3, sizeof(message->content));
    return 0;
}

Result protocol_send(const int socket_fd, const Message *message, const Address *destination) {
    char buffer[256];

    if (message_serialize(message, buffer) != 0) {
        printf("[ERROR] Failed to serialize message\n");
    }

    ssize_t bytes_sent = sendto(socket_fd, buffer, 3 + sizeof(message->content), 0,
                                (struct sockaddr *)destination, sizeof(*destination));

    if (bytes_sent == -1) {
        printf("[ERROR] Failed to send message\n");
        return RESULT_ERROR;
    }

    if (message->type == MESSAGE_GAME_STATE) {
        // printf("[DEBUG] Sent message to %s:%d: (10) [game state: state=%d, players=%d]\n",
        //        inet_ntoa(destination->sin_addr), ntohs(destination->sin_port),
        //        (uint8_t)message->content[0], (uint8_t)message->content[1]);
    } else {
        // printf("[DEBUG] Sent message to %s:%d: (%d) %s\n", inet_ntoa(destination->sin_addr),
        //        ntohs(destination->sin_port), (Type)message->type, message->content);
    }
    return RESULT_SENT;
}

Result protocol_receive(const int socket_fd, Message *message, Address *source) {
    char buffer[256];

    Address source_addr = {0};
    socklen_t addr_len = sizeof(source_addr);
    ssize_t recv_len = recvfrom(socket_fd, buffer, sizeof(buffer), MSG_DONTWAIT,
                                (struct sockaddr *)&source_addr, &addr_len);

    if (recv_len < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return RESULT_NOTHING;
        }

        perror("[ERROR] Failed to receive message");
        return RESULT_ERROR;
    }

    if (message_deserialize(message, buffer) != 0) {
        printf("[ERROR] Failed to deserialize message\n");
        return RESULT_ERROR;
    }

    // printf("[DEBUG] Received message from %s:%d: (%d) %s\n", inet_ntoa(source_addr.sin_addr),
    //        ntohs(source_addr.sin_port), (Type)message->type, message->content);

    if (source != NULL) {
        *source = source_addr;
    }

    return RESULT_MESSAGE;
}

Result protocol_wait(const int socket_fd, Message *message, Address *source, const Type type,
                     const int timeout) {
    char buffer[256];

    struct timeval timeout_value = {.tv_sec = timeout, .tv_usec = 0};
    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout_value, sizeof(timeout_value));

    socklen_t source_len = sizeof(*source);
    while (true) {
        ssize_t recv_len =
            recvfrom(socket_fd, buffer, sizeof(buffer), 0, (struct sockaddr *)source, &source_len);

        if (recv_len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return RESULT_TIMEOUT;
            }

            return RESULT_ERROR;
        }

        if (message_deserialize(message, buffer) != 0) {
            return RESULT_ERROR;
        }

        if (message->type == type || type == MESSAGE_ANY) {
            break;
        }
    }

    // printf("[DEBUG] Received expected message from %s:%d: (%d) %s\n",
    // inet_ntoa(source->sin_addr),
    //        ntohs(source->sin_port), (Type)message->type, message->content);

    timeout_value.tv_sec = 0;
    timeout_value.tv_usec = 0;
    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout_value, sizeof(timeout_value));

    return RESULT_MESSAGE;
}