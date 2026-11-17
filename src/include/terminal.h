#ifndef TERMINAL_H
#define TERMINAL_H

void terminal_init(int width, int height, char *previous_screen, char *current_screen);
void terminal_render(const char *status, int width, int height, char *previous_screen,
                     char *current_screen);

#endif