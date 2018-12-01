# Assignment 2 - part 1
## Objective  
Single process event driven model for web server. The web server accepts connections, processes http requests and responds with contents if the resource is found

## Design
`data.c`  
* creates the data store that stores the state of each connection being processed
* array based implementation implies fixed capacity and linear search

`event_driven_server.c`
* using epoll to check for I/O events on sockets
* using a single listening socket to accept connections
* each connection has a state maintained in the data store and a non-blocking socket in the epoll listener
* `READ_STATE` the process keeps reading data until it finds end of request
* file is mapped to memory using mmap, `404 Not Found` is sent back if file does not exist
* `WRITE_STATE` the process writes data from memory mapped file after which the socket is closed and the state data deleted

## Results
processing 800 http req/s without failing when data store capacity is 200 connections


## Scope of improvement
* implement a multi-threaded event driven model. it will use message queues to queue events. each thread will handle only a particular state in the http request cycle
* use a separate thread to call mlock
* use a hashmap to store and find values in the data store
* create the server in modern languages like rust or go
* fix to fast cgi thread to modify epoll events for the socket it is processing