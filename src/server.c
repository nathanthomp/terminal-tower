#include "include/game.h"
#include "include/protocol.h"
#include "include/queue.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <unistd.h>

#define HEARTBEAT_INTERVAL 5
#define HEARTBEAT_TIMEOUT 15

/**
 * Dispatching
 *
 * The first implementation of broadcast versus unicast message dispatching is to use a same length
 * array with type address. If the message at i is unicast, the target at [i] is the source. If the
 * target at [i] is broadcast, the target at [i] is null or zero initialized.
 *
 * The second potential implementation, and most likely the better implementation, is to flatten out
 * broadcast messages into a list of messages each wth a different address based on who that
 * broadcast needs to go to.
 */

/**
 * Multithreading
 *
 * The first implementation of client session multithreading is to have a single thread for each
 * client session with the main loop running on its own thread routing messages to the appropriate
 * client session thread through a message queue.
 *
 * The main thread handles session management, while session threads handle established-player
 * activity.
 */
typedef struct timespec Time;

typedef enum { DELIVERY_UNICAST, DELIVERY_BROADCAST } Delivery;

typedef struct {
    Message message;
    Delivery delivery;
    Address target;
} DeliverableMessage;

typedef struct {
    uint16_t id;
    bool active;
    Time last_activity;
    Time last_ping;

    Address address;

    Game *game;
    Player player;

    pthread_t handler;
    pthread_mutex_t mutex;

    int socket;

    Message incoming_queue_elements[QUEUE_CAPACITY];
    Queue incoming_queue;

    Queue *outgoing_queue;
} Session;

typedef struct {
    bool running;

    Game game;
    Session sessions[MAX_PLAYERS];
    size_t session_count;

    int socket;

    DeliverableMessage outgoing_queue_elements[QUEUE_CAPACITY];
    Queue outgoing_queue;
} Server;

Session *server_get_session(Server *server, const Address *address) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (server->sessions[i].active &&
            server->sessions[i].address.sin_addr.s_addr == address->sin_addr.s_addr &&
            server->sessions[i].address.sin_port == address->sin_port) {
            return &server->sessions[i];
        }
    }
    return NULL;
}

/**
 *
 */

bool server_session_active(Session *session) {
    pthread_mutex_lock(&session->mutex);
    bool active = session->active;
    pthread_mutex_unlock(&session->mutex);
    return active;
}

void *server_session_handler(void *arg) {
    Session *session = arg;

    printf("[INFO] Player %s (%s:%d) connected to session %d\n", session->player.id,
           inet_ntoa(session->address.sin_addr), ntohs(session->address.sin_port), session->id);

    while (server_session_active(session)) {
        /**
         * Process all messages (network)
         */
        while (true) {
            Message message;
            if (!queue_dequeue(&session->incoming_queue, &message)) {
                break;
            }

            switch (message.type) {
            case MESSAGE_GAME_START: {
                printf("[INFO] Player %s (%s:%d) has started the game\n", session->player.id,
                       inet_ntoa(session->address.sin_addr), ntohs(session->address.sin_port));
                session->game->state = STATE_PLAYING;

                char buffer[128];
                game_serialize(session->game, buffer);

                DeliverableMessage game_state_message = {
                    .message = message_init(MESSAGE_GAME_STATE, buffer),
                    .delivery = DELIVERY_BROADCAST};

                queue_enqueue(session->outgoing_queue, &game_state_message);
                break;
            }
            case MESSAGE_GAME_PAUSE: {
                printf("[INFO] Player %s (%s:%d) has paused the game: %s\n", session->player.id,
                       inet_ntoa(session->address.sin_addr), ntohs(session->address.sin_port),
                       message.content);
                break;
            }
            case MESSAGE_PLAYER_MOVE: {
                if (strcmp(message.content, "left") == 0) {
                    session->player.x--;
                } else if (strcmp(message.content, "right") == 0) {
                    session->player.x++;
                }

                char buffer[128] = {0};

                game_serialize(session->game, buffer);
                // printf("[INFO] Sending game state: state=%d, players=%zu\n",
                // session->game->state,
                //        session->game->player_count);
                // for (size_t i = 0; i < session->game->player_count; i++) {
                //     const Player *player = &session->game->players[i];
                //     printf("[INFO] Player %zu: %s at (%d, %d)\n", i, player->id, player->x,
                //            player->y);
                // }
                Message message = {0};
                message.type = MESSAGE_GAME_STATE;
                memcpy(message.content, buffer, sizeof(message.content));
                DeliverableMessage move_message = {.message = message,
                                                   .delivery = DELIVERY_BROADCAST};

                queue_enqueue(session->outgoing_queue, &move_message);
                break;
            }
            default:
                break;
            }
        }

        /**
         * Ping after 5 seconds without activity and disconnect after 15 seconds.
         */
        Time now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        pthread_mutex_lock(&session->mutex);
        Time last_activity = session->last_activity;
        Time last_ping = session->last_ping;
        pthread_mutex_unlock(&session->mutex);
        double elapsed =
            (now.tv_sec - last_activity.tv_sec) + (now.tv_nsec - last_activity.tv_nsec) / 1e9;
        double ping_elapsed =
            (now.tv_sec - last_ping.tv_sec) + (now.tv_nsec - last_ping.tv_nsec) / 1e9;

        if (elapsed >= HEARTBEAT_TIMEOUT) {
            pthread_mutex_lock(&session->mutex);
            session->active = false;
            pthread_mutex_unlock(&session->mutex);
        } else if (elapsed >= HEARTBEAT_INTERVAL && ping_elapsed >= HEARTBEAT_INTERVAL) {
            Message ping = message_init(MESSAGE_PING, "Ping");
            protocol_send(session->socket, &ping, &session->address);
            pthread_mutex_lock(&session->mutex);
            clock_gettime(CLOCK_MONOTONIC, &session->last_ping);
            pthread_mutex_unlock(&session->mutex);
        }
    }

    DeliverableMessage disconnect = {
        .message = message_init(MESSAGE_PLAYER_DISCONNECTED, session->player.id),
        .delivery = DELIVERY_BROADCAST};
    queue_enqueue(session->outgoing_queue, &disconnect);

    printf("[INFO] Player %s (%s:%d) disconnected from session %d\n", session->player.id,
           inet_ntoa(session->address.sin_addr), ntohs(session->address.sin_port), session->id);

    return NULL;
}

