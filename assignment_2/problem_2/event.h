#include <stdlib.h>
#include <stdbool.h>

#define ACCEPT_EVENT 0
#define READ_EVENT 1
#define PROCESS_EVENT 2
#define WRITE_EVENT 3

typedef struct event {
    int sock_fd;
    int event;
    struct event* next;
} event;

typedef event* Event;

void add_event(int sock_fd, int event_type);
void delete_head_event(void);
Event get_next_event(void);
void push_back_head(void);
bool has_event(void);
void print_head_event(void);
void print_tail_event(void);
void print_events(void);