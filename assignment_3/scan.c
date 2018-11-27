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

fd_set result;

void* raw_socket(void* args);
void udp_port_checker(struct sockaddr_in check_address, int start, int end);
void tcp_port_checker(struct sockaddr_in check_address, int start, int end);

int main(int argc, char* argv[]) {
    if (argc != 3) perror("incorrect arguments\n");

    // start and end ports for scan
    int start = atoi(argv[1]);
    int end = atoi(argv[2]);

    struct sockaddr_in check_address;
    check_address.sin_family = AF_INET;
    check_address.sin_addr.s_addr = inet_addr("50.63.196.34");

    udp_port_checker(check_address, start, end);
}

void udp_port_checker(struct sockaddr_in check_address, int start, int end) {

    // use an fd_set to maintain a list of ports to be checked
    // used as a cheap substitute for an array
    FD_ZERO(&result);
    for (int i = start; i <= end; i++) {
        FD_SET(i, &result);
    }

    // liisten on raw socket in separate thread
    pthread_t raw_socket_thread;
    int thread1 = pthread_create(&raw_socket_thread, NULL, raw_socket, NULL);

    // write to ports iteratively
    // send port number as packet data because it is returned in ICMP error
    for (int i = start; i <= end; i++) {
        int sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        check_address.sin_port = htons(i);
        sendto(sock_fd, &i, sizeof(i), 0, (struct sockaddr*) &check_address, sizeof(struct sockaddr));
    }
    
    pthread_join(thread1, NULL);

    // result set has been modified by raw socket
    // set ports are open
    for (int i = start; i <= end; i++) {
        if (FD_ISSET(i, &result)) {
            printf("udp port %d is open\n", i);
        }
    }
    return;
}

void* raw_socket(void* args) {
    int sock_fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);

    // apply 10 second timeout
    struct timeval tv;
    tv.tv_usec = 10;
    tv.tv_usec = 0;
    setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));
    
    int error_fd;
    // TODO: exit thread after a certain time interval
    while (1) {
        if (recvfrom(sock_fd, &error_fd, sizeof(int), 0, NULL, 0) == -1) {
            if (errno != EAGAIN || errno != EWOULDBLOCK) {
                perror("recvfrom()");
            } else {
                // clear receive port from result set
                FD_CLR(error_fd, &result);
            }
        }
    }
}

void tcp_port_checker(struct sockaddr_in check_address, int start, int end) {

    struct timeval tval;
    fd_set rset, wset;

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
                FD_ZERO(&wset);
                FD_SET(sock_fd, &rset);
                FD_SET(sock_fd, &wset);
                tval.tv_sec = 1;
                tval.tv_usec = 0;

                if (select(sock_fd + 1, &rset, &wset, NULL, &tval) == 0) {
                    close(sock_fd);
                    #ifdef DEBUG
                    printf("tcp port %d not open\n", i);
                    #endif
                } else {
                    if (FD_ISSET(sock_fd, &rset) || FD_ISSET(sock_fd, &wset)) printf("tcp port %d open\n", i);
                }
            }
        } else {
            printf("tcp port %d open\n", i);
        }
        close(sock_fd);
    }
}
