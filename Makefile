all: client server

client: src/client.c
	gcc src/client.c src/shared/protocol.c -o client

server: src/server.c
	gcc src/server.c src/shared/protocol.c -o server

clean:
	rm -f client server