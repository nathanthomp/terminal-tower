#include "include/session.h"
#include <stdio.h> // Debugging
#include <string.h>

static void *session_worker(void *arg) {
    Session *session = arg;

    printf("[INFO] Session thread started for player: %s:%d\n",
           inet_ntoa(session->address.sin_addr), ntohs(session->address.sin_port));

    while (session->active) {
        Time now, elapsed = {0};
        clock_gettime(CLOCK_MONOTONIC, &now);
        elapsed.tv_sec = now.tv_sec - session->last_activity.tv_sec;

        if (elapsed.tv_sec >= HEARTBEAT_TIMEOUT) {
            // session->active = false;
            // printf("[WARNING] Session timed out for player: %s:%d\n",
            //        inet_ntoa(session->address.sin_addr), ntohs(session->address.sin_port));
        } else if (elapsed.tv_sec >= HEARTBEAT_INTERVAL) {
            // Message ping = message_init(MESSAGE_PING, "Ping");
            // protocol_send(session->socket, &ping, &session->address);
        } else {
            /**
             * Still good
             */
        }
    }

    /**
     * Disconnect message
     */
    // Message disconnect = message_init(MESSAGE_PLAYER_DISCONNECTED, session->player.id);
    // server_broadcast(session->server, &disconnect);
    // printf("[INFO] %s disconnected\n", session->player.id);

    printf("[INFO] Session thread ended for player: %s:%d\n", inet_ntoa(session->address.sin_addr),
           ntohs(session->address.sin_port));

    return NULL;
}

int session_connect(Session *session, const Player *player, const Address *address) {
    *session = (Session){0};
    session->address = *address;
    session->player = *player;
    session->active = true;

    clock_gettime(CLOCK_MONOTONIC, &session->last_activity);

    int result = pthread_create(&session->thread, NULL, session_worker, session);
    if (result != 0) {
        session->active = false;
        fprintf(stderr, "[ERROR] Failed to create session thread for player %s: %s\n",
                session->player.id, strerror(result));
        return result;
    }

    return 0;
}

void session_process(Session *session, const Game *game, const Message *message) {
    clock_gettime(CLOCK_MONOTONIC, &session->last_activity);

    switch (message->type) {
    case MESSAGE_PING: {
        Message pong = message_init(MESSAGE_PONG, "Pong");
        protocol_send(session->socket, &pong, &session->address);
        break;
    }
    default:
        printf("[WARNING] Unhandled message type %d from player %s\n", message->type,
               session->player.id);
        break;
    }
}

void session_disconnect(Session *session) {
    if (session->active) {
        session->active = false;
        pthread_join(session->thread, NULL);
    }
}