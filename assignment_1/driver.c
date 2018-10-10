#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "nmb.h"

int main() {
    int local_fd = msgget_nmb(2034);
    char* body = "sending from port 2034 to port 2035!";
    msgsnd_nmb(local_fd, INADDR_LOOPBACK, 2035, body);
    printf("sent message\n");
    int remote_fd = msgget_nmb(2035);
    Message new_msg = msgrcv_nmb(remote_fd);
    printf("received message at remote endpoint\n");
    print_msg(new_msg);
}
