#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>

void* raw_socket(void* args);
void udp_port_checker(struct sockaddr_in check_address, int start, int end);
void tcp_port_checker(struct sockaddr_in check_address, int start, int end);

int main(int argc, char* argv[]) {
    // if (argc != 2) perror("Invalid arguments");

    struct sockaddr_in check_address;
    check_address.sin_family = AF_INET;
    check_address.sin_addr.s_addr = inet_addr("50.63.196.34");

    udp_port_checker(check_address, 75, 85);
}

void udp_port_checker(struct sockaddr_in check_address, int start, int end) {
    pthread_t raw_socket_thread;
    int thread1 = pthread_create(&raw_socket_thread, NULL, raw_socket, NULL);

    for (int i = start; i <= end; i++) {
        int sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        check_address.sin_port = htons(i);
        sendto(sock_fd, &i, sizeof(i), 0, (struct sockaddr*) &check_address, sizeof(struct sockaddr));
    }
    
    pthread_join(thread1, NULL);
    return;
}

void* raw_socket(void* args) {
    int sock_fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    
    int error_sock;
    while (1) {
        recvfrom(sock_fd, &error_sock, sizeof(int), 0, NULL, 0);
        #ifdef DEBUG
        printf("udp port %d not open\n", error_sock);
        #endif
    }
}

void tcp_port_checker(struct sockaddr_in check_address, int start, int end) {

    struct timeval tval;
    fd_set rset;

    for (int i = start; i <= end; i++) {
        int sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        fcntl(sock_fd, F_SETFL, fcntl(sock_fd, F_GETFL, 0) | O_NONBLOCK);
        check_address.sin_port = htons(i);
        if (connect(sock_fd, (struct sockaddr*) &check_address, sizeof(struct sockaddr)) == -1) {
            if (errno != EINPROGRESS) {
                #ifdef DEBUG
                printf("tcp port %d not open\n", i);
                #endif
                close(sock_fd);
                continue;
            } else {
                FD_ZERO(&rset);
                FD_SET(sock_fd, &rset);
                tval.tv_sec = 1;
                tval.tv_usec = 0;

                if (select(sock_fd + 1, &rset, NULL, NULL, &tval) == 0) {
                    close(sock_fd);
                    #ifdef DEBUG
                    printf("tcp port %d not open\n", i);
                    #endif
                } else {
                    if (FD_ISSET(sock_fd, &rset)) printf("tcp port %d open\n", i);
                }
            }
        } else {
            printf("tcp port %d open\n", i);
        }
        close(sock_fd);
    }
}
