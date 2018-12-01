out/event_driven_server.out: event_driven_server.c data.c
	mkdir -p ./out
	gcc -std=c99 -g -c data.c -o out/data.o
	gcc -std=c99 -g -c event_driven_server.c -o out/event_driven_server.o -lpthread
	gcc -std=c99 -g out/event_driven_server.o out/data.o -o event_driven_server.out -lpthread

debug: event_driven_server.c data.c
	mkdir -p ./out
	gcc -std=c99 -g -c data.c -o out/data.o
	gcc -std=c99 -g -c -D DEBUG event_driven_server.c -o out/event_driven_server.o -lpthread
	gcc -std=c99 -g -D DEBUG out/event_driven_server.o out/data.o -o event_driven_server.out -lpthread
	