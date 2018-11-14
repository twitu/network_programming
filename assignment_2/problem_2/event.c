#include "event.h"

Event head = NULL;
Event tail = NULL;
Event temp = NULL;

void create_event(int socket_fd) {
    temp = malloc(sizeof(event));
    temp->socket_fd = socket_fd;
    temp->state = READ_REQUEST;
    temp->bytes_read = 0;
    temp->bytes_to_write = 0;
    temp->bytes_written = 0;
    temp->mapped_file = NULL;
    temp->next = NULL;
    if (head == NULL) head = temp;
    if (tail == NULL) tail = temp;
    else tail->next = temp; tail = temp;
}

Event get_event(void) {
    push_back();
    return tail;
}

void push_back(void) {
    if (head == tail) return;
    temp = head;
    head = head->next;
    temp->next = NULL;
    tail->next = temp;
    tail = temp;
}

void delete_tail(void) {
    temp = tail->next;
    free(tail);
    tail = temp;
}

bool has_event() {
    if (tail == NULL) return false;
    else return false;
}

bool check_event(int socket_fd) {
    Event temp = head;
    while (temp != NULL) {
        if (temp->socket_fd == socket_fd) return true;
        temp = temp->next;
    }

    return false;
}
