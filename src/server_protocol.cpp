#include <stdio.h>
#include <server_protocol.h>
#include <string>
#include <sstream>
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <pthread.h>
#include <poll.h>
#include <unistd.h>
#include <log_entry.h>
#include <logger.h>
#ifdef ENABLE_SGX
#include <enclave_xsearch_u.h>
extern sgx_enclave_id_t global_eid;
#endif

extern void printinfo( const char *format , ... );
/**
  * Global static init: to be called in the beginning of times
  */
void ServerProtocol::init() {
    //TLSConnection::init();
}
//------------------------------------------------------------------------------
ServerProtocol::ServerProtocol( int s ) : clt_socket_(s), querymanager_(s),
                                          state_(INITIAL) {
}

/**
  * Main loop. Plaintext at beginning, TLS later
  */
void ServerProtocol::main_loop() {
    char buffer[10010];
    //tlsserver_.tlsserver_setup();
    ssize_t sz;
    while( (sz = recv( clt_socket_, buffer, sizeof(buffer), 0 )) >= 0 ) {
        if( !client_in( buffer, sz ) ) break;
        if( sz == 0 ) {
            printinfo("Client smoothly finished the connection\n");
            goto getout;
        }

        if( state_ == REMOTE_CLOSED_CONN ) {
            printinfo("Remote smoothly finished the connection\n");
            goto getout;
        }
    }

getout:
    //::close( clt_socket_ ); // already finished in main loop
    printinfo("%lX Main loop finished\n", (long)pthread_self() );
}

//------------------------------------------------------------------------------
bool ServerProtocol::client_in( char *buff, ssize_t len ) {
    if( len == 0 ) return true;
    if( state_ == INITIAL ) return init_state(buff, len);
    else if( state_ == SERVER_TLSCONNECTED) return connected_state(buff, len);
    std::cout << "client_in: Unexpected state: " << state_ << "\n";
    return false;
}

//------------------------------------------------------------------------------
bool ServerProtocol::server_in( char *buff, ssize_t len ) {
    if( state_ != FWDREQUEST ) {
        std::cout << "client_int: Unexpected state: " << state_ << "\n";
        return false;
    }
    return false;
/*
    ssize_t sent = 0;
    if( (sent = tlsserver_.reply(buff, len)) == len ) {
        std::cout << "Server said: (" << buff << ")\n";
        return true;
    } else {
        printf("Mismatch on fwd data %ld!=%ld\n",len,sent);
        return false;
    }
*/
}

extern Logger<LogEntry,std::ofstream> logger;
//------------------------------------------------------------------------------
bool ServerProtocol::init_state( char *buff, ssize_t len ) {
#if 0
    std::stringstream ss;
    std::string  command, address, data(buff,len);
    ss.str( data );
    ss >> command >> address;

    //void *thr = (void*)pthread_self();
    if( command == "CONNECT" ) {
        printf("Client wants an end-to-end HTTPS connection\n");
/*
        if( tlsclient_.tlsconnect( address ) ) {
            state_ = SERVER_TLSCONNECTED;
            printf( "%lX Connection with '%s' established\n",
                                                    (long)thr, address.c_str());
            const char *repl =
            "HTTP/1.1 200 Connection Established\r\nConnection: close\r\n\r\n";
            send( clt_socket_, repl, strlen(repl), 0 );
            return false; // because henceforth we do TLS (breaks first loop)
        }
*/
        return true;
    } else if( command == "GET" ) {
#endif
        //printinfo("Client made an unencrypted HTTP request\n");
        int ret;
        std::chrono::steady_clock::time_point t1, t2, z;
        t1 = std::chrono::steady_clock::now();
#ifdef ENABLE_SGX
        ecall_handlerequest( global_eid, &ret, clt_socket_, buff, len );
#else
        ret = querymanager_.handle_request( std::string(buff,len) );
#endif
        if( ret > 0 ) {
            printinfo("Page succesfully forwarded: %d bytes\n", ret);
        } else {
            printf("Something has happened sz=%d\n", ret);
        }
        state_ = REMOTE_CLOSED_CONN;
        t2 = std::chrono::steady_clock::now();
        //logger.add( LogEntry(len,ret,t1,z,z,z,z,t2) );
        return true;
#if 0
    } else {
        std::cout << "Unrecogniz. command: '" << command << "'\n";
        return false;
    }
#endif
}

//------------------------------------------------------------------------------
bool ServerProtocol::connected_state( char *buff, ssize_t len ) {
    std::cout << "Fwd req... len=" << len << " Content: \n["
              << buff << "]" << std::endl;
    //tlsclient_.forward(buff,len);
    state_ = FWDREQUEST;
    return true;
}

//------------------------------------------------------------------------------
