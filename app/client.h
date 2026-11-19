#ifndef CLIENT_H
#define CLIENT_H

#include <stddef.h>

int client_connect(const char *host, const char *service, int timeout_ms);
int client_set_recv_timeout(int fd, int timeout_ms);
int client_send(int fd, const char *line);
int client_recv(int fd, char *out, size_t cap);
int client_exchange(int fd, const char *send, char *recv, size_t cap);
void client_close(int fd);

#endif
