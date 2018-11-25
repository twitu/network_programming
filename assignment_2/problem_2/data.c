#include <stdlib.h>
#include <stdbool.h>

#include "data.h"

Data sock_data;
Data* data_store;
int count;
int max_clients;

void create_data_store(int max) {
    count = 0;
    max_clients = max;
    data_store = calloc(max_clients, sizeof(Data));
}

int find_empty_slot(void) {
    for (int i = 0; i < max_clients; i++) {
        if (data_store[i] == NULL) return i;
    }
    return -1;
}

int find_data_slot(int socket_fd) {
    for (int i = 0; i < max_clients; i++) {
        if (data_store[i] != NULL && data_store[i]->socket_fd == socket_fd) return i;
    }
    return -1;
}

int new_data_slot(int socket_fd) {
    int index = find_empty_slot();
    if (index == -1) return -1;
    sock_data = malloc(sizeof(data));
    sock_data->index = index;
    sock_data->socket_fd = socket_fd;
    sock_data->state = READ_STATE;
    sock_data->bytes_read = 0;
    sock_data->bytes_to_write = 0;
    sock_data->bytes_written = 0;
    sock_data->mapped_file = NULL;
    data_store[index] = sock_data;
    count++;
    return index;
}

void delete_event_data(int socket_fd) {
    int index = find_data_slot(socket_fd);
    if (index == -1) return;
    free(data_store[index]);
    data_store[index] = NULL;
    count--;
}

Data get_event_data(int socket_fd) {
    int index = find_data_slot(socket_fd);
    if (index == -1) return NULL;
    return data_store[index];
}

bool has_space(void) {
    return count < max_clients;
}
