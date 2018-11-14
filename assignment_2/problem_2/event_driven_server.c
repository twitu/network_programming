#define _BSD_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "event.h"

#define LISTEN_PORT 8000
#define MAX_FILE_SIZE 2048
#define max(a,b) (a>b?a:b)

// global variables
char* shared_mem;

int main() {

    // create shared memory for single helper process
    shared_mem = mmap(NULL, MAX_FILE_SIZE, PROT_READ, MAP_ANONYMOUS | MAP_SHARED, -1, 0);

    // create listening address to bind
    struct sockaddr_in listen_addr;
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(LISTEN_PORT);
    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // create socket to listen for connections on 8000 and bind
    int listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    bind(listen_fd, (struct sockaddr*) &listen_addr, sizeof(struct sockaddr));

    fd_set rset, wset, temp_rset, temp_wset;
    FD_SET(listen_fd, &rset);

    int maxfd = listen_fd + 1;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    int i;
    while (1) {
        temp_rset = rset;
        temp_wset = wset;
        select(maxfd, &temp_rset, NULL, NULL, &timeout);
        for (i = 0; i < maxfd; i++) {
            if (!FD_ISSET(i, &temp_rset)) continue;
            if (listen_fd == i) {
                int conn = accept(listen_fd, NULL, 0);  // accept connection
                fcntl(conn, F_SETFL, fcntl(conn, F_GETFL, 0) | O_NONBLOCK);
                FD_SET(conn, &rset);
                maxfd = max(maxfd, conn) + 1;
            } else {
                if (!check_event(i)) create_event(i);
            }
        }
    }
}

void event_handler() {
    int bytes;
    char resource[100];

    Event sock_event;
    while (1) {
        if (!has_event()) continue;
        sock_event = get_event();
        switch (sock_event->state) {
            case READ_REQUEST:
                if ((bytes = recv(sock_event->socket_fd, &sock_event->request[sock_event->bytes_read], HTTP_REQ_LIMIT, 0)) < 0) {
                    if (errno != EWOULDBLOCK) {
                        close(sock_event->socket_fd);
                        delete_tail();
                    } 
                } else {
                    sock_event->bytes_read += bytes;
                    sock_event->request[bytes] = '\0';
                    sock_event->state = PROCESS_REQUEST;
                    if (strcmp(&sock_event->request[bytes - 4], "\r\n\r\n")) {
                            // request received
                            sprintf(sock_event->request, "GET /%s ", resource);
                            sscanf(shared_mem, "%s", resource);
                    }
                }
                break;
            case WRITE_RESULT:
                if ((bytes = send(sock_event->socket_fd, &sock_event->mapped_file[sock_event->bytes_written], sock_event->bytes_to_write, 0)) < 0) {
                    if (errno != EWOULDBLOCK) {
                        close(sock_event->socket_fd);
                        delete_tail();
                        break;
                    }
                } else {
                    sock_event->bytes_written += bytes;
                    if (sock_event->bytes_to_write == sock_event->bytes_written) {
                        munmap(sock_event->mapped_file, sock_event->bytes_to_write);
                        close(sock_event->socket_fd);
                        delete_tail();
                    }
                }
                break;
            case PROCESS_REQUEST:
                break;
            default:
                break;
        }
    }
}

void helper_process(void) {
    char resource[100];
    sprintf(shared_mem, "%s", resource);
    int file_fd = open(resource, O_RDONLY);
    struct stat file_stat;
    fstat(file_fd, &file_stat);
    sscanf(shared_mem, "%ld", file_stat.st_size);
    mmap(&shared_mem[sizeof(long)], file_stat.st_size, PROT_READ, MAP_ANONYMOUS | MAP_SHARED, file_fd, 0);
    kill(getppid(), SIGUSR1);
}

void signal_handler() {
    return;
}