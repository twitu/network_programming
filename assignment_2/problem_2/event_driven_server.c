#define _BSD_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <math.h>
#include <poll.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
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
int listen_fd, maxfd;
fd_set rset, wset;
sem_t mutex;

int main() {
    sem_init(&mutex, 0, 1);
    pthread_t thread1, thread2;
    int iret1 = pthread_create(&thread1, NULL, event_generator, NULL);
    int iret2 = pthread_create(&thread2, NULL, event_handler, NULL);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL); 
    exit(0);
}

void event_generator(void) {

    // create listening address to bind
    struct sockaddr_in listen_addr;
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(LISTEN_PORT);
    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // create socket to listen for connections on 8000 and bind
    int listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    bind(listen_fd, (struct sockaddr*) &listen_addr, sizeof(struct sockaddr));

    fd_set temp_rset, temp_wset;
    FD_SET(listen_fd, &rset);

    maxfd = listen_fd + 1;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    int i;
    while (1) {
        sem_wait(&mutex);
        temp_rset = rset;
        temp_wset = wset;
        sem_post(&mutex);
        select(maxfd, &temp_rset, &temp_wset, NULL, &timeout);
        for (i = 0; i < maxfd; i++) {
            // check sockets iteratively for events
            if (FD_ISSET(i, &temp_rset)) {
                if (i == listen_fd){
                    // add default event to accept connection
                    add_event(create_event(-1));
                } else {
                    // add event from buffer to data from socket
                    add_event(get_from_buffer(i));
                    sem_wait(&mutex);
                    FD_CLR(i, &rset);
                    sem_post(&mutex);
                }
            } else if (FD_ISSET(i, &temp_wset)) {
                // add event from buffer to write data to socket
                add_event(get_from_buffer(i));
                sem_wait(&mutex);
                FD_CLR(i, &wset);
                sem_post(&mutex);
            }
        }
    }
}

void event_handler(void) {
    int bytes, conn;
    char resource[100];

    Event sock_event;
    while (1) {
        if (!has_event()) continue;
        sock_event = get_next_event();
        switch (sock_event->state) {
            case ACCEPT_EVENT:
                conn = accept(listen_fd, NULL, 0);
                fcntl(conn, F_SETFL, fcntl(conn, F_GETFL, 0) | O_NONBLOCK);
                maxfd = max(maxfd, conn) + 1;
                sock_event->socket_fd = conn;
                add_to_buffer(sock_event);
                sem_wait(&mutex);
                FD_SET(conn, &rset);
                sem_post(&mutex);
            case READ_EVENT:
                if ((bytes = recv(sock_event->socket_fd, &sock_event->request[sock_event->bytes_read], HTTP_REQ_LIMIT, 0)) < 0) {
                    if (errno != EWOULDBLOCK) {
                        close(sock_event->socket_fd);
                        sem_wait(&mutex);
                        FD_CLR(sock_event->socket_fd, &rset);
                        sem_post(&mutex);
                        free(sock_event);
                    } else {
                        add_to_buffer(sock_event);
                        sem_wait(&mutex);
                        FD_SET(sock_event->socket_fd, &rset);
                        sem_post(&mutex);
                    }
                } else {
                    sock_event->bytes_read += bytes;
                    sock_event->request[bytes] = '\0';
                    if (strcmp(&sock_event->request[bytes - 4], "\r\n\r\n")) {
                            // request received
                            sock_event->state = PROCESS_EVENT;
                            sprintf(sock_event->request, "GET /%s ", resource);
                            if (strstr(resource, ".cgi") == NULL) {
                                // call helper thread to load file  
                                sscanf(sock_event->request, "%s", resource);
                            } else {
                                // call fast cgi process
                            }
                    } else {
                        add_to_buffer(sock_event);
                        sem_wait(&mutex);
                        FD_SET(sock_event->socket_fd, &rset);
                        sem_post(&mutex);
                    }
                }
                break;
            case WRITE_EVENT:
                if ((bytes = send(sock_event->socket_fd, &sock_event->mapped_file[sock_event->bytes_written], sock_event->bytes_to_write, 0)) < 0) {
                    if (errno != EWOULDBLOCK) {
                        close(sock_event->socket_fd);
                        sem_wait(&mutex);
                        FD_CLR(sock_event->socket_fd, &wset);
                        sem_post(&mutex);
                        free(sock_event);
                    } else {
                        add_to_buffer(sock_event);
                        sem_wait(&mutex);
                        FD_SET(sock_event->socket_fd, &wset);
                        sem_post(&mutex);
                    }
                } else {
                    sock_event->bytes_written += bytes;
                    if (sock_event->bytes_to_write == sock_event->bytes_written) {
                        munmap(sock_event->mapped_file, sock_event->bytes_to_write);
                        close(sock_event->socket_fd);
                        free(sock_event);
                    } else {
                        add_to_buffer(sock_event);
                        sem_wait(&mutex);
                        FD_SET(sock_event->socket_fd, &wset);
                        sem_post(&mutex);
                    }
                }
                break;
        }
    }
}

void helper_thread(char* filename, Event sock_event) {
    int file_fd = open(filename, O_RDONLY);
    struct stat file_stat;
    fstat(file_fd, &file_stat);
    char* mem_ptr = (char*) mmap(NULL, file_stat.st_size, PROT_READ, MAP_SHARED, file_fd, 0);
    sock_event->mapped_file = mem_ptr;
    sock_event->bytes_to_write = file_stat.st_size;
    sock_event->state = WRITE_EVENT;
    add_to_buffer(sock_event);
    sem_wait(&mutex);
    FD_SET(sock_event->socket_fd, &wset);
    sem_post(&mutex);
}
