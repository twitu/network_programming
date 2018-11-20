#include <stdlib.h>
#include <stdio.h>

#include "event.h"

Event temp = NULL;
Event head = NULL;
Event tail = NULL;

void add_event(int sock_fd, int event_type) {
    temp = malloc(sizeof(event));
    temp->sock_fd = sock_fd;
    temp->event = event_type;
    if (head == NULL) head = temp;
    if (tail == NULL) tail = temp;
    tail->next = temp;
    tail = temp;
}

void delete_head_event(void) {
    if (head == NULL) return;
    temp = head->next;
    free(head);
    head = temp;
}

Event get_next_event(void) {
    return head;
}

void push_back_head(void) {
    if (head == tail)  return;  // handles NULL and single event cases
    temp = head;
    head = head->next;
    tail->next = temp;
    tail = tail->next;
}

bool has_event(void) {
    return head != NULL;
}

void print_head_event(void) {
    printf("socket: %d, event: %d\n", head->sock_fd, head->event);
}

void print_tail_event(void) {
    printf("socket: %d, event: %d\n", tail->sock_fd, tail->event);
}

void print_events(void) {
    temp = head;
    while (temp != NULL) {
        printf("socket: %d, event: %d\n", temp->sock_fd, temp->event);
        temp = temp->next;
    }
}
