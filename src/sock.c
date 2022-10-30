#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include "sock.h"

int listener_socket;

int start_server_sock() {
    listener_socket = (int)socket(AF_INET, SOCK_STREAM, 0);

    if(listener_socket < 0) {
        perror("socket() failed");
        return -1;
    }

    int flags = fcntl(listener_socket, F_GETFL, 0);
    fcntl(listener_socket, F_SETFL, (flags | O_NONBLOCK));

    int flag = 1;
    setsockopt(listener_socket, SOL_SOCKET, SO_REUSEADDR, (void *)&flag, sizeof(int));

    struct sockaddr_in sockaddr;
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = INADDR_ANY;
    sockaddr.sin_port = htons(25565);

    if(bind(listener_socket, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0) {
        perror("bind() failed");
        return -1;
    }
    

    setsockopt(listener_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

    if(listen(listener_socket, 1) < 0) {
        perror("listen() failed");
        return -1;
    }
    
    return 0;
}

void stop_server_sock() {
    close(listener_socket);
}

Connection accept_new_conn() {
    struct sockaddr_in socketAddress;
    socketAddress.sin_family = AF_INET;
    socketAddress.sin_family = AF_INET;
    socketAddress.sin_addr.s_addr = INADDR_ANY;
    socketAddress.sin_port = htons(25565);

    int addressLen = sizeof(socketAddress);
    int conn = (int)(accept(listener_socket, (struct sockaddr *)&socketAddress, (socklen_t *)&addressLen));

    Connection connection;
    connection.fd = conn;

    if(conn < 0) {
        if(errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("accept() failed");
        }
        return connection;
    }

    char* ip = inet_ntoa(socketAddress.sin_addr);
    connection.ip = ip;
    printf("New connection from: %s on port %d\n", ip, socketAddress.sin_port);

    return connection;
}

void close_conn(int fd) {
    close(fd);
}

char peek_recv(const int fd) {
    char r = 0;

    if(recv(fd, &r, 1, MSG_PEEK) < 0) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            return -2;
        }
        perror("recv() failed");
        return -1;
    }

    return r;
}

int conn_send(const int fd, const char* buffer, const unsigned int length) {
    ssize_t total_sent = 0;

    while(total_sent < length) {
        ssize_t result = send(fd, buffer, length, 0);

        if(result < 0) {
            perror("send() failed");
            return -1;
        }
        
        total_sent += result;
    }

    return total_sent;
}


int full_recv(const int fd, char* buf, const unsigned int length) {
    ssize_t total_recv = 0;

    while(total_recv < length) {
        ssize_t result = recv(fd, buf + total_recv, length - total_recv, 0);

        if(result < 0) {
            perror("recv() failed");
            return -1;
        }
        
        total_recv += result;
    }

    return total_recv;
}