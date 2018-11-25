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

#include "data.h"

#define LISTEN_PORT 8000
#define MAX_BUFFER_SIZE 2048
#define MAX_CLIENTS 200
#define RESPONSE_HEADER_LIMIT 200

// global variables
int epfd;
Data cgi_data;
sem_t mutex;

// function stubs
void* event_generator(void* args);
void* fast_cgi(void* args);

int main() {
    sem_init(&mutex, 0, 0);
    pthread_t thread1, thread2;
    int iret1 = pthread_create(&thread1, NULL, event_generator, NULL);
    int iret2 = pthread_create(&thread2, NULL, fast_cgi, NULL);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    exit(0);
}

void* event_generator(void* args) {

    int bytes, conn;
    char request[HTTP_REQ_LIMIT];
    char resource[RESOURCE_LIMIT];
    char response_header[RESPONSE_HEADER_LIMIT];

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
    fcntl(listen_fd, F_SETFL, fcntl(listen_fd, F_GETFL, 0) | O_NONBLOCK);
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
        event_count = epoll_wait(epfd, events, MAX_CLIENTS + 1, 1000);
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
                            if (bytes == 0) {
                                // situation indicates socket has been closed from client side
                                close(conn);
                                delete_event_data(conn);
                                continue;
                            }
                            event_data->bytes_read += bytes;
                            if (strstr(event_data->request, "\r\n\r\n")) {
                                // request received
                                event_data->state = PROCESS_STATE;
                                sscanf(event_data->request, "GET /%s ", resource);
                                if (strstr(resource, ".cgi") == NULL) {
                                    // call helper thread to load file  
                                    sprintf(event_data->resource, "%s", resource);

                                    // map file to memory
                                    int file_fd = open(resource, O_RDONLY);
                                    if (file_fd == -1) {
                                        // error accessing file
                                        close(file_fd);
                                        char* error_reponse = "HTTP/1.1 404 Not Found\r\n\r\n";
                                        send(conn, error_reponse, strlen(error_reponse), 0);
                                        close(conn);
                                        delete_event_data(conn);
                                    }
                                    struct stat file_stat;
                                    fstat(file_fd, &file_stat);
                                    event_data->mapped_file = (char*) mmap(NULL, file_stat.st_size, PROT_READ, MAP_PRIVATE, file_fd, 0);
                                    mlock(event_data->mapped_file, file_stat.st_size);
                                    event_data->bytes_to_write = file_stat.st_size;
                                    close(file_fd);

                                    // send header to client
                                    sprintf(response_header, "HTTP/1.1 200 OK\r\nContent-Length : %ld\r\n\r\n", file_stat.st_size);
                                    send(conn, response_header, strlen(response_header), 0);

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
                                    // call fast cgi process
                                    sprintf(event_data->resource, "%s", resource);
                                    cgi_data = event_data;
                                    sem_post(&mutex);
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

                        if ((bytes = send(conn, &event_data->mapped_file[bytes], event_data->bytes_to_write - bytes, 0)) == -1) {
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

void* fast_cgi(void* args) {
    while (1) {
        sem_wait(&mutex);
        #ifdef DEBUG
        printf("process fast cgi\n");
        #endif
        
        char query[4];
        sscanf(cgi_data->resource, "factorial.cgi=%s", query);
        int limit = atoi(query);
        int a = 1;
        int b = 1;
        int c;
        cgi_data->mapped_file = (char*) mmap(NULL, sizeof(int) * limit, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        int* head = (int*) cgi_data->mapped_file;
        head[0] = 1;
        head[1] = 1;
        for (int i = 2; i < limit; i++) {
            c = a + b;
            head[i] = c;
            a = b;
            b = c;
        }
        cgi_data->bytes_to_write = limit * sizeof(int);
        // send header to client
        char response_header[100];
        sprintf(response_header, "HTTP/1.1 200 OK\r\nContent-Length : %d\r\n\r\n", cgi_data->bytes_to_write);
        send(cgi_data->socket_fd, response_header, strlen(response_header), 0);
        // modify to write state and change socket events for OUT
        cgi_data->state = WRITE_STATE;
        struct epoll_event modify;
        modify.data.fd = cgi_data->socket_fd;
        modify.events = EPOLLOUT | EPOLLET;
        epoll_ctl(epfd, EPOLL_CTL_MOD, cgi_data->socket_fd, &modify);
        
    }
}
