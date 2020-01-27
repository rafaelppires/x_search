#ifndef _TLS_CONNECTION_H_
#define _TLS_CONNECTION_H_
#include <string>
#include <tls.h>

class TLSConnection {
public:
    TLSConnection();
    TLSConnection(int);
    ~TLSConnection();
    bool tlsconnect( const std::string & );
    void tlsserver_setup();
    ssize_t recv( char *buff, ssize_t n );
    ssize_t get( char *buff, ssize_t n );
    ssize_t forward( char *buff, ssize_t n );
    ssize_t reply( char *buff, ssize_t n );
    int socketfd() { return srv_socket_; }
    void closeconn();

    static void init();
private:
    void tlsclient_setup();
    struct tls *tls_;
    int srv_socket_;
    struct tls *tls2;

    static struct tls_config *config_server;
};

#endif
