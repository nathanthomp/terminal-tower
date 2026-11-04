all: client server

client: src/client.c
	gcc src/client.c src/protocol.c -o client

server: src/server.c
	gcc src/server.c src/protocol.c src/game.c -o server

clean:
	rm -f client server