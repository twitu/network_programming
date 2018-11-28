#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>

// global variables
fd_set result;
int start, end;
struct sockaddr_in check_address;

void* raw_socket_checker(void* args);
void* udp_port_checker(void* args);
void* tcp_port_checker(void* args);

int main(int argc, char* argv[]) {
    if (argc != 3) perror("incorrect arguments\n");

    // start and end ports for scan
    start = atoi(argv[1]);
    end = atoi(argv[2]);

    FILE* ip_file = fopen("ip_list.txt", "r");
    if (ip_file == NULL) exit(EXIT_FAILURE);
    char* ip_string;
    size_t length = 0;
    ssize_t read;

    while ((read = getline(&ip_string, &length, ip_file)) != -1) {

        check_address.sin_family = AF_INET;
        check_address.sin_addr.s_addr = inet_addr(ip_string);

        pthread_t raw_socket_thread, udp_thread, tcp_thread;

        // check tcp ports
        int iret1 = pthread_create(&tcp_thread, NULL, tcp_port_checker, NULL);
        pthread_join(tcp_thread, NULL);

        // check udp ports
        // use an fd_set to maintain a list of ports to be checked
        // used as a cheap substitute for an array
        FD_ZERO(&result);
        int i;
        for (i = start; i <= end; i++) {
            FD_SET(i, &result);
        }

        int iret2 = pthread_create(&raw_socket_thread, NULL, raw_socket_checker, NULL);
        int iret3 = pthread_create(&udp_thread, NULL, udp_port_checker, NULL);
        pthread_join(udp_thread, NULL);
        // after sending all udp packets
        // wait for possible ICMP packets to arrive in the raw socket 
        sleep(5);

        // result set has been modified by raw socket
        // set ports are open
        for (i = start; i <= end; i++) {
            if (FD_ISSET(i, &result)) {
                printf("udp port %d is open\n", i);
            }
        }
    }

}

void* udp_port_checker(void* args) {

    // write to ports iteratively
    // send port number as packet data because it is returned in ICMP error
    int i, send_port_val;
    for (i = start; i <= end; i++) {
        int sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        check_address.sin_port = htons(i);
        // send port number as data
        send_port_val = htonl(i);
        sendto(sock_fd, &send_port_val, sizeof(int), 0, (struct sockaddr*) &check_address, sizeof(struct sockaddr));
    }
}

void* raw_socket_checker(void* args) {
    // declare structures to interpret received data
    int hlen1, hlen2, icmp_len, ret;
    struct ip *ip, *ip_hdr;
    struct icmp *icmp;
    struct udphdr *udp;

    // initialize raw socket
    int sock_fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    
    int n, error_port;
    char recvbuf[2000];
    while (1) {
        n = recvfrom(sock_fd, &recvbuf, sizeof(recvbuf), 0, NULL, 0);
        if (n > 0) {
            ip = (struct ip*) recvbuf; // start of ip header
            hlen1 = ip->ip_hl << 2; // length of ip header 

            icmp = (struct icmp*) (recvbuf + hlen1); // start of icmp header
            if ((icmp_len = n - hlen1) < 8) continue; // icmp header not arrived
            
            if (icmp->icmp_type == ICMP_UNREACH && icmp->icmp_code == ICMP_UNREACH_PORT) {
                if (icmp_len < 8 + sizeof(struct ip)) continue; // not enough data to look at ip
                
                ip_hdr = (struct ip*) (recvbuf + hlen1 + 8);
                hlen2 = ip_hdr->ip_hl << 2;
                if (icmp_len < 8 + hlen2 + 4) continue; // not enough to see ports

                udp = (struct udphdr*) (recvbuf + hlen1 + 8 + hlen2);
                error_port = ntohs(udp->dest);
                if (FD_ISSET(error_port, &result)) FD_CLR(error_port, &result);
            }
        }
    }
}

void* tcp_port_checker(void* args) {

    struct timeval tval;
    fd_set rset, wset;

    int i;
    for (i = start; i <= end; i++) {
        int sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        fcntl(sock_fd, F_SETFL, fcntl(sock_fd, F_GETFL, 0) | O_NONBLOCK);
        check_address.sin_port = htons(i);
        if (connect(sock_fd, (struct sockaddr*) &check_address, sizeof(struct sockaddr)) == -1) {
            if (errno != EINPROGRESS) {
                #ifdef DEBUG
                printf("tcp port %d not open\n", i);
                #endif
                close(sock_fd);
                continue;
            } else {
                FD_ZERO(&rset);
                FD_ZERO(&wset);
                FD_SET(sock_fd, &rset);
                FD_SET(sock_fd, &wset);
                tval.tv_sec = 1;
                tval.tv_usec = 0;

                if (select(sock_fd + 1, &rset, &wset, NULL, &tval) == 0) {
                    close(sock_fd);
                    #ifdef DEBUG
                    printf("tcp port %d not open\n", i);
                    #endif
                } else {
                    if (FD_ISSET(sock_fd, &rset) || FD_ISSET(sock_fd, &wset)) printf("tcp port %d open\n", i);
                }
            }
        } else {
            printf("tcp port %d open\n", i);
        }
        close(sock_fd);
    }
}