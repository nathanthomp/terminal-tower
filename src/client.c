#include "include/game.h"
#include "include/protocol.h"
#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

typedef enum { STATUS_CONNECTING, STATUS_CONNECTED, STATUS_DISCONNECTED, STATUS_ERRORED } Status;

typedef struct {
    bool running;
    int socket;
    Address server;

    Status status;
    char id[MAX_PLAYER_ID_LENGTH];

    pthread_t network_handler;
    pthread_t input_handler;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
} Client;

bool client_running(Client *client) {
    pthread_mutex_lock(&client->mutex);
    bool running = client->running;
    pthread_mutex_unlock(&client->mutex);
    return running;
}

void client_stop(Client *client) {
    pthread_mutex_lock(&client->mutex);
    client->running = false;
    pthread_mutex_unlock(&client->mutex);
}

void client_set_status(Client *client, Status status) {
    pthread_mutex_lock(&client->mutex);
    client->status = status;
    pthread_mutex_unlock(&client->mutex);
}

void *client_network_handler(void *arg) {
    Client *client = (Client *)arg;

    Message connect_request = message_init(MESSAGE_PLAYER_CONNECT, client->id);
    protocol_send(client->socket, &connect_request, &client->server);

    Message connect_response;
    if (protocol_wait(client->socket, &connect_response, &client->server,
                      MESSAGE_PLAYER_CONNECT_ACK, PROTOCOL_TIMEOUT_SECONDS) != RESULT_SUCCESS) {
        client_set_status(client, STATUS_ERRORED);
        printf("[ERROR] Unable to connect to server\n");
        client_stop(client);
        return NULL;
    }

    client_set_status(client, STATUS_CONNECTED);

    printf("[INFO] Connected to server session: %d\n", atoi(connect_response.content));

    while (client_running(client)) {
        Message message;
        Result result = protocol_wait(client->socket, &message, &client->server, MESSAGE_ANY,
                                      PROTOCOL_TIMEOUT_SECONDS);
        if (result == RESULT_SUCCESS) {
            switch (message.type) {
            case MESSAGE_PING: {
                Message pong = message_init(MESSAGE_PONG, "Pong");
                protocol_send(client->socket, &pong, &client->server);
                break;
            }
            case MESSAGE_ERROR: {
                printf("[ERROR] Server error: %s\n", message.content);
                client_stop(client);
                break;
            }
            default:
                printf("[DEBUG] Received message from server: (%d) %s\n", (Type)message.type,
                       message.content);
                break;
            }
        } else if (result == RESULT_ERROR) {
            printf("[ERROR] Failed to receive message from server\n");
        } else if (result == RESULT_TIMEOUT) {
            // printf("[DEBUG] Waiting for message (timed out)\n");
        }
    }

    client_set_status(client, STATUS_DISCONNECTED);

    Message disconnect = message_init(MESSAGE_PLAYER_DISCONNECT, "Disconnecting");
    protocol_send(client->socket, &disconnect, &client->server);

    return NULL;
}

void *client_input_handler(void *arg) {
    Client *client = (Client *)arg;

    /**
     * Discover keyboard devices.
     */
    int device_fds[15] = {0};
    DIR *directory = opendir("/dev/input/");

    if (directory == NULL) {
        perror("opendir");
        return NULL;
    }

    int device_count = 0;
    struct dirent *entry;

    while ((entry = readdir(directory)) != NULL) {
        if (strncmp(entry->d_name, "event", 5) != 0) {
            continue;
        }

        char path[267];
        snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);

        int fd = open(path, O_RDONLY | O_NONBLOCK);

        if (fd < 0) {
            perror("open");
            continue;
        }

        unsigned long event_bits[EV_MAX / (sizeof(unsigned long) * 8) + 1] = {0};
        if (ioctl(fd, EVIOCGBIT(0, sizeof(event_bits)), event_bits) < 0) {
            close(fd);
            continue;
        }

        unsigned long key_bits[KEY_MAX / (sizeof(unsigned long) * 8) + 1] = {0};
        if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0) {
            close(fd);
            continue;
        }

        bool has_a = key_bits[KEY_A / (sizeof(unsigned long) * 8)] &
                     (1UL << (KEY_A % (sizeof(unsigned long) * 8)));

        bool has_enter = key_bits[KEY_ENTER / (sizeof(unsigned long) * 8)] &
                         (1UL << (KEY_ENTER % (sizeof(unsigned long) * 8)));

        bool has_space = key_bits[KEY_SPACE / (sizeof(unsigned long) * 8)] &
                         (1UL << (KEY_SPACE % (sizeof(unsigned long) * 8)));

        if (has_a && has_enter && has_space) {
            if (device_count >= 15) {
                close(fd);
                continue;
            }

            // char name[256] = {0};

            // if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0) {
            //     printf("[INFO] Added keyboard device: %s (%s)\n", name, entry->d_name);
            // }

            device_fds[device_count++] = fd;
        } else {
            close(fd);
        }
    }

    closedir(directory);

    if (device_count == 0) {
        printf("[ERROR] No keyboard devices found\n");
        return NULL;
    }

    // printf("[INFO] Found %d keyboard devices\n", device_count);

    /**
     * Read events from keyboard devices.
     */
    while (client_running(client)) {
        for (int i = 0; i < device_count; i++) {
            struct input_event event;
            ssize_t bytes = read(device_fds[i], &event, sizeof(event));

            if (bytes == sizeof(event)) {
                if (event.type == EV_KEY) {
                    if (event.code == KEY_Q && event.value == 1) {
                        client_stop(client);
                        printf("[INFO] Quitting...\n");
                    }
                }
            }
        }
    }

    /**
     * Close devices.
     */
    for (int i = 0; i < device_count; i++) {
        close(device_fds[i]);
    }

    return NULL;
}

