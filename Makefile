all: client server

client: src/client.c
	gcc -pthread src/client.c src/game.c src/protocol.c src/queue.c -o client

server: src/server.c
	gcc -pthread src/server.c src/game.c src/protocol.c src/queue.c -o server

clean:
	rm -f client server