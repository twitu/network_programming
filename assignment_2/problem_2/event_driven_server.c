#define _BSD_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "event.h"
#include "data.h"

#define LISTEN_PORT 8000
#define MAX_BUFFER_SIZE 2048
#define MAX_CLIENTS 200

// global variables
int epfd;

// function stubs
void* event_generator(void* args);
void* event_handler(void* args);

int main() {
    pthread_t thread1, thread2;
    int iret1 = pthread_create(&thread1, NULL, event_generator, NULL);
    int iret2 = pthread_create(&thread2, NULL, event_handler, NULL);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL); 
    exit(0);
}

void* event_generator(void* args) {

    int bytes, conn;
    char request[HTTP_REQ_LIMIT];
    char resource[RESOURCE_LIMIT];

    // initialize data structures to store data
    create_data_store(MAX_CLIENTS);

    // create listening address to bind
    struct sockaddr_in listen_addr;
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(LISTEN_PORT);
    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // create non-blocking socket to listen for connections on 8000 and bind
    int enable = 1;
    int listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    fcntl(listen_fd, F_SETFL, 0);
    bind(listen_fd, (struct sockaddr*) &listen_addr, sizeof(struct sockaddr));
    listen(listen_fd, 5);

    // initialize structures for epoll
    struct epoll_event listener_event, events[MAX_CLIENTS + 1];
    listener_event.data.fd = listen_fd;
    listener_event.events = EPOLLIN | EPOLLET;

    // register epfd to register maximum client sockets and listener socket
    int epfd = epoll_create(MAX_CLIENTS + 1);
    
    // register listener to epoll fd
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &listener_event);

    int event_count, event_fd;
    Data event_data;
    while (1) {
        event_count = epoll_wait(epfd, events, MAX_CLIENTS + 1, 10000);
        for (int i = 0; i < event_count; i++) {
            // check events iteratively for events
            #ifdef DEBUG
            printf("events to process %d\n", event_count);
            #endif

            // accept connections
            if (events[i].data.fd == listen_fd) {
                // if there is no space in data store accept and close connections
                // this is done to avoid indefinetly blocking on listen_fd
                // this is a common issue with edge triggered systems
                while (!has_space()) {
                    #ifdef DEBUG
                    printf("no space dropping connections\n");
                    #endif
                    conn = accept(listen_fd, NULL, 0);
                    close(conn);
                }

                while (1) {
                    if ((conn = accept(listen_fd, NULL, 0)) == -1) {
                        if (errno == EAGAIN | errno == EWOULDBLOCK) break;
                        else perror("accept()");
                    } else {
                        #ifdef DEBUG
                        printf("connected on socket %d\n", conn);
                        #endif
                        fcntl(conn, F_SETFL, fcntl(conn, F_GETFL, 0) | O_NONBLOCK);
                        new_data_slot(conn);
                        struct epoll_event new_event;
                        new_event.data.fd = conn;
                        new_event.events = EPOLLIN | EPOLLET;  // receive only read events to get complete http request
                        epoll_ctl(epfd, EPOLL_CTL_ADD, conn, &new_event);
                    }
                }
            } else {
                if (events[i].events == EPOLLIN) {
                    event_data = get_event_data(events[i].data.fd);
                    // ignore if data not in READ_STATE
                    if (event_data->state == READ_STATE) {    
                        conn = events[i].data.fd;
                        bytes = event_data->bytes_read;

                        if ((bytes = recv(conn, &event_data->request[bytes], HTTP_REQ_LIMIT - bytes, 0)) == -1) {
                            // read nothing
                            if (errno == EAGAIN | errno == EWOULDBLOCK);
                            else perror("recv()");
                        } else {
                            event_data->bytes_read += bytes;
                            if (strstr(event_data->request, "\r\n\r\n")) {
                                // request received
                                event_data->state = PROCESS_STATE;
                                sprintf(event_data->request, "GET /%s ", resource);
                                if (strstr(resource, ".cgi") == NULL) {
                                    // call helper thread to load file  
                                    sscanf(event_data->resource, "%s", resource);

                                    // map file to memory
                                    int file_fd = open(resource, O_RDONLY);
                                    struct stat file_stat;
                                    fstat(file_fd, &file_stat);
                                    event_data->mapped_file = (char*) mmap(NULL, file_stat.st_size, PROT_READ, MAP_PRIVATE, file_fd, 0);
                                    mlock(event_data->mapped_file, file_stat.st_size);

                                    // modify to write state and change socket events for OUT
                                    event_data->state = WRITE_STATE;
                                    struct epoll_event modify;
                                    modify.data.fd = conn;
                                    modify.events = EPOLLOUT | EPOLLET;
                                    epoll_ctl(epfd, EPOLL_CTL_MOD, conn, &modify);
                                    #ifdef DEBUG
                                    printf("received complete request\n");
                                    #endif
                                } else {
                                    // TODO call fast cgi process
                                }
                            }

                            #ifdef DEBUG
                            printf("read %d bytes\n", bytes);
                            #endif
                        }
                    }
                } else if (events[i].events == EPOLLOUT) {
                    #ifdef DEBUG
                    printf("write event\n");
                    #endif

                    event_data = get_event_data(events[i].data.fd);
                    // ignore if data not in WRITE_STATE
                    if (event_data->state == WRITE_STATE) {    
                        conn = events[i].data.fd;
                        bytes = event_data->bytes_written;

                        if ((bytes = sendto(conn, &event_data->mapped_file[bytes], event_data->bytes_to_write - bytes, 0, NULL, 0)) == -1) {
                            if (errno == EAGAIN | errno == EWOULDBLOCK);
                            else perror("send()");
                        } else {
                            event_data->bytes_written += bytes;
                            if (event_data->bytes_to_write == event_data->bytes_written) {
                                close(conn);
                                munlock(event_data->mapped_file, event_data->bytes_to_write);
                                delete_event_data(conn);
                            }
                        }
                    }
                }
            }
        }
    }
}

