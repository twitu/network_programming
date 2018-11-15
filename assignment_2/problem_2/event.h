#include <stdlib.h>
#include <stdbool.h>

#define HTTP_REQ_LIMIT 255

#define ACCEPT_EVENT 0
#define READ_EVENT 1
#define PROCESS_EVENT 2
#define WRITE_EVENT 3

typedef struct event {
    int socket_fd;
    char state;
    char request[HTTP_REQ_LIMIT];
    int bytes_read;
    char* mapped_file;
    int bytes_to_write;
    int bytes_written;
    struct event* next;
} event;

typedef event* Event;

Event create_event(int socket_fd);
void add_event(Event to_add);
void add_to_buffer(Event to_add);
Event get_next_event(void);
void delete(void);
bool has_event(void);
Event get_from_buffer(int socket_fd);
