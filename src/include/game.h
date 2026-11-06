#ifndef GAME_H
#define GAME_H

#include "protocol.h"
#include <time.h>

#define MAX_PLAYERS 2
#define MAX_PLAYER_ID_LENGTH 32

typedef struct timespec Time;

typedef struct {
    char id[MAX_PLAYER_ID_LENGTH];
    Address address;
    bool active;
    Time last_activity;
} Player;

typedef enum { STATE_WAITING, STATE_READY, STATE_PLAYING, STATE_PAUSED, STATE_OVER } State;

typedef struct {
} Map;

typedef struct {
    Player players[MAX_PLAYERS];
    int num_players;
    State state;
    Map map;
} Game;

Player *game_get_player(const Game *game, const Address *address);
void game_add_player(Game *game, const char *player_id, const Address *address);
void game_remove_player(Game *game, const char *player_id);

#endif