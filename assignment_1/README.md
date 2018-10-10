# Assignment 1  
## Objective  
Creating a network message bus service so that processes can communicate with each other. This should work for inter and intra host communication.

## Design
`local_server.c`  
* the local  accepts TCP connections on 1111 and listens for UDP packets on 1112
* on receiving a UDP datagram, the server extracts the message and puts in a message queue with the message type same as destination port number specified in the message
* the server creates new threads for each connection it accepts
* in each thread two tasks are performed
     1. perform non blocking receive on message queue with message typr same as port of peer connection. If it receives a message send the message across the accepted TCP connection to the destined local process
     2. use poll to perform non-blocking receive on accepted TCP connection. It interprets the data and sends to the message to destination IP address as specified in the message

`nmb.c`
* handles communication between `local_server.c` and local processes
* takes port number as input, binds new sockets to the port and makes connection with the local TCP server
* takes message data, creates message struct forwards the data to the `local_server.c` through user specfied socket via TCP
* performs blocking read on user provided socket for any message destined for port bound to the given socket

`driver.c`
* a simple test client to show this service in action

## Scope of improvement
* it is possible for a program to specify any port number and access messages not destined for it
* each thread created by `local_server.c` performs a busy wait, alternately checking the TCP connection and message queue. this might not be scalable for large number of processes
* create a better `driver.c` to show the service in action over the network