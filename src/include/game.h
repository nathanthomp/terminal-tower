#ifndef GAME_H
#define GAME_H

#include "protocol.h"

#define MAX_PLAYERS 2
#define MAX_PLAYER_ID_LENGTH 32

typedef struct {
    char id[MAX_PLAYER_ID_LENGTH];
    Address address;
    bool is_connected;
} Player;

typedef struct {
} Map;

typedef struct {
    Player players[MAX_PLAYERS];
    int num_players;
    Map map;
} Game;

void game_add_player(Game *game, const char *player_id, const Address *address);
void game_remove_player(Game *game, const char *player_id);

#endif