void* event_handler(void* args) {

    int bytes, conn;
    char request[HTTP_REQ_LIMIT];
    char resource[RESOURCE_LIMIT];

    // initialize data structures to store data
    create_data_store(MAX_CLIENTS);

    Event sock_event;
    Data event_data;
    while (1) {
        if (!has_event()) continue;
        sock_event = get_next_event();
        if (sock_event->event == ACCEPT_EVENT) {
            #ifdef DEBUG
                printf("accept event\n");
            #endif
            
            // if there is no space in data store accept and close connections
            // this is done to avoid indefinetly blocking on listen_fd
            // this is a common issue with edge triggered systems
            while (!has_space()) {
                #ifdef DEBUG
                printf("no space dropping connections\n");
                #endif
                conn = accept(sock_event->sock_fd, NULL, 0);
                close(conn);
            }

            if (conn = accept(sock_event->sock_fd, NULL, 0) == -1) {
                if (errno == EAGAIN | errno == EWOULDBLOCK);
                else perror("accept()");
            } else {
                #ifdef DEBUG
                printf("connected on socket %d\n", conn);
                #endif
                fcntl(conn, F_SETFL, fcntl(conn, F_GETFL, 0) | O_NONBLOCK);
                new_data_slot(conn);
                struct epoll_event new_event;
                new_event.data.fd = conn;
                new_event.events = EPOLLIN | EPOLLET;  // receive only read events to get complete http request
                epoll_ctl(epfd, EPOLL_CTL_ADD, conn, &new_event);
            }
        } else if (sock_event->event == READ_EVENT) {
            #ifdef DEBUG
                printf("read event\n");
            #endif

            event_data = get_event_data(sock_event->sock_fd);
            // ignore if data not in READ_STATE
            if (event_data->state == READ_STATE) {    
                conn = sock_event->sock_fd;
                bytes = event_data->bytes_read;

                if ((bytes = recv(conn, &event_data->request[bytes], HTTP_REQ_LIMIT - bytes, 0)) == -1) {
                    // read nothing
                    if (errno == EAGAIN | errno == EWOULDBLOCK);
                    else perror("recv()");
                } else {
                    event_data->bytes_read += bytes;
                    if (strstr(event_data->request, "\r\n\r\n")) {
                        // request received
                        event_data->state = PROCESS_STATE;
                        sprintf(event_data->request, "GET /%s ", resource);
                        if (strstr(resource, ".cgi") == NULL) {
                            // call helper thread to load file  
                            sscanf(event_data->resource, "%s", resource);

                            // map file to memory
                            int file_fd = open(resource, O_RDONLY);
                            struct stat file_stat;
                            fstat(file_fd, &file_stat);
                            event_data->mapped_file = (char*) mmap(NULL, file_stat.st_size, PROT_READ, MAP_PRIVATE, file_fd, 0);
                            mlock(event_data->mapped_file, file_stat.st_size);
                            event_data->state = WRITE_STATE;
                            struct epoll_event modify;
                            modify.data.fd = conn;
                            modify.events = EPOLLOUT | EPOLLET;
                            epoll_ctl(epfd, EPOLL_CTL_MOD, conn, &modify);
                            #ifdef DEBUG
                            printf("received complete request\n");
                            #endif
                        } else {
                            // TODO call fast cgi process
                        }
                    }

                    #ifdef DEBUG
                    printf("read %d bytes\n", bytes);
                    #endif
                }
            }
        } else if (sock_event->event == WRITE_EVENT) {
            #ifdef DEBUG
                printf("write event\n");
            #endif

            event_data = get_event_data(sock_event->sock_fd);
            // ignore if data not in WRITE_STATE
            if (event_data->state == WRITE_STATE) {    
                conn = sock_event->sock_fd;
                bytes = event_data->bytes_written;

                if ((bytes = sendto(conn, &event_data->mapped_file[bytes], event_data->bytes_to_write - bytes, 0, NULL, 0)) == -1) {
                    if (errno == EAGAIN | errno == EWOULDBLOCK);
                    else perror("send()");
                } else {
                    event_data->bytes_written += bytes;
                    if (event_data->bytes_to_write == event_data->bytes_written) {
                        close(sock_event->sock_fd);
                        munlock(event_data->mapped_file, event_data->bytes_to_write);
                        delete_event_data(conn);
                    }
                }
            }
        }

        // always delete head event after processing
        delete_head_event();
    }
}
