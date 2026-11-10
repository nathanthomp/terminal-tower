all: client server

client: src/client.c
	gcc -pthread src/client.c src/protocol.c -o client

server: src/server.c
	gcc -pthread src/server.c src/protocol.c src/game.c src/session.c -o server

clean:
	rm -f client server