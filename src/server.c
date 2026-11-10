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
        printf("[DEBUG] Session %d: (%s) Player=%s, Address=%s:%d\n", i,
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
    /**
     * Parse arguments for the help flag. If provided, print the help message and exit.
     * If not provided, continue with the program.
     */

    /**
     * Parse aguments for server port. If not provided, exit with an error message.
     * If provided, validate the port. If invalid, exit with an error message.
     */
    char *server_port = "65432";

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

    /**
     * Multithreaded server architecture:
     *
     * Main thread
     * - Listens for incoming UDP messages.
     * - Identifies which player sent each message.
     * - Places incoming messages into that player's queue.
     * - Creates new session threads when players connect.

     * View of the server's main thread:
     * ON MESSAGE:
     * IF CONNECT:
     * - Finds an available player slot if available.
     * - Initializes the player's information.
     * - Creates a message queue for that player.
     * - Starts a new session thread.
     * ELSE:
     * - Determine player.
     * - Place message into that player's queue.
     * - Notify that player's session thread that a new message is available.
     *
     * View of a player's session thread:
     * WHILE SESSION IS ACTIVE:
     * - Wait for a message to be available in the queue.
     * - Process the message.
     * - Send responses.
     * - Update the player's heartbeat timer.
     * - If the heartbeat timer exceeds the timeout, terminate the session.
     *
     * The game variable is what will be shared between the main thread and the session threads. It
     * will use a mutex to ensure that only one thread can modify it at a
     * time.
     *
     * The main thread will handle the player disconnect message and remove the player
     * from the game. The session thread will handle the cleanup of the player's session
     * and terminate itself.
     *
     * Using thread safety, the player.active value will be set to false in the main
     * thread, and the session thread will check that value and terminate itself.
     */
    /**
     * Main server loop. Responsabilities include:
     * - Message routing
     * - Session management
     */
    Session sessions[MAX_PLAYERS] = {0};

    server_print(&game, sessions);

    while (1) {
        Address client = {0};
        Message request = {0};

        protocol_receive(socket_fd, &request, &client);
        Session *session = server_get_session(sessions, &client);

        if (session == NULL) {
            printf("[DEBUG] No session found for player: %s:%d\n", inet_ntoa(client.sin_addr),
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
            if (session_connect(new_session, &player, &client) != 0) {
                printf("[ERROR] Failed to create session for player: %s:%d\n",
                       inet_ntoa(client.sin_addr), ntohs(client.sin_port));
                continue;
            }

            Message connect = message_init(MESSAGE_PLAYER_CONNECTED, new_session->player.id);
            server_broadcast(socket_fd, sessions, &connect);
            printf("[INFO] %s connected\n", new_session->player.id);

            if (current_session_count + 1 == MAX_PLAYERS) {
                game.state = STATE_READY;
                Message message = message_init(MESSAGE_GAME_READY, "Game is ready to start");
                server_broadcast(socket_fd, sessions, &message);
            } else {
                Message message = message_init(MESSAGE_GAME_WAIT, "Waiting for more players");
                server_unicast(socket_fd, &client, &message);
            }
        } else {
            // if (request.session_id != session->id) {
            //     printf("[WARNING] Received invalid session ID from player: %s:%d\n",
            //            inet_ntoa(client.sin_addr), ntohs(client.sin_port));
            //     continue;
            // }

            if (request.type == MESSAGE_PLAYER_DISCONNECT) {
                session_disconnect(session);
                continue;
            }

            /**
             * Add message to session's queue.
             */
            session_process(session, &game, &request);
        }

        server_print(&game, sessions);
    }

    close(socket_fd);
    printf("Server has stopped.\n");
    return 0;
}