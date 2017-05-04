#pragma once

#include <inttypes.h>

extern int serial_received;
extern int serial_sent;
extern int serial_remaining;
extern uint8_t serial_recv_buffer;

void serial_send(uint8_t data);
int serial_recv(void);
int serial_serverinit(int port);
int serial_clientinit(char *host, int port);
int serial_linked(void);
void serial_close(void);
