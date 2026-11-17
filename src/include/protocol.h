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
    MESSAGE_PLAYER_CONNECT_ACK = 2,
    MESSAGE_PLAYER_CONNECTED = 3,

    MESSAGE_PLAYER_DISCONNECT = 4,
    MESSAGE_PLAYER_DISCONNECTED = 5,

    MESSAGE_GAME_START = 8,
    MESSAGE_GAME_PAUSE = 9,

    MESSAGE_GAME_STATE = 10,

    MESSAGE_PING = 11,
    MESSAGE_PONG = 12,
    MESSAGE_ERROR = 13,

    MESSAGE_ANY = 127,
} Type;

typedef struct {
    Type type;
    int session_id;
    char content[128];
} Message;

typedef enum {
    RESULT_SUCCESS = 0,
    RESULT_ERROR = -1,
    RESULT_TIMEOUT = -2,
} Result;

Message message_init(const Type type, const char *content);

Result protocol_send(const int socket_fd, const Message *message, const Address *destination);
Result protocol_receive(const int socket_fd, Message *message, Address *source);
Result protocol_wait(const int socket_fd, Message *message, Address *source, const Type type,
                     const int timeout);

#endif