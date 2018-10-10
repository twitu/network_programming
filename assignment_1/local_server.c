#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/msg.h>
#include <sys/prctl.h>
#include <sys/poll.h>
#include <arpa/inet.h>

#include "nmb.h"

#define QUEUE 10
#define MSG_KEY 214298

typedef struct que_msg {
    long msgtype;
    message data;
} que_msg;
typedef que_msg* Que_msg;

typedef struct thread_arg {
    int conn_fd;
    struct sockaddr_in peer_addr;
} thread_arg;


// global variables
int msg_que_id;
int tcp_acpt_conn_fd;

Que_msg create_que_message(Message data);
void die(char* s);
void* tcp_connection_thread(void* arg);

void main() {

    // creating message queue
    if (msg_que_id = msgget(MSG_KEY, IPC_CREAT | 0644) == -1) die("mssget()");

    // parent process is the TCP server, child process is the UDP listener
    switch (fork()) {
        case -1: 
            die("fork()");
        case 0: // child process
        
            // kill process if parent dies
            prctl(PR_SET_PDEATHSIG, SIGKILL);

            // preparing UDP socket related variables
            struct sockaddr_in *sock_recv;
            int sock_len = sizeof(struct sockaddr_in);
            sock_recv = (struct sockaddr_in*) calloc(1, sizeof(struct sockaddr_in));

            // receiving socket
            int udp_conn_recv_fd;
            if ((udp_conn_recv_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) die("socket()");
            int udp_true_val = 1;
            if (setsockopt(udp_conn_recv_fd, SOL_SOCKET, SO_REUSEADDR, &udp_true_val, sizeof(int)) == -1) die("setsockopt()");

            // receiving address
            sock_recv->sin_family = AF_INET;
            sock_recv->sin_port = htons(UDP_PORT);
            sock_recv->sin_addr.s_addr = htonl(INADDR_ANY);

            // UDP server receving messages from the network
            // buffer can contain only one message at once
            if (bind(udp_conn_recv_fd, (struct sockaddr*) sock_recv, sock_len) == -1) die("udp no bind");

            // receive UDP datagrams
            // interpret them as queue messages
            // send them to the queue
            message info;
            while (1) {
                if (recvfrom(udp_conn_recv_fd, &info, sizeof(info), 0, (struct sockaddr*)sock_recv, &sock_len) == -1) die("recvfrom()");
                to_host_byte_order(&info);
                Que_msg temp = create_que_message(&info);
                msgsnd(msg_que_id, temp, sizeof(que_msg), IPC_NOWAIT);
                #ifdef DEBUG
                print_msg(&info);
                #endif
            }
            break;
        default: // parent process

            // prepare TCP connection related socket and socket addr
            if ((tcp_acpt_conn_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) die("socket()");
            int tcp_true_val = 1;
            if (setsockopt(tcp_acpt_conn_fd, SOL_SOCKET, SO_REUSEADDR, &tcp_true_val, sizeof(int)) == -1) die("setsockopt()");
            struct sockaddr_in myaddr, peer_addr;
            myaddr.sin_family = AF_INET;
            myaddr.sin_port = htons(TCP_PORT);
            myaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            int new_conn_fd, peer_addr_len = sizeof(struct sockaddr_in);

            // bind socket to my address and listen for connection with maximun outstanding connections defined in QUEUE
            if (bind(tcp_acpt_conn_fd, (struct sockaddr*)&myaddr, sizeof(myaddr)) == -1) die("tcp no bind");
            if (listen(tcp_acpt_conn_fd, QUEUE) == -1) die("listen()");
            #ifdef DEBUG
            printf("listening for TCP connections\n");
            #endif

            // wait for UDP socket to bind
            sleep(0.5);

            thread_arg args;
            // passive port listens for new connections and creates thread for each new connection
            while(1)
            {
                if ((new_conn_fd = accept(tcp_acpt_conn_fd, (struct sockaddr*)&peer_addr, &peer_addr_len)) == -1) die("accept()");
                #ifdef DEBUG
                printf("accepted connection from port %d", htons(peer_addr.sin_port));
                #endif
                args.conn_fd = new_conn_fd;
                args.peer_addr = peer_addr;
                pthread_t thread_id;
                pthread_create(&thread_id, NULL, tcp_connection_thread, (void*) &args);
            }
    }
}

void* tcp_connection_thread(void* arg) {
    // process argument
    thread_arg* data = arg;
    int conn_fd = data->conn_fd;
    struct sockaddr_in peer_addr = data->peer_addr;

    // initialize variables to use poll
    struct pollfd checkfd;
    checkfd.fd = conn_fd;
    checkfd.events = POLLIN;

    // get port number of remote end of connection
    // this will be used for retrieving message from message queue
    in_port_t peer_sockfd = peer_addr.sin_port;
    long msgtype = ntohs(peer_sockfd);

    // create udp sockaddr and socket for sending data
    int udp_conn_dest_fd;
    if ((udp_conn_dest_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) die("udp sender socket()");
    struct sockaddr_in sock_dest;
    sock_dest.sin_family = AF_INET;
    sock_dest.sin_port = htons(UDP_PORT);
    int sock_len = sizeof(sock_dest);

    // create buffers for receiving TCP data and messages from message queue
    size_t total = sizeof(message);
    void* buffer[sizeof(message)];
    que_msg new_que_msg;

    // non blocking listen on message queue and poll TCP socket connection
    while (1) {

        // non blocking read on message queue
        // if receive message send to local process via TCP connection
        if (msgrcv(msg_que_id, &new_que_msg, sizeof(que_msg), msgtype, IPC_NOWAIT) > 0) {
            if (sendto(conn_fd, &new_que_msg.data, sizeof(message), 0, NULL, 0) == -1) die("tcp connection sendto()");
        } else {
            if (errno != ENOMSG) die("msgrcv()");
        }

        // poll tcp connection to check if readable
        int activity = poll(&checkfd, 1, 0);
        switch (activity) {
            case -1:
                die("poll()");
            case 0:
                close(checkfd.fd);
                break;
            default:
                // if data is available read and send to destination address through UDP
                if (checkfd.revents == POLLIN) {
                    size_t count = 0, bytes = 0;
                    void* pointer = buffer;
                    while (count < total) {
                        bytes = read(conn_fd, pointer, total - count);
                        if (bytes == -1) close(conn_fd);
                        else if (bytes > 0) {
                            pointer += bytes;
                            count += bytes;
                        }
                    }
                    to_net_byte_order(((Message)&buffer));
                    if (sendto(udp_conn_dest_fd, &buffer, sizeof(buffer), 0, (struct sockaddr*) &sock_dest, sock_len) == -1) die("udp destination sendto()");
                }
        }
    }
}

void die(char* s) {
    printf("error number %d\n", errno);
    printf("%s\n", s);
    exit(1);
}

void print_msg(Message info) {
    printf("ip: %d, port: %d\nmessage:\n %s\n", info->dest_addr, info->dest_port, info->body);
}

Message to_net_byte_order(Message info) {
    info->dest_addr = htonl(info->dest_addr);
    info->dest_port = htons(info->dest_port);
    return info;
}

Message to_host_byte_order(Message info) {
    info->dest_addr = ntohl(info->dest_addr);
    info->dest_port = ntohs(info->dest_port);
    return info;
}

// msgtype is same as destination port
// destination port is used to identify the local process to send the message to
Que_msg create_que_message(Message data) {
    Que_msg new_data = (Que_msg) malloc(sizeof(que_msg));
    new_data->msgtype = data->dest_port;
    new_data->data = *data;
    return new_data;
}
