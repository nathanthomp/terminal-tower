# Intermediate Gameplay Execution Log

This document logs the execution of gameplay events during the intermediate stages of the game.

## Requesting a Connection

The client starts by asking the server to connect player Nathan. The request is sent to the server on port 8080.

![Client requesting a connection](images/client-connecting.png)

## Connection Successful

The server receives the request, creates a session for Nathan, and sends back a session ID. The client then confirms that it is connected to the server.

![Server accepting the connection](images/server-connected.png)

![Client connected to the server](images/client-connected.png)

## Connection Unsuccessful

When the server is not available, the client waits for a response and then reports that it could not connect.

![Client unable to connect](images/client-unable-to-connect.png)

## Multiple Connections

When another player connects, the server creates a second session and tells both players that the game is ready to start.

![Two clients connected to the server](images/server-two-clients-connected.png)

If another player tries to join after the server is full, the server rejects the request and reports that the game is full.

![Server full](images/server-full.png)