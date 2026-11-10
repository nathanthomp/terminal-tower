/**
 * Game determines what messages need to be sent and who should receive them.
 *
 * This should inlcude the message and delivery.
 */
#ifndef GAME_H
#define GAME_H

#define MAX_PLAYERS 2
#define MAX_PLAYER_ID_LENGTH 32

typedef struct {
    char id[MAX_PLAYER_ID_LENGTH];
} Player;

typedef enum { STATE_WAITING, STATE_READY, STATE_PLAYING, STATE_PAUSED, STATE_OVER } State;

typedef struct {
} Map;

typedef struct {
    // Player players[MAX_PLAYERS];
    State state;
    Map map;

    // Session testing
    int pings;
} Game;

void game_add_player(Game *game, const Player *player);
void game_remove_player(Game *game, const char *player_id);

/**
 * game_process_input
 * game_process_message
 */

#endif