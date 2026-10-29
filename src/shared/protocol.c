#include "protocol.h"
#include <stdint.h>
#include <string.h>

int broadcast() { return 0; }

int unicast() { return 0; }

int serialize(const Message *message, char *buffer) {
    // Byte 0: MessageType
    buffer[0] = (char)message->type;
    // Bytes 1-127: Content
    strncpy(buffer + 1, message->content, sizeof(message->content) - 1);
    return 0;
}

int deserialize(const char *buffer, Message *message) {
    // Byte 0: MessageType
    message->type = (MessageType)buffer[0];
    // Bytes 1-127: Content
    strncpy(message->content, buffer + 1, sizeof(message->content) - 1);
    return 0;
}