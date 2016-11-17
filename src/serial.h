#pragma once

#include <inttypes.h>

void serial_send(void);
int serial_recv(void);
int serial_serverinit(int port);
int serial_clientinit(char *host, int port);
void serial_close(void);
