#ifndef SESSION_H
#define SESSION_H

#include "game.h"
#include "protocol.h"
#include <pthread.h>
#include <time.h>

#define HEARTBEAT_INTERVAL 5
#define HEARTBEAT_TIMEOUT 15

/**
 * A Session represents an active connection between a client and your server, associated with a
 * particular player. It is responsible for managing the player's connection to the server,
 * including handling messages, maintaining the player's status, and ensuring the connection remains
 * active.
 *
 * Will consist of:
 * - Player
 * - Thread
 * - Message queue
 *
 *
 * Multithreaded Architecture: Session thread
 * - Waits for messages from its player.
 * - Processes those messages.
 * - Sends responses.
 * - Manages that player's heartbeat.
 * - Detects when that player times out.
 * - Handles the player's session cleanup.
 *
 *
 */

typedef struct timespec Time;

typedef struct {
    // Session information
    int id;
    Time last_activity;

    // Player information
    Address address;
    Player player;

    // Thread information
    pthread_t thread;
    bool active;

    // Server information
    int socket;

    // Message queues
    Message incoming[10];
    /**
     * INDIVIDUAL MESSAGE QUEUE FOR EACH SESSION THREAD
     * SHARED MESSAGE QUEUE FOR ALL SESSION THREADS
     */
} Session;

int session_connect(Session *session, const Player *player, const Address *address);
void session_process(Session *session, const Game *game, const Message *message);
void session_disconnect(Session *session);

#endif