#include "include/game.h"
#include <stdio.h>
#include <string.h>

int game_init(Game *game) {
    game->player_count = 0;
    game->state = STATE_WAITING;

    return 0;
}

int game_add_player(Game *game, const Player *player) {
    if (game->player_count >= MAX_PLAYERS) {
        return -1;
    }

    bool duplicate_player = false;
    for (size_t i = 0; i < game->player_count; i++) {
        const Player existing_player = game->players[i];
        if (strcmp(existing_player.id, player->id) == 0) {
            duplicate_player = true;
            break;
        }
    }
    if (duplicate_player) {
        return -1;
    }

    game->players[game->player_count] = *player;
    game->player_count++;

    return 0;
}

void game_remove_player(Game *game, const Player *player) {
    for (size_t i = 0; i < game->player_count; i++) {
        if (strcmp(game->players[i].id, player->id) == 0) {
            for (size_t j = i + 1; j < game->player_count; j++) {
                game->players[j - 1] = game->players[j];
            }
            game->player_count--;
            game->players[game->player_count] = (Player){0};
            break;
        }
    }
}

void game_serialize(const Game *game, char *buffer) {
    memset(buffer, 0, 128);
    buffer[0] = (char)game->state;
    buffer[1] = (char)game->player_count;

    for (size_t i = 0; i < game->player_count; i++) {
        const Player *player = &game->players[i];
        strncpy(buffer + 2 + (i * 34), player->id, MAX_PLAYER_ID_LENGTH - 1);
        buffer[2 + (i * 34) + MAX_PLAYER_ID_LENGTH - 1] = '\0';
        buffer[2 + (i * 34) + MAX_PLAYER_ID_LENGTH] = (char)player->x;
        buffer[2 + (i * 34) + MAX_PLAYER_ID_LENGTH + 1] = (char)player->y;
    }
}

void game_deserialize(Game *game, const char *buffer) {
    game->state = (State)buffer[0];
    game->player_count = (size_t)buffer[1];

    for (size_t i = 0; i < game->player_count; i++) {
        Player *player = &game->players[i];
        strncpy(player->id, buffer + 2 + (i * 34), MAX_PLAYER_ID_LENGTH - 1);
        player->id[MAX_PLAYER_ID_LENGTH - 1] = '\0';
        player->x = (int)buffer[2 + (i * 34) + MAX_PLAYER_ID_LENGTH];
        player->y = (int)buffer[2 + (i * 34) + MAX_PLAYER_ID_LENGTH + 1];
    }
}

void game_reset(Game *game) {
    game->player_count = 0;
    game->state = STATE_WAITING;
}