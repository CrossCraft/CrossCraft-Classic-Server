#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/signal.h>
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

    if(listen(listener_socket, 1) < 0) {
        perror("listen() failed");
        return -1;
    }
    
    return 0;
}

void stop_server_sock() {
    close(listener_socket);
}


int heartbeat_socket;
int start_heartbeat() {
    heartbeat_socket = (int)socket(AF_INET, SOCK_STREAM, 0);

    if(heartbeat_socket < 0) {
        perror("socket() failed");
        return -1;
    }

    struct sockaddr_in sockaddr;
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = INADDR_ANY;
    sockaddr.sin_port = htons(80);

    inet_pton(AF_INET, "104.20.2.125", &sockaddr.sin_addr.s_addr);

    if(connect(heartbeat_socket, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0) {
        perror("connect() failed");
        return -1;
    }

    
    char str[234] = {
        0x50, 0x4f, 0x53, 0x54, 0x20, 0x2f, 0x68, 0x65, 0x61, 0x72,
        0x74, 0x62, 0x65, 0x61, 0x74, 0x2e, 0x6a, 0x73, 0x70, 0x20, 0x48, 0x54, 0x54, 0x50, 0x2f, 0x31,
        0x2e, 0x31, 0x0d, 0x0a, 0x55, 0x73, 0x65, 0x72, 0x2d, 0x41, 0x67, 0x65, 0x6e, 0x74, 0x3a, 0x20,
        0x4d, 0x43, 0x47, 0x61, 0x6c, 0x61, 0x78, 0x79, 0x20, 0x31, 0x2e, 0x39, 0x2e, 0x34, 0x2e, 0x33,
        0x0d, 0x0a, 0x43, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d, 0x54, 0x79, 0x70, 0x65, 0x3a, 0x20,
        0x61, 0x70, 0x70, 0x6c, 0x69, 0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x2f, 0x78, 0x2d, 0x77, 0x77,
        0x77, 0x2d, 0x66, 0x6f, 0x72, 0x6d, 0x2d, 0x75, 0x72, 0x6c, 0x65, 0x6e, 0x63, 0x6f, 0x64, 0x65,
        0x64, 0x0d, 0x0a, 0x48, 0x6f, 0x73, 0x74, 0x3a, 0x20, 0x77, 0x77, 0x77, 0x2e, 0x63, 0x6c, 0x61,
        0x73, 0x73, 0x69, 0x63, 0x75, 0x62, 0x65, 0x2e, 0x6e, 0x65, 0x74, 0x0d, 0x0a, 0x43, 0x61, 0x63,
        0x68, 0x65, 0x2d, 0x43, 0x6f, 0x6e, 0x74, 0x72, 0x6f, 0x6c, 0x3a, 0x20, 0x6e, 0x6f, 0x2d, 0x73,
        0x74, 0x6f, 0x72, 0x65, 0x2c, 0x6e, 0x6f, 0x2d, 0x63, 0x61, 0x63, 0x68, 0x65, 0x0d, 0x0a, 0x50,
        0x72, 0x61, 0x67, 0x6d, 0x61, 0x3a, 0x20, 0x6e, 0x6f, 0x2d, 0x63, 0x61, 0x63, 0x68, 0x65, 0x0d,
        0x0a, 0x43, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d, 0x4c, 0x65, 0x6e, 0x67, 0x74, 0x68, 0x3a,
        0x20, 0x31, 0x35, 0x34, 0x0d, 0x0a, 0x43, 0x6f, 0x6e, 0x6e, 0x65, 0x63, 0x74, 0x69, 0x6f, 0x6e,
        0x3a, 0x20, 0x4b, 0x65, 0x65, 0x70, 0x2d, 0x41, 0x6c, 0x69, 0x76, 0x65, 0x0d, 0x0a, 0x0d, 0x0a,
    };

    char str2[154] = {
        0x26, 0x70, 0x6f, 0x72, 0x74, 0x3d, 0x32, 0x35, 0x35, 0x36,
        0x35, 0x26, 0x6d, 0x61, 0x78, 0x3d, 0x31, 0x36, 0x26, 0x6e, 0x61, 0x6d, 0x65, 0x3d, 0x25, 0x35,
        0x42, 0x4d, 0x43, 0x47, 0x61, 0x6c, 0x61, 0x78, 0x79, 0x25, 0x35, 0x44, 0x25, 0x32, 0x30, 0x44,
        0x65, 0x66, 0x61, 0x75, 0x6c, 0x74, 0x26, 0x70, 0x75, 0x62, 0x6c, 0x69, 0x63, 0x3d, 0x46, 0x61,
        0x6c, 0x73, 0x65, 0x26, 0x76, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x3d, 0x37, 0x26, 0x73, 0x61,
        0x6c, 0x74, 0x3d, 0x65, 0x46, 0x39, 0x7a, 0x7a, 0x58, 0x35, 0x4a, 0x70, 0x59, 0x4b, 0x7a, 0x79,
        0x75, 0x56, 0x72, 0x66, 0x6a, 0x4a, 0x4d, 0x68, 0x6d, 0x76, 0x55, 0x68, 0x43, 0x4e, 0x44, 0x44,
        0x74, 0x68, 0x4e, 0x26, 0x75, 0x73, 0x65, 0x72, 0x73, 0x3d, 0x30, 0x26, 0x73, 0x6f, 0x66, 0x74,
        0x77, 0x61, 0x72, 0x65, 0x3d, 0x4d, 0x43, 0x47, 0x61, 0x6c, 0x61, 0x78, 0x79, 0x25, 0x32, 0x30,
        0x31, 0x2e, 0x39, 0x2e, 0x34, 0x2e, 0x33, 0x26, 0x77, 0x65, 0x62, 0x3d, 0x54, 0x72, 0x75, 0x65,
    };

    send(heartbeat_socket, str, 234, 0);
    send(heartbeat_socket, str2, 154, 0);

    return 0;
}

void stop_heartbeat() {
    close(heartbeat_socket);
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

    struct timeval tv = {
        .tv_sec = 10,
        .tv_usec = 0,
    };

    if(setsockopt(conn, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0)
        perror("setsockopt()");

    if(setsockopt(conn, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        perror("setsockopt()");
    
    int flag = 1;
    setsockopt(conn, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

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

    int cnt = 0;
    while(total_sent < length && cnt < 15) {
        ssize_t result = send(fd, buffer, length, MSG_NOSIGNAL | MSG_DONTWAIT);

        if(result < 0) {
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                cnt++;
                usleep(1000);
                continue;
            }
            perror("send() failed");
            return -1;
        }
        
        total_sent += result;
    }

    if(cnt >= 15){
        return -1;
    }

    return total_sent;
}


int full_recv(const int fd, char* buf, const unsigned int length) {
    ssize_t total_recv = 0;

    while(total_recv < length) {
        ssize_t result = recv(fd, buf + total_recv, length - total_recv, 0);

        if(result < 0) {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            
            perror("recv() failed");
            return -1;
        }
        
        total_recv += result;
    }

    return total_recv;
}