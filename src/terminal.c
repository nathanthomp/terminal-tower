/**
 * This will be a wrapper around either termios or ncurses.
 */

#include "include/terminal.h"
#include <stdio.h>
#include <string.h>

void terminal_init(int width, int height, char *previous_screen, char *current_screen) {
    /**
     * Initialize the terminal
     */
    // int width = 0;
    // int height = 0;

    // struct winsize size;
    // if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == -1) {
    //     printf("[ERROR] Failed to get terminal size\n");
    //     close(client.socket);
    //     return EXIT_FAILURE;
    // }
    // width = size.ws_col;
    // height = size.ws_row;

    // printf("[INFO] Terminal size: %dx%d\n", width, height);

    // char previous_screen[height * width];
    // char current_screen[height * width];

    // terminal_init(width, height, previous_screen, current_screen);
    /**
     *
     */

    memset(previous_screen, ' ', width * height);
    memset(current_screen, 0, width * height);

    // Clear the terminal, move cursor to top-left, and hide cursor once.
    printf("\033[2J");
    printf("\033[H");
    printf("\033[?25l");

    fflush(stdout);
}

void terminal_render(const char *status, int width, int height, char *previous_screen,
                     char *current_screen) {
    memset(current_screen, ' ', width * height);

    // Draw border and corners
    for (int x = 0; x < width; x++) {
        current_screen[x] = '-';
        current_screen[(height - 1) * width + x] = '-';
    }
    for (int y = 0; y < height; y++) {
        current_screen[y * width] = '|';
        current_screen[y * width + width - 1] = '|';
    }
    current_screen[0] = '+';
    current_screen[width - 1] = '+';
    current_screen[(height - 1) * width] = '+';
    current_screen[(height - 1) * width + width - 1] = '+';

    // Draw header
    const char *header = "Terminal Tower";
    int header_length = strlen(header);
    int header_x = (width - header_length) / 2;
    int header_y = 1;
    int header_height = 1;

    for (int i = 0; i < header_length; i++) {
        current_screen[header_height * width + header_x + i] = header[i];
    }

    for (int x = 1; x < width - 1; x++) {
        current_screen[(header_height + 1) * width + x] = '-';
    }

    // Draw footer
    const char *footer = "Press 'q' to quit, 'p' to pause, 'r' to resume";
    int footer_length = strlen(footer);
    int footer_x = (width - footer_length) / 2;
    int footer_y = height - 2;
    for (int x = 1; x < width - 1; x++) {
        current_screen[(footer_y - 1) * width + x] = '-';
    }

    for (int i = 0; i < footer_length; i++) {
        current_screen[footer_y * width + footer_x + i] = footer[i];
    }

    int status_length = strlen(status);
    int status_x = (width - status_length) / 2;
    int status_y = height / 2;
    for (int i = 0; i < status_length; i++) {
        current_screen[status_y * width + status_x + i] = status[i];
    }

    // Draw player
    // int player_index = player_y * width + player_x;

    // if (player_x >= 1 && player_x < width - 1 && player_y >= 1 && player_y < height - 1) {
    //     current_screen[player_index] = 'P';
    // }

    // Render
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int index = y * width + x;
            if (current_screen[index] != previous_screen[index]) {
                printf("\033[%d;%dH", y + 1, x + 1);
                putchar(current_screen[index]);
                previous_screen[index] = current_screen[index];
            }
        }
    }

    fflush(stdout);
}