int server_session_connect(Session *session, const Player *player, const Address *address,
                           const int socket, Queue *outgoing_queue, Game *game) {
    *session = (Session){0};
    session->address = *address;
    session->player = *player;
    session->game = game;
    session->socket = socket;
    session->handler = 0;

    clock_gettime(CLOCK_MONOTONIC, &session->last_activity);
    session->last_ping = session->last_activity;

    pthread_mutex_init(&session->mutex, NULL);

    session->outgoing_queue = outgoing_queue;
    queue_init(&session->incoming_queue, session->incoming_queue_elements, sizeof(Message));

    do {
        if (getrandom(&session->id, sizeof(session->id), 0) != sizeof(session->id)) {
            printf("[ERROR] Failed to generate session id for:   %s (%s:%d)\n", session->player.id,
                   inet_ntoa(session->address.sin_addr), ntohs(session->address.sin_port));
            return -1;
        }
    } while (session->id == 0);

    session->active = true;
    if (pthread_create(&session->handler, NULL, server_session_handler, session) != 0) {
        fprintf(stderr, "[ERROR] Failed to create session thread for  %s (%s:%d)\n",
                session->player.id, inet_ntoa(session->address.sin_addr),
                ntohs(session->address.sin_port));
        session->active = false;
        return -1;
    }

    return 0;
}

void server_session_process(Session *session, const Game *game, const Message *message) {
    pthread_mutex_lock(&session->mutex);
    queue_enqueue(&session->incoming_queue, message);
    clock_gettime(CLOCK_MONOTONIC, &session->last_activity);
    pthread_mutex_unlock(&session->mutex);
}

void server_session_disconnect(Session *session) {
    pthread_mutex_lock(&session->mutex);
    bool active = session->active;
    if (active) {
        session->active = false;
    }
    pthread_mutex_unlock(&session->mutex);

    if (active) {
        pthread_join(session->handler, NULL);
        queue_destroy(&session->incoming_queue);
    }
}

/**
 *
 */

void server_unicast(Server *server, const Address *address, const Message *message) {
    protocol_send(server->socket, message, address);
}

void server_broadcast(Server *server, const Message *message) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        const Session session = server->sessions[i];
        if (session.active) {
            protocol_send(server->socket, message, &session.address);
        }
    }
}

int server_init(Server *server, const Address *address) {
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    server->socket = socket_fd;

    int bind_result = bind(socket_fd, (struct sockaddr *)address, sizeof(*address));
    if (bind_result < 0) {
        perror("Bind failed");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    server->game = (Game){0};
    game_init(&server->game);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        server->sessions[i] = (Session){0};
    }

    queue_init(&server->outgoing_queue, server->outgoing_queue_elements,
               sizeof(DeliverableMessage));

    server->running = true;

    return EXIT_SUCCESS;
}

