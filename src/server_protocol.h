#ifndef _SERVER_PROTOCOL_H_
#define _SERVER_PROTOCOL_H_
//#include <tls_connection.h>
#include <query_manager.h>

class ServerProtocol {
public:
    ServerProtocol( int s );
    bool client_in( char *buff, ssize_t len );
    bool server_in( char *buff, ssize_t len );
    void main_loop();
    //int socket() { return clt_socket_; }

    static void init();

private:
    enum State {
        INITIAL,
        SERVER_TLSCONNECTED,
        CLIENTTLSDONE,
        FWDREQUEST,
        REMOTE_CLOSED_CONN
    };

    bool init_state( char *buff, ssize_t len );
    bool connected_state( char *buff, ssize_t len );
    int forward_http_req( const std::string &, char *, ssize_t );

    int clt_socket_;
    QueryManager querymanager_;
//    TLSConnection tlsclient_, // TLS connection with remote server
//                  tlsserver_; // TLS connection with browser
    State state_;
};

#endif
