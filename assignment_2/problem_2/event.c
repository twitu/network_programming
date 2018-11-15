#include "event.h"

Event head = NULL;
Event tail = NULL;
Event buffer_head = NULL;
Event buffer_tail = NULL;
Event temp = NULL;

int event_count[4];

Event create_event(int socket_fd) {
    temp = malloc(sizeof(event));
    temp->socket_fd = socket_fd;
    temp->state = ACCEPT_EVENT;
    temp->bytes_read = 0;
    temp->bytes_to_write = 0;
    temp->bytes_written = 0;
    temp->mapped_file = NULL;
    temp->next = NULL;
    return temp;
}

void add_event(Event to_add) {
    if (to_add == NULL) return;
    if (head == NULL) head = to_add;
    if (tail == NULL) tail = to_add;
    else tail->next = to_add; tail = to_add;
}

void add_to_buffer(Event to_add) {
    if (buffer_head == NULL) buffer_head = to_add;
    if (buffer_tail == NULL) buffer_tail = to_add;
    else buffer_tail->next = to_add; buffer_tail = to_add;
}

Event get_next_event(void) {
    temp = head;
    head = head->next;
    return temp;
}

void delete(void) {
    temp = head->next;
    free(head);
    head = temp;
}

bool has_event(void) {
    return head != NULL;
}

Event get_from_buffer(int socket_fd) {
    if (buffer_head == NULL) return NULL;
    if (buffer_head->socket_fd == socket_fd) {
        temp = head;
        head = head->next;
        return temp;
    }

    temp = buffer_head;
    while (temp->next != NULL) {
        if (temp->next->socket_fd == socket_fd) {
            Event to_return = temp->next;
            temp->next = temp->next->next;
            return to_return;
        }
        temp = temp->next;
    }

    return NULL;
}
