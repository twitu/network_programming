compile:
	mkdir -p ./out
	gcc -g -pthread -D DEBUG local_server.c -o ./out/local_server
	gcc -c nmb.c -o ./out/nmb.o
	gcc -c driver.c -o ./out/driver.o
	gcc ./out/driver.o ./out/nmb.o -o ./out/main