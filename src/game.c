#include "include/game.h"
#include <string.h>

// Player *game_get_player(const Game *game, const Address *player_address) {
//     for (int i = 0; i < MAX_PLAYERS; i++) {
//         const Player *player = &game->players[i];
//         // Check if the player is active and if the address ip and port match the provided
//         address if (player->active && player->address.sin_addr.s_addr ==
//         player_address->sin_addr.s_addr &&
//             player->address.sin_port == player_address->sin_port) {
//             return (Player *)player;
//         }
//     }
//     return NULL;
// }

// Should "throw an error" if the game is full or if the player is already connected
// void game_add_player(Game *game, const Player *player) {
//     for (int i = 0; i < MAX_PLAYERS; i++) {
//         Player *slot = &game->players[i];
//         if (!slot->active) {
//             *slot = *player;
//             game->num_players++;
//             return;
//         }
//     }
// }

// Should "throw an error" if the player is not found
// void game_remove_player(Game *game, const char *player_id) {
//     for (int i = 0; i < MAX_PLAYERS; i++) {
//         Player *player = &game->players[i];
//         if (player->active && strcmp(player->id, player_id) == 0) {
//             *player = (Player){0};
//             game->num_players--;
//             return;
//         }
//     }
// }

void game_update(Game *game) {}
