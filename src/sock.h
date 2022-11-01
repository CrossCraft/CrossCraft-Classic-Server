#ifndef SOCK_H
#define SOCK_H

int start_server_sock();
void stop_server_sock();

typedef struct {
    int fd;
    char* ip;
} Connection;

Connection accept_new_conn();
void close_conn(int fd);

signed char peek_recv(const int fd);
int full_recv(const int fd, char* buf, const unsigned int length);
int conn_send(const int fd, const char* buffer, const unsigned int length);

int start_heartbeat();
void stop_heartbeat();

#endif