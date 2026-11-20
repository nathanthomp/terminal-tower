#ifndef GAME_H
#define GAME_H

#include <stdlib.h>

#define MAX_PLAYERS 2
#define MAX_PLAYER_ID_LENGTH 32

typedef struct {
    char id[MAX_PLAYER_ID_LENGTH];
    int x;
    int y;
} Player;

typedef enum { STATE_WAITING, STATE_READY, STATE_PLAYING, STATE_PAUSED, STATE_OVER } State;

typedef struct {
} Map;

typedef struct {
    Player players[MAX_PLAYERS];
    size_t player_count;

    State state;
    Map map;
} Game;

int game_init(Game *game);
int game_add_player(Game *game, const Player *player);
void game_remove_player(Game *game, const Player *player);

void game_serialize(const Game *game, char *buffer);
void game_deserialize(Game *game, const char *buffer);

#endif