#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <arpa/inet.h>
#include <stdint.h>

#define PROTOCOL_TIMEOUT_SECONDS 5

typedef struct sockaddr_in Address;

typedef int8_t Byte;
typedef uint8_t Flag;

typedef enum {
    MESSAGE_PLAYER_CONNECT = 1,
    MESSAGE_PLAYER_CONNECTED = 2,
    MESSAGE_PLAYER_DISCONNECT = 3,
    MESSAGE_PLAYER_DISCONNECTED = 4,
    MESSAGE_GAME_START = 5,
    MESSAGE_GAME_WAIT = 6,
    MESSAGE_ERROR = 7,
} MessageType;

typedef struct {
    MessageType type;
    char content[128];
} Message;

Message message_init(const MessageType type, const char *content);

ssize_t protocol_send(const int socket_fd, const Message *message, const Address *destination);
ssize_t protocol_receive(const int socket_fd, Message *message, Address *source);
ssize_t protocol_wait(const int socket_fd, const Message *request, Message *response,
                      const MessageType type, const int timeout_seconds);

#endif