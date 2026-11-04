#include "include/game.h"
#include "include/protocol.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

ssize_t server_process_message(Game *game, const Message *request, const Address *source,
                               Message *responses) {
    switch (request->type) {
    case MESSAGE_PLAYER_CONNECT: {
        if (game->num_players >= MAX_PLAYERS) {
            responses[0] = message_init(MESSAGE_ERROR, "Game is full");
            return 1;
        }

        for (int i = 0; i < MAX_PLAYERS; i++) {
            const Player *player = &game->players[i];
            if (player->is_connected && strcmp(player->id, request->content) == 0) {
                responses[0] = message_init(MESSAGE_ERROR, "Player already connected");
                return 1;
            }
        }

        game_add_player(game, request->content, source);

        /**
         * Broadcast the player connected message to all players
         * If the game is full, send a game start message to all players
         */
        char connected_message[MAX_PLAYER_ID_LENGTH + 20];
        strcpy(connected_message, request->content);
        strcat(connected_message, " connected");

        responses[0] = message_init(MESSAGE_PLAYER_CONNECTED, connected_message);

        printf("%s connected\n", request->content);

        if (game->num_players == MAX_PLAYERS) {
            responses[1] = message_init(MESSAGE_GAME_START, "Game is starting");
            printf("Starting game\n");
        } else {
            responses[1] = message_init(MESSAGE_GAME_WAIT, "Waiting for more players");
        }

        return 2;
    }
    case MESSAGE_PLAYER_DISCONNECT: {
        printf("%s disconnected\n", request->content);

        game_remove_player(game, request->content);

        char disconnected_message[MAX_PLAYER_ID_LENGTH + 20];
        strcpy(disconnected_message, request->content);
        strcat(disconnected_message, " disconnected");

        responses[0] = message_init(MESSAGE_PLAYER_DISCONNECTED, disconnected_message);

        return 1;
    }
    default:
        printf("Unsupported message type: %d\n", request->type);
        return 0;
    }
}

int main() {
    char *server_port = "65432";

    Address server_address;
    memset(&server_address, 0, sizeof(server_address));
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
    while (1) {
        Address source;
        socklen_t addr_len = sizeof(source);

        Message request;
        ssize_t receive_len = protocol_receive(socket_fd, &request, &source);
        if (receive_len < 0) {
            perror("Failed to receive message");
            break;
        }

        /**
         * Process the received message
         */
        printf("[DEBUG] Received message: (%d) %s\n", request.type, request.content);

        size_t response_len = server_process_message(&game, &request, &source, responses);
        for (int i = 0; i < response_len; i++) {
            ssize_t bytes_sent = protocol_send(socket_fd, &responses[i], &source);
            if (bytes_sent < 0) {
                perror("Failed to send response");
            } else {
                printf("[DEBUG] Sent message: (%d) %s\n", responses[i].type, responses[i].content);
            }
        }

        for (int i = 0; i < MAX_PLAYERS; i++) {
            const Player *player = &game.players[i];
            printf("[DEBUG] Player %d: (%s) ID=%s, Address=%s:%d\n", i,
                   player->is_connected ? "Active" : "Inactive", player->id,
                   inet_ntoa(player->address.sin_addr), ntohs(player->address.sin_port));
        }
    }

    close(socket_fd);
    printf("Server has stopped.\n");
    return 0;
}