#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "nmb.h"

Message create_message(in_addr_t dest_ip, in_port_t dest_port, char* body) {
    if (strlen(body) >= BODY_LIMIT) return NULL;  
    Message new_message = (Message) malloc(sizeof(message));
    new_message->dest_addr = dest_ip;
    new_message->dest_port = dest_port;
    memset(new_message->body, 0, BODY_LIMIT);
    strcpy(new_message->body, body);
    return new_message;
}

int msgget_nmb(in_port_t port) {
    if (port < 1025) return -1;
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    // bind socket to localhost and specified port number
    struct sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    local.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr*) &local, sizeof(struct sockaddr)) == -1) return -1;
    struct sockaddr_in tcp_conn_addr;
    tcp_conn_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    tcp_conn_addr.sin_family = AF_INET;
    tcp_conn_addr.sin_port = htons(TCP_PORT);
    connect(sockfd, (struct sockaddr*) &tcp_conn_addr, sizeof(tcp_conn_addr));
    return sockfd;
}

Message msgrcv_nmb(int sockfd) {
    struct sockaddr_in conn;
    int conn_len = sizeof(conn);
    size_t count = 0, bytes = 0, total = sizeof(message);
    void* buffer = malloc(sizeof(message));
    void* pointer = buffer;
    while (count < total) {
        bytes = read(sockfd, pointer, total - count);
        if (bytes == -1) close(sockfd);
        else if (bytes > 0) {
            pointer += bytes;
            count += bytes;
        }
    }
    return (Message) buffer;
}

int msgsnd_nmb(int sockfd, in_addr_t dest_ip, in_port_t dest_port, char* body) {
    if (dest_port < 1025) return -1;
    Message to_send = create_message(dest_ip, dest_port, body);
    if (sendto(sockfd, to_send, sizeof(message), 0, NULL, 0) == -1) return -1;
    return 0;
}

void print_msg(Message info) {
    in_addr_t ip = info->dest_addr;
    if (info == NULL) printf("did not receive message");
    else {
        printf("Messaged received at ip: %d:%d:%d:%d, port: %d\n %s\n",
            (ip >> 24) & 0xFF,
            (ip >> 16) & 0xFF,
            (ip >> 8) & 0xFF,
            ip & 0xFF,
            info->dest_port,
            info->body
        );
    }
}
