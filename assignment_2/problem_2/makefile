out/event_driven_server.out: event_driven_server.c event.c
	mkdir -p ./out
	gcc -g -c event.c -o out/event.o
	gcc -g -c event_driven_server.c -o out/event_driven_server.o
	gcc -g out/event_driven_server.o out/event.o -o out/event_driven_server.out
	