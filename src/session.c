#include "include/session.h"
#include <stdio.h> // Debugging
#include <string.h>
#include <sys/random.h>

bool session_active(Session *session) {
    pthread_mutex_lock(&session->mutex);
    bool active = session->active;
    pthread_mutex_unlock(&session->mutex);
    return active;
}

static void *session_handler(void *arg) {
    Session *session = arg;

    printf("[INFO] Session started for: %s (%s:%d) with ID: %d\n", session->player.id,
           inet_ntoa(session->address.sin_addr), ntohs(session->address.sin_port), session->id);

    while (session_active(session)) {
        /**
         * Process all messages (network)
         */

        /**
         * Ping every 15 seconds
         */
        Time now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        pthread_mutex_lock(&session->mutex);
        Time last_activity = session->last_activity;
        pthread_mutex_unlock(&session->mutex);
        double elapsed =
            (now.tv_sec - last_activity.tv_sec) + (now.tv_nsec - last_activity.tv_nsec) / 1e9;
        if (elapsed > HEARTBEAT_TIMEOUT) {
            Message ping = message_init(MESSAGE_PING, "Ping");
            protocol_send(session->socket, &ping, &session->address);
            pthread_mutex_lock(&session->mutex);
            clock_gettime(CLOCK_MONOTONIC, &session->last_activity);
            pthread_mutex_unlock(&session->mutex);
        }
    }

    /**
     * Disconnect message
     */
    // Message disconnect = message_init(MESSAGE_PLAYER_DISCONNECTED, session->player.id);
    // server_broadcast(session->socket, &disconnect);
    // printf("[INFO] %s disconnected\n", session->player.id);

    printf("[INFO] Session ended for: %s (%s:%d) with ID: %d\n", session->player.id,
           inet_ntoa(session->address.sin_addr), ntohs(session->address.sin_port), session->id);

    return NULL;
}

int session_connect(Session *session, const Player *player, const Address *address,
                    const int socket) {
    *session = (Session){0};
    session->address = *address;
    session->player = *player;
    session->socket = socket;
    session->handler = 0;
    pthread_mutex_init(&session->mutex, NULL);
    clock_gettime(CLOCK_MONOTONIC, &session->last_activity);

    if (getrandom(&session->id, sizeof(session->id), 0) != sizeof(session->id)) {
        fprintf(stderr, "[ERROR] Failed to generate session id for:   %s (%s:%d)\n",
                session->player.id, inet_ntoa(session->address.sin_addr),
                ntohs(session->address.sin_port));
        return -1;
    }

    session->active = true;
    if (pthread_create(&session->handler, NULL, session_handler, session) != 0) {
        fprintf(stderr, "[ERROR] Failed to create session thread for  %s (%s:%d)\n",
                session->player.id, inet_ntoa(session->address.sin_addr),
                ntohs(session->address.sin_port));

        pthread_mutex_lock(&session->mutex);
        session->active = false;
        pthread_mutex_unlock(&session->mutex);
        return -1;
    }

    return 0;
}

void session_process(Session *session, const Game *game, const Message *message) {
    pthread_mutex_lock(&session->mutex);
    clock_gettime(CLOCK_MONOTONIC, &session->last_activity);
    pthread_mutex_unlock(&session->mutex);

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
        pthread_join(session->handler, NULL);
    }
}