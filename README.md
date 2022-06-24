# Socket calculator
Socket calculator is a simple client-server application where multiple clients can connect to a server in order to 
send any math operations and get the results from the server.

# How it works
## Client
It connects to a server through socket TCP and waits to get a good response in order to start sending operations.

Before to write the operation on the opened socket, the user is asked to digit a correct operation format by keyboard.
when it is sent, client waits for a result from the server and when it gets it, an output result format is printed on user screen.

## Server
Whent it is started, it listens on any device interface for incoming connections; whenever a client connects to server, 
it checks whether the maximum number of clients is reached or not, if it's not then server sends a response to client saying
that it accepts the operations sent.

If there are too many clients connected, server will put the connection in a wait status until a previous connected client 
is disconnected.


