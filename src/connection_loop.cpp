/**
  *  Receives messages
  */

#include <connection_loop.h>
#include <unistd.h> // open, close, read, write
#include <stdio.h>
#include <sys/socket.h> // send, recv
#include <server_protocol.h>
#include <string.h>

extern void printinfo( const char *format , ... );

#if 0
void *connection_handler(void *socket_desc) {
    // Get the socket descriptor
    int sock = *(int*)socket_desc;
    ServerProtocol sp( sock );
    sp.main_loop();

    close(sock);
    printinfo("%lX Thread finished\n", pthread_self() );
    return 0;
}
#else
void connection_handler( int sock ) {
#if 0
    char buffer[50000];
    const char *reply = "HTTP/1.1 200 OK\n\n";
    recv( sock, buffer, sizeof(buffer), 0 );
    send( sock, reply, strlen(reply), 0 );
    close( sock );
#else
    ServerProtocol *sp = new ServerProtocol( sock );
    sp->main_loop();
    delete sp;
    close(sock);
    printinfo("%lX Thread finished\n", pthread_self() );
#endif
}
#endif

