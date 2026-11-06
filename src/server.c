#include "include/game.h"
#include "include/protocol.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

/**
 * The first implementation of broadcast versus unicast message dispatching is to use a same length
 * array with type address. If the message at i is unicast, the target at [i] is the source. If the
 * target at [i] is broadcast, the target at [i] is null or zero initialized.
 *
 * The second potential implementation, and most likely the better implementation, is to flatten out
 * broadcast messages into a list of messages each wth a different address based on who that
 * broadcast needs to go to.
 */
size_t server_process_message(Game *game, const Message *request, const Address *source,
                              Message *responses, Address *targets) {
    if (request->type == MESSAGE_PLAYER_CONNECT) {
        if (game->num_players >= MAX_PLAYERS) {
            responses[0] = message_init(MESSAGE_ERROR, "Game is full");
            targets[0] = *source;
            return 1;
        }
        for (int i = 0; i < MAX_PLAYERS; i++) {
            const Player *player = &game->players[i];
            if (player->active && strcmp(player->id, request->content) == 0) {
                responses[0] = message_init(MESSAGE_ERROR, "Player already connected");
                targets[0] = *source;
                return 1;
            }
        }
        game_add_player(game, request->content, source);
        responses[0] = message_init(MESSAGE_PLAYER_CONNECTED, request->content);
        printf("%s connected\n", request->content);
        if (game->num_players == MAX_PLAYERS) {
            game->state = STATE_READY;
            responses[1] = message_init(MESSAGE_GAME_READY, "Game is ready to start");
            targets[1] = (Address){0};
            printf("Game ready\n");
        } else {
            game->state = STATE_WAITING;
            responses[1] = message_init(MESSAGE_GAME_WAIT, "Waiting for more players");
            targets[1] = (Address){0};
        }
        return 2;
    }

    Player *player = game_get_player(game, source);
    if (player == NULL) {
        // Investigate: This is a potential security risk.
        return 0;
    }
    clock_gettime(CLOCK_MONOTONIC, &player->last_activity);

    switch (request->type) {
    case MESSAGE_PLAYER_DISCONNECT: {
        game_remove_player(game, request->content);
        printf("%s disconnected\n", request->content);
        responses[0] = message_init(MESSAGE_PLAYER_DISCONNECTED, request->content);
        targets[0] = (Address){0};
        return 1;
    }
    default:
        printf("Unsupported message type: %d\n", request->type);
        return 0;
    }
}

void server_dispatch_messages(int socket_fd, Game *game, Message *responses, Address *targets,
                              size_t response_len) {
    for (int i = 0; i < response_len; i++) {
        Address target = targets[i];
        if (target.sin_family == 0) {
            // broacast to all players
            for (int j = 0; j < MAX_PLAYERS; j++) {
                const Player *player = &game->players[j];
                if (player->active) {
                    protocol_send(socket_fd, &responses[i], &player->address);
                }
            }
        } else {
            // unicast to the specific player in targets[i]
            protocol_send(socket_fd, &responses[i], &target);
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

    printf("Server is running on %s:%s\n", server_ip, server_port);

    /**
     * Receive messages
     */
    Message responses[10];
    Address targets[10];
    while (1) {
        Address client = {0};
        Message request = {0};

        /**
         * Receive a message from a client. This will block until a message is received.
         */
        protocol_receive(socket_fd, &request, &client);

        /**
         * Process the message and generate responses.
         */
        size_t response_len = server_process_message(&game, &request, &client, responses, targets);

        /**
         * Send the responses to the appropriate clients.
         */
        server_dispatch_messages(socket_fd, &game, responses, targets, response_len);

        /**
         * Print the current state of the game
         */
        printf("[DEBUG] Current game state:\n");
        for (int i = 0; i < MAX_PLAYERS; i++) {
            const Player *player = &game.players[i];
            printf("[DEBUG] Player %d: (%s) ID=%s, Address=%s:%d\n", i,
                   player->active ? "Active" : "Inactive", player->id,
                   inet_ntoa(player->address.sin_addr), ntohs(player->address.sin_port));
        }
    }

    close(socket_fd);
    printf("Server has stopped.\n");
    return 0;
}