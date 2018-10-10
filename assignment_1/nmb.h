#include <arpa/inet.h>

#define UDP_PORT 1112
#define TCP_PORT 1111
#define BODY_LIMIT 506

typedef struct message {
    in_port_t dest_port;
    in_addr_t dest_addr;
    char body[BODY_LIMIT];
} message;

typedef message* Message;

// creates message from given data
// if body larger than max size give error
// if body less then max size pad with zeroes
Message create_message(in_addr_t dest_ip, in_port_t dest_port, char* body);

// translates host to network byte order
// translation is done for dest_addr, dest_port
// returns translated message
Message to_net_byte_order(Message info);

// translates network to host byte order
// translation is done for dest_addr, dest_port
// returns translated message
Message to_host_byte_order(Message info);

// creates TCP connection for the given port with TCP server
// return socket file descriptor to TCP connection
// if port is below 1025 return -1
int msgget_nmb(in_port_t port);

// creates and sends message across given socket
// return 0 if successful else return -1
int msgsnd_nmb(int sockfd, in_addr_t dest_ip, in_port_t dest_port, char* body);

// receives message from network message bus
// It is a blocking call
// return pointer to message if successful else return NULL
Message msgrcv_nmb(int sockfd);

// print message in readable format
// to be used for debugging
void print_msg(Message info);