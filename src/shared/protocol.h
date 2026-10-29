#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "message.h"
#include <stdint.h>

typedef int8_t Byte;

int broadcast();
int unicast();

int serialize(const Message *message, char *buffer);
int deserialize(const char *buffer, Message *message);

#endif