void server_process_incoming(Server *server, const Address *client, const Message *request) {
    Session *session = server_get_session(server, client);
    if (session == NULL) {
        if (request->type != MESSAGE_PLAYER_CONNECT) {
            // Ignore messages from unknown players
            printf("[WARNING] Received message from unknown player: %s:%d\n",
                   inet_ntoa(client->sin_addr), ntohs(client->sin_port));
            /**
             * TODO HERE
             */
            return;
        }

        Player player = {0};
        strcpy(player.id, request->content);

        if (game_add_player(&server->game, &player) != 0) {
            Message message = message_init(MESSAGE_ERROR, "Cannot add player to game");
            server_unicast(server, client, &message);

            /**
             * TODO HERE
             */
            return;
        }

        Session *new_session = NULL;
        for (size_t i = 0; i < MAX_PLAYERS; i++) {
            if (!server->sessions[i].active) {
                new_session = &server->sessions[i];
                break;
            }
        }
        if (new_session == NULL) {
            Message message = message_init(MESSAGE_ERROR, "Cannot add player to game");
            server_unicast(server, client, &message);
            return;
        }

        if (server_session_connect(new_session, &player, client, server->socket,
                                   &server->outgoing_queue, &server->game) != 0) {
            printf("[ERROR] Failed to create session for player: %s:%d\n",
                   inet_ntoa(client->sin_addr), ntohs(client->sin_port));
            /**
             * TODO HERE
             */
            return;
        }
        server->session_count++;

        /**
         * Move to session_handler thread.
         */
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%d", new_session->id);
        Message ack = message_init(MESSAGE_PLAYER_CONNECT_ACK, buffer);
        ack.session_id = new_session->id;
        server_unicast(server, client, &ack);

        Message connect = message_init(MESSAGE_PLAYER_CONNECTED, new_session->player.id);
        server_broadcast(server, &connect);

        if (server->game.player_count == MAX_PLAYERS) {
            server->game.state = STATE_READY;
        }
        char game_buffer[128];
        game_serialize(&server->game, game_buffer);
        Message game_state = {0};
        game_state.type = MESSAGE_GAME_STATE;
        game_state.session_id = new_session->id;
        memcpy(game_state.content, game_buffer, sizeof(game_state.content));
        server_broadcast(server, &game_state);
        /**
         * Move to session_handler thread.
         */

    } else {
        // if (request.session_id != session->id) {
        //     printf("[WARNING] Received invalid session ID from player: %s:%d\n",
        //            inet_ntoa(client.sin_addr), ntohs(client.sin_port));
        //     continue;
        // }

        if (request->type == MESSAGE_PLAYER_DISCONNECT) {
            server_session_disconnect(session);
            game_remove_player(&server->game, &session->player);
            server->session_count--;
            server->game.state = STATE_PAUSED;

            /**
             * Game will stay in pause
             */

            char game_buffer[128] = {0};
            game_serialize(&server->game, game_buffer);
            Message game_state = {0};
            game_state.type = MESSAGE_GAME_STATE;
            memcpy(game_state.content, game_buffer, sizeof(game_state.content));
            server_broadcast(server, &game_state);
        } else {
            server_session_process(session, &server->game, request);
        }
    }
}

void server_process_outgoing(Server *server) {
    while (true) {
        DeliverableMessage message;
        if (!queue_dequeue(&server->outgoing_queue, &message)) {
            break;
        }
        if (message.delivery == DELIVERY_BROADCAST) {
            server_broadcast(server, &message.message);
        } else if (message.delivery == DELIVERY_UNICAST) {
            server_unicast(server, &message.target, &message.message);
        }
    }
}

int main(int argc, char *argv[]) {
    if ((argc > 1 && strcmp(argv[1], "-h") == 0) || argc < 3) {
        printf("Usage: %s -p <server_port>\n", argv[0]);
        return EXIT_SUCCESS;
    }

    char *server_port = argv[2];

    Address server_address = {0};
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(atoi(server_port));

    Server server = {0};
    if (server_init(&server, &server_address) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    char server_ip[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &server_address.sin_addr, server_ip, sizeof(server_ip)) == NULL) {
        perror("Failed to get server IP address");
        close(server.socket);
        return EXIT_FAILURE;
    }

    printf("[INFO] Server is running on %s:%s\n", server_ip, server_port);

    while (server.running) {
        Address client = {0};
        Message request = {0};

        if (protocol_receive(server.socket, &request, &client) == RESULT_MESSAGE) {
            server_process_incoming(&server, &client, &request);
        }

        server_process_outgoing(&server);
    }

    close(server.socket);
    printf("Server has stopped.\n");
    return 0;
}