int client_resolve_address(const char *server_name, const char *server_port, Address *server) {
    /**
     * Commented out for testing purposes.
     */
    // struct addrinfo hints = {0};
    // hints.ai_family = AF_INET;
    // hints.ai_socktype = SOCK_DGRAM;
    // hints.ai_protocol = IPPROTO_UDP;

    // struct addrinfo *result = NULL;
    // int status = getaddrinfo(server_name, server_port, &hints, &result);
    // if (status != 0) {
    //     printf("[ERROR] Failed to resolve server address: %s\n", gai_strerror(status));
    //     return EXIT_FAILURE;
    // }

    // *server = *(struct sockaddr_in *)result->ai_addr;
    // freeaddrinfo(result);

    server->sin_family = AF_INET;
    server->sin_port = htons(atoi(server_port));
    if (inet_pton(AF_INET, "0.0.0.0", &server->sin_addr) <= 0) {
        printf("[ERROR] Invalid server address: %s\n", server_name);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int client_init(Client *client, Address *server) {
    pthread_mutex_init(&client->mutex, NULL);
    pthread_cond_init(&client->condition, NULL);

    client->running = true;
    client->server = *server;

    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd == -1) {
        printf("[ERROR] Socket failed\n");
        return EXIT_FAILURE;
    }

    client->socket = socket_fd;

    char player_id[MAX_PLAYER_ID_LENGTH];
    printf("Enter player id: ");
    if (fgets(player_id, sizeof(player_id), stdin) == NULL) {
        printf("[ERROR] Bad input\n");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    player_id[strcspn(player_id, "\n")] = 0;
    strncpy(client->id, player_id, sizeof(client->id) - 1);
    client->id[sizeof(client->id) - 1] = '\0';

    if (pthread_create(&client->network_handler, NULL, client_network_handler, client) != 0) {
        printf("[ERROR] Failed to create network listener thread\n");
        close(client->socket);
        return EXIT_FAILURE;
    }

    // if (pthread_create(&client->input_handler, NULL, client_input_handler, client) != 0) {
    //     printf("[ERROR] Failed to create input listener thread\n");
    //     close(client->socket);
    //     return EXIT_FAILURE;
    // }

    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "-h") == 0) {
        printf("Usage: %s -i <server_name> -p <server_port>\n", argv[0]);
        return EXIT_SUCCESS;
    }

    if (argc < 5) {
        printf("Usage: %s -i <server_name> -p <server_port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *server_name = argv[2];
    char *server_port = argv[4];

    Address server = {0};
    if (client_resolve_address(server_name, server_port, &server) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    Client client = {0};
    if (client_init(&client, &server) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    while (client_running(&client)) {
        /**
         * 1. Process all messages (network)
         */
        /**
         * 2. Process all events (input)
         */
        /**
         * 3. Update game state
         */
        /**
         * 4. Render game
         */
        /**
         * 5. Wait for next frame ~30 FPS.
         */
        usleep(33333);
    }

    /**
     * Cleanup
     */
    pthread_join(client.network_handler, NULL);
    // pthread_join(client.input_handler, NULL);

    close(client.socket);

    return EXIT_SUCCESS;
}