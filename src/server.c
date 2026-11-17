#include "include/game.h"
#include "include/protocol.h"
#include "include/session.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

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

void server_print(Game *game, Session *sessions) {
    printf("[DEBUG] Game state: %d\n", game->state);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        const Session session = sessions[i];
        printf("[DEBUG] Session[%d] %d: (%s) Player=%s, Address=%s:%d\n", i, session.id,
               session.active ? "Active" : "Inactive", session.player.id,
               inet_ntoa(session.address.sin_addr), ntohs(session.address.sin_port));
    }
}

Session *server_get_session(Session *sessions, const Address *address) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (sessions[i].active && sessions[i].address.sin_addr.s_addr == address->sin_addr.s_addr &&
            sessions[i].address.sin_port == address->sin_port) {
            return &sessions[i];
        }
    }
    return NULL;
}

int server_get_session_count(const Session *sessions) {
    int count = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (sessions[i].active) {
            count++;
        }
    }
    return count;
}

void server_unicast(int socket, const Address *address, const Message *message) {
    protocol_send(socket, message, address);
}

void server_broadcast(int socket, const Session *sessions, const Message *message) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        const Session session = sessions[i];
        if (session.active) {
            protocol_send(socket, message, &session.address);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "-h") == 0) {
        printf("Usage: %s -p <server_port>\n", argv[0]);
        return EXIT_SUCCESS;
    }

    if (argc < 3) {
        printf("Usage: %s -p <server_port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *server_port = argv[2];

    Address server_address = {0};
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(atoi(server_port));

    /**
     * Initialize server
     */
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    int bind_result = bind(socket_fd, (struct sockaddr *)&server_address, sizeof(server_address));
    if (bind_result < 0) {
        perror("Bind failed");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    Game game = {0};

    // Get server ip address
    char server_ip[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &server_address.sin_addr, server_ip, sizeof(server_ip)) == NULL) {
        perror("Failed to get server IP address");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    printf("[INFO] Server is running on %s:%s\n", server_ip, server_port);

    Session sessions[MAX_PLAYERS] = {0};

    // server_print(&game, sessions);

    while (1) {
        Address client = {0};
        Message request = {0};

        protocol_receive(socket_fd, &request, &client);
        Session *session = server_get_session(sessions, &client);

        if (session == NULL) {
            printf("[DEBUG] No session found for: %s:%d\n", inet_ntoa(client.sin_addr),
                   ntohs(client.sin_port));

            if (request.type != MESSAGE_PLAYER_CONNECT) {
                // Ignore messages from unknown players
                printf("[WARNING] Received message from unknown player: %s:%d\n",
                       inet_ntoa(client.sin_addr), ntohs(client.sin_port));
                continue;
            }

            int current_session_count = server_get_session_count(sessions);

            // If there are any inactive sessions.
            if (current_session_count >= MAX_PLAYERS) {
                Message message = message_init(MESSAGE_ERROR, "Game is full");
                server_unicast(socket_fd, &client, &message);
                continue;
            }

            // If there are any sessions with this player id.
            bool duplicate_player = false;
            for (int i = 0; i < MAX_PLAYERS; i++) {
                const Session session = sessions[i];
                if (session.active && strcmp(session.player.id, request.content) == 0) {
                    duplicate_player = true;
                    break;
                }
            }
            if (duplicate_player) {
                Message message = message_init(MESSAGE_ERROR, "Player already connected");
                server_unicast(socket_fd, &client, &message);
                continue;
            }

            Player player = {0};
            strcpy(player.id, request.content);

            Session *new_session = &sessions[current_session_count];
            if (session_connect(new_session, &player, &client, socket_fd) != 0) {
                printf("[ERROR] Failed to create session for player: %s:%d\n",
                       inet_ntoa(client.sin_addr), ntohs(client.sin_port));
                continue;
            }

            printf("[DEBUG] New session created for: %s (%s:%d) with ID: %d\n",
                   new_session->player.id, inet_ntoa(new_session->address.sin_addr),
                   ntohs(new_session->address.sin_port), new_session->id);

            /**
             * Move to session_handler thread.
             */
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "%d", new_session->id);
            Message ack = message_init(MESSAGE_PLAYER_CONNECT_ACK, buffer);
            server_unicast(socket_fd, &client, &ack);

            Message connect = message_init(MESSAGE_PLAYER_CONNECTED, new_session->player.id);
            server_broadcast(socket_fd, sessions, &connect);
            // printf("[INFO] %s connected\n", new_session->player.id);

            if (current_session_count + 1 == MAX_PLAYERS) {
                game.state = STATE_READY;
                Message message = message_init(MESSAGE_GAME_STATE, "Game is ready to start");
                server_broadcast(socket_fd, sessions, &message);
            } else {
                Message message = message_init(MESSAGE_GAME_STATE, "Waiting for more players");
                server_unicast(socket_fd, &client, &message);
            }
        } else {
            if (request.session_id != session->id) {
                printf("[WARNING] Received invalid session ID from player: %s:%d\n",
                       inet_ntoa(client.sin_addr), ntohs(client.sin_port));
                continue;
            }

            if (request.type == MESSAGE_PLAYER_DISCONNECT) {
                session_disconnect(session);
            } else {
                /**
                 * Add message to session's queue.
                 */
                session_process(session, &game, &request);
            }
        }

        // server_print(&game, sessions);
    }

    close(socket_fd);
    printf("Server has stopped.\n");
    return 0;
}