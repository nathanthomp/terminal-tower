#ifndef MESSAGE_H
#define MESSAGE_H

typedef enum { MESSAGE_HELLO = 1, MESSAGE_GOODBYE = 2 } MessageType;

typedef struct {
    MessageType type;
    char content[128];
} Message;

#endif
