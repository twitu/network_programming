#include <stdlib.h>
#include <stdbool.h>

#define HTTP_REQ_LIMIT 255
#define RESOURCE_LIMIT 100

#define READ_STATE 1
#define PROCESS_STATE 2
#define WRITE_STATE 3

// extra byte for \0 character
typedef struct data {
    int index;
    int socket_fd;
    char state;
    char request[HTTP_REQ_LIMIT + 1];
    char resource[RESOURCE_LIMIT + 1];
    int bytes_read;
    char* mapped_file;
    int bytes_to_write;
    int bytes_written;
} data;

typedef data* Data;

void create_data_store(int max_clients);
int find_empty_slot(void);
int new_data_slot(int socket_fd);
void delete_event_data(int socket_fd);
Data get_event_data(int socket_fd);
bool has_space(void);
