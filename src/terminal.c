
#include <stdio.h>
#include <string.h>

void terminal_render(const char *status, int width, int height, char *previous_screen,
                     char *current_screen) {

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
}