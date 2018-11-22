#define _GNU_SOURCE  // Note: this reduces the portability of the code, as getline is only available in Linux
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define HTTP_HEADER 255

int main() {
    FILE* web_request_file = fopen("webpages.txt", "r");
    if (web_request_file == NULL) exit(EXIT_FAILURE);
    
    char* web_request = NULL;
    char *domain, *resource;
    char response[HTTP_HEADER];
    size_t len = 0;
    ssize_t read;
    int response_code, sock_fd, bytes;

    // create address to send http request
    struct sockaddr_in endpoint;
    endpoint.sin_family = AF_INET;
    endpoint.sin_port = htons(80);

    // process each line one by one
    // split domain and resource
    // check if domain exists, connect to port 80
    // send http get request
    // check response code and print
    while ((read = getline(&web_request, &len, web_request_file)) != -1) {

        domain = strtok(web_request, "/");
        resource = strtok(NULL, "\n");

        struct hostent* domain_host = gethostbyname(domain);
        if (domain_host == NULL) { printf("%s domain does not exist\n", domain); continue;}

        // set address
        memcpy(&endpoint.sin_addr, domain_host->h_addr_list[0], 4);

        // connect to port 80 else print error and continue
        sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (connect(sock_fd, (struct sockaddr*) &endpoint, sizeof(struct sockaddr)) == -1) {
            printf("could not connect to port 80 in %s domain\n", domain);
            continue;
        }

        // create new request and send
        char request[100] = {0};
        sprintf(request, "GET /%s HTTP/1.1\r\nHost: %s\r\nContent-Type: text/plain\r\n\r\n", resource, domain);
        send(sock_fd, request, 100, 0);
        #ifdef DEBUG
        printf("sent http request to %s for %s\n", domain, resource);
        #endif

        // use select to timeout after 2 seconds on unresponsive web server
        struct timeval timeout;
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;

        fd_set rset;
        FD_ZERO(&rset);
        FD_SET(sock_fd, &rset);

        select(sock_fd + 1, &rset, NULL, NULL, &timeout);

        if (FD_ISSET(sock_fd, &rset)) {
            // receive response from server
            bytes = recv(sock_fd, response, HTTP_HEADER, 0);
            response[bytes] = '\0';
            close(sock_fd);

            // check response code and print
            sscanf(response, "HTTP/1.1 %d", &response_code);
            if (response_code == 200) {
                printf("%s/%s: Page exists\n", domain, resource);
                
            } else {
                printf("Received error code %d\n", response_code);
            }
            #ifdef DEBUG
            printf("response:\n%s\n", response);
            #endif
        } else {
            // unresponsive web server
            close(sock_fd);
            printf("%s did not respond for request %s\n", domain, resource);
        }
    }

    fclose(web_request_file);
    if (web_request)
        free(web_request);
    exit(EXIT_SUCCESS);
}