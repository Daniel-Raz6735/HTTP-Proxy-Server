Name: Daniel Raz 

to activate program: open linux terminal, navigate to ex3 executeable file location using "cd" command (confirm it using ls command) and type 
valgrind ./proxyServer <port> <pool-size> <max-num-of-request> <filter>.


list of submitted files:
threadpool.c : The pool is implemented by a queue. When the server gets a connection (getting back from
ac cept()), it should put the connection in the queue. When there will be available thread (can be
immediately), it will handle this connection (read request and write response).

proxyServer.c :The proxy server gets an HTTP request from the client, and
performs some predefined checks on it. If the request is
found legal, it forwards the request to the appropriate web server, and sends the response back to the client.
Otherwise, it send s a response to the client without sending anything to the server. O nly IPv4 connections should be
supported.


private function in proxyServer.c:

int clientHendler(void *arg) - this function its the function that the thread will handle ,recive request from the client and move it forward to check_input.

char *errorResponse(int code) - this function get a code of error and make the relvent error respnse.

int create_socket(int) - this function for create the main socket for the proxy server.

char *check_input(char *str, int client_sd)- this function get the request from the client and check if its contain GET method , path and HTTP/1.0 or HTTP/1.1 at the first line.
and Host header and move it forward to talk_to_server when it back from talk_to_server we check if the return give a String "ok" that say the client get the response from the server and if not he get error resposnse.

char *talk_to_server(char *host, int port, char *request, int client_sd) - this function create socket for the server and send him the request and recive is response if evrything fine return  String "ok".

int get_the_filter(char *) - this function make list of forbidden hosts from the filter file.
