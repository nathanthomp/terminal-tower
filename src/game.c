#include "include/game.h"
#include "include/protocol.h"
#include <string.h>

// Should "throw an error" if the game is full or if the player is already connected
void game_add_player(Game *game, const char *player_id, const Address *address) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Player *player = &game->players[i];
        if (!player->is_connected) {
            strncpy(player->id, player_id, sizeof(player->id) - 1);
            player->id[sizeof(player->id) - 1] = '\0';
            player->address = *address;
            player->is_connected = true;
            game->num_players++;
            return;
        }
    }
}

// Should "throw an error" if the player is not found
void game_remove_player(Game *game, const char *player_id) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Player *player = &game->players[i];
        if (player->is_connected && strcmp(player->id, player_id) == 0) {
            player->is_connected = false;
            game->num_players--;
            return;
        }
    }
}

void game_update(Game *game) {}
