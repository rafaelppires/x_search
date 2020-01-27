#include <tls_connection.h>
#include <iostream>
#include <netdb.h> // gethostbyname
#include <string>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>

/**
  * When socket identifier is not given, we assume TLS client; server otherwise
  */
//------------------------------------------------------------------------------
TLSConnection::TLSConnection() : srv_socket_(-1), tls_(NULL), tls2(NULL) { // TLS client
}

//------------------------------------------------------------------------------
TLSConnection::TLSConnection(int fd) : srv_socket_(fd), tls_(NULL), tls2(NULL) { // TLS server
}


//------------------------------------------------------------------------------
TLSConnection::~TLSConnection() {
    closeconn();
}

//------------------------------------------------------------------------------
void TLSConnection::closeconn() {
    if( tls_ ) {
        tls_close( tls_ );
        tls_free( tls_ );
        tls_ = 0;
    }

    if( tls2 ) {
        tls_close( tls2 );
        tls_free( tls2 );
        tls2 = 0;
    }

    if( srv_socket_ >= 0 ) {
        close( srv_socket_ );
        srv_socket_ = -1;
    }
}
//------------------------------------------------------------------------------

/**
  * Global static init: to be called in the beginning of times
  * Call tls_init and sets the configuration for the server TLS endpoint
  */
struct tls_config *TLSConnection::config_server;
void TLSConnection::init() {
    static bool oonce = false;
    if( oonce ) return; else oonce = true;

    if(tls_init() < 0) {
        printf("tls_init error\n");
        exit(1);
    }

    config_server = tls_config_new();
    if(config_server == NULL) {
        printf("tls_config_new error\n");
        exit(1);
    }

    unsigned int protocols = 0;
    if(tls_config_parse_protocols(&protocols, "secure") < 0) {
        printf("tls_config_parse_protocols error\n");
        exit(1);
    }

    tls_config_set_protocols(config_server, protocols);

    const char *ciphers = "ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384";
    if(tls_config_set_ciphers(config_server, ciphers) < 0) {
        printf("tls_config_set_ciphers error\n");
        exit(1);
    }

    if(tls_config_set_key_file(config_server, "private.pem") < 0) {
        printf("tls_config_set_key_file error\n");
        exit(1);
    }

    if(tls_config_set_cert_file(config_server, "server.crt") < 0) {
        printf("tls_config_set_cert_file error\n");
        exit(1);
    }
}

/**
  * Allocates both config and tls structs and fills them
  * Free the config
  */
void TLSConnection::tlsclient_setup() {
    struct tls_config *config = NULL;
    tls_ = tls_client();
    if(tls_ == NULL) {
        printf("tls_client error\n");
        exit(1);
    }

    config = tls_config_new();
    tls_config_insecure_noverifycert(config);
    tls_config_insecure_noverifyname(config);
    tls_configure(tls_, config);
    //tls_config_free(config);
}

/**
  * Allocates the tls struct and fill it with config filled in the static init
  */
void TLSConnection::tlsserver_setup() {
    tls_ = tls_server();
    if(tls_ == NULL) {
        printf("tls_server error\n");
        exit(1);
    }

    if(tls_configure(tls_, config_server) < 0) {
        printf("tls_configure error: %s\n", tls_error(tls_));
        exit(1);
    }
}

/**
  * Establishes a new TLS client socket connection with server
  */
bool TLSConnection::tlsconnect( const std::string &addr ) {
    std::string::size_type n = addr.find( ":" );
    if( n == std::string::npos ) return false;
    std::string host = addr.substr(0,n),
                port = addr.substr(n+1);

    tlsclient_setup();

    // Connection to remote server
    struct hostent* hostinfo = gethostbyname( host.c_str() );
    struct sockaddr_in host_addr;
    bzero((char*)&host_addr,sizeof(host_addr));
    host_addr.sin_port=htons( std::stoi(port) );
    host_addr.sin_family=AF_INET;
    bcopy( (char*)hostinfo->h_addr,
           (char*)&host_addr.sin_addr.s_addr, hostinfo->h_length );
    srv_socket_ = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if( connect( srv_socket_,
                 (struct sockaddr*)&host_addr, sizeof(struct sockaddr))  < 0 ) {
        perror("Error connecting to remote server");
        return false;
    } else {
        if(tls_connect_socket(tls_, srv_socket_, host.c_str()) < 0) {
            printf("tls_connect error\n");
            printf("%s\n", tls_error(tls_));
            exit(1);
        }
        std::cout << "TLS connection established with server\n";
    }

    return true;
}

/**
  * Receives data from browser (server TLS socket)
  */
ssize_t TLSConnection::recv( char *buff, ssize_t n ) {
    if( tls2 == NULL ) {
        if(tls_accept_socket(tls_, &tls2, srv_socket_) < 0) {
            printf("tls_accept_socket error\n");
            exit(1);
        }
        std::cout << "Looks like we have a TLS connection with client\n";
    }

    int nn = tls_read( tls2, buff, n );
    if( nn < 0 ) std::cout << "oops n=" << nn << " err:" << tls_error(tls2) << "\n";

    return nn;
}

/**
  * Receives data from server (client TLS socket)
  */
ssize_t TLSConnection::get( char *buff, ssize_t n ) {
    int nn = tls_read( tls_, buff, n );
    if( nn < 0 ) std::cout << "oops n=" << nn << " err:" << tls_error(tls_) << "\n";
    return nn;
}

/**
  * Forwards browser request to server (client TLS socket)
  */
ssize_t TLSConnection::forward( char *buff, ssize_t n ) {
    return tls_write(tls_,buff,n);
}

/**
  * Reply to browser (server TLS socket)
  */
ssize_t TLSConnection::reply( char *buff, ssize_t n ) {
    return tls_write(tls2,buff,n);
}
//------------------------------------------------------------------------------
