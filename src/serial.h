#pragma once

#include <inttypes.h>

void serial_send(uint8_t data);
int serial_recv(void);
int serial_serverinit(int port);
int serial_clientinit(char *host, int port);
void serial_close(void);
