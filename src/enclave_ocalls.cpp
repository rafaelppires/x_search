#include <string.h>
#include <netdb.h> // gethostbyname
#include <unistd.h>
#include <iostream>

#ifdef ENABLE_SGX
#include <enclave_xsearch_u.h>
#else
#include <enclave_ocalls.h>
#endif

extern void printinfo( const char *format , ... );
//------------------------------------------------------------------------------
void ocall_print( const char* str ) {
    printinfo("\033[96m%s\033[0m", str);
}

void ocall_system( const char *cmd ) {
    int ret = system( cmd );
}

void ocall_close( int fd ) {
    close( fd );
}

//------------------------------------------------------------------------------
int ocall_connect2host( const char *hname, int port ) {
    printinfo("Connection to remote at %s:%d\n", hname, port);
    struct hostent* host = gethostbyname( hname );
    struct sockaddr_in host_addr;
    bzero((char*)&host_addr,sizeof(host_addr));
    host_addr.sin_family = AF_INET;
    host_addr.sin_port = htons( port );
    bcopy((char*)host->h_addr,(char*)&host_addr.sin_addr.s_addr,host->h_length);

    int sockfd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if( connect( sockfd,
                 (struct sockaddr*)&host_addr, sizeof(struct sockaddr)) < 0 ) {
        perror("Error in connecting to remote server");
        return -1;
    }
    printinfo("Connected to %s\n", hname);
    return sockfd;
}

//------------------------------------------------------------------------------
int ocall_forward( int from, int to ) {
    char buffer[10010];
    ssize_t n = 0;
    int ret = 0;
    do {
        bzero((char*)buffer,10000);
        n = recv( from, buffer, 10000, 0 );

        if( n > 0 ) {
            ret += send( to, buffer, n, 0 );
        }
    } while( n > 0 );

    if( n == 0 )
        printinfo("Remote closed connection accordingly.\n"); 
    else
        ret = -ret;

    return ret;
}

//------------------------------------------------------------------------------
int ocall_recv( int from, char *data, size_t len ) {
    int ret;
    if( (ret = recv( from, data, len, 0 )) < 0 ) {
        perror("Error reading from socket");
    }
    if( ret < 0 ) perror("recv error");
    return ret;
}

//------------------------------------------------------------------------------
int ocall_send( int to, const char *data, size_t len ) {
    int ret;
    if( (ret = send( to, data, len, 0 )) < 0 ) {
        perror("Error writing to socket");
    }
    if( ret < 0 ) perror("recv error");
    return ret;
}

//------------------------------------------------------------------------------


