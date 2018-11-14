#include <stdlib.h>
#include <stdbool.h>

#define HTTP_REQ_LIMIT 255

#define READ_REQUEST 0
#define PROCESS_REQUEST 1
#define WRITE_RESULT 2

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

void create_event(int socket_fd);
Event get_event(void);
void push_back(void);
bool check_event(int socket_fd);
void delete_tail(void);
