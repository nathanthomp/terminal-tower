#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <arpa/inet.h>
#include <stdint.h>

#define PROTOCOL_TIMEOUT_SECONDS 5

typedef struct sockaddr_in Address;

typedef int8_t Byte;
typedef uint8_t Flag;

/**
 * Seperate by client to server and server to client messages??
 *
 */
typedef enum {
    MESSAGE_PLAYER_CONNECT = 1,
    MESSAGE_PLAYER_CONNECTED = 2,
    MESSAGE_PLAYER_DISCONNECT = 3,
    MESSAGE_PLAYER_DISCONNECTED = 4,
    MESSAGE_GAME_WAIT = 5,
    MESSAGE_GAME_READY = 6,
    MESSAGE_GAME_START = 7,
    MESSAGE_GAME_STARTED = 8,
    MESSAGE_GAME_PAUSE = 9,
    MESSAGE_GAME_PAUSED = 10,

    MESSAGE_PING = 11,
    MESSAGE_PONG = 12,
    MESSAGE_ERROR = 13,
} MessageType;

typedef struct {
    MessageType type;
    int session_id;
    char content[128];
} Message;

Message message_init(const MessageType type, const char *content);

void protocol_send(const int socket_fd, const Message *message, const Address *destination);
void protocol_receive(const int socket_fd, Message *message, Address *source);
void protocol_wait(const int socket_fd, const Message *request, Message *response,
                   const MessageType type, const int timeout_seconds);

#endif