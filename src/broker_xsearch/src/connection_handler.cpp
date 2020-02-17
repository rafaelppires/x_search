#include <connection_handler.h>
#include <iostream>
#include <netdb.h> // gethostbyname
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <logger.h>
#include <log_entry.h>
#include <fstream>
#include <crypto.h>
#define NONENCLAVE_MATCHING
#include <sgx_utiles.h>

int ConnectionHandler::port = 0;
std::string ConnectionHandler::host;
bool ConnectionHandler::encryption_enabled = false;
extern void printinfo( const char *format , ... );
//------------------------------------------------------------------------------
int ConnectionHandler::connect2host( const std::string &hname, int p ) {
    struct hostent* host = gethostbyname( hname.c_str() );
    struct sockaddr_in host_addr;
    bzero((char*)&host_addr,sizeof(host_addr));
    host_addr.sin_family = AF_INET;
    host_addr.sin_port = htons( p );
    bcopy((char*)host->h_addr,(char*)&host_addr.sin_addr.s_addr,host->h_length);

    int sockfd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if( connect( sockfd,
                 (struct sockaddr*)&host_addr, sizeof(struct sockaddr)) < 0 ) {
        char buff[100];
        snprintf( buff, sizeof(buff),
                  "Error connecting to %s:%d", hname.c_str(),p);
        perror(buff);
        return -1;
    }
    printinfo("Connected to %s:%d\n", hname.c_str(), p );
    return sockfd;
}

extern Logger<LogEntry,std::ofstream> logger;
//------------------------------------------------------------------------------
void ConnectionHandler::main_loop() {
    char buffer[100000];
    int n, m;
    std::chrono::steady_clock::time_point t1, t2, t3, t4, t5, t6, epoch;
  
    t1 = std::chrono::steady_clock::now();
    dst_fd_ = connect2host( host, port );
    if( dst_fd_ < 0 ) {
        close(src_fd_);
        printinfo("Closed %d\n", src_fd_);
        return;
    } else printinfo("Created %d\n", dst_fd_);

    t2 = std::chrono::steady_clock::now();
    size_t req_sz = 0, rep_sz = 0;
    //do {
    n = recv( src_fd_, buffer, 10000, 0 ); // request from browser
    req_sz += n;
    if( t3 == epoch ) t3 = std::chrono::steady_clock::now();
    if( n > 0 ) {
        if( encryption_enabled ) {
            std::string cipher = Crypto::encrypt_aes( std::string(buffer,n) );
            m = send( dst_fd_, cipher.c_str(), cipher.size(), 0 );
        } else {
            m = send( dst_fd_, buffer, n, 0 ); // forwards to proxy
        }

        if( t4 == epoch ) t4 = std::chrono::steady_clock::now();
        if( n != m ) {
            printf("Problem while forwarding request\n");
            goto getout;
        }

        do { // gets back response and delivers it
            m = recv( dst_fd_, buffer, sizeof(buffer), 0 );
            rep_sz += m;
            if( m > 0 ) {
                if( encryption_enabled ) {
                    if( handle_cipher_chunk( buffer, m ) < 0 ) goto getout;
                } else
                    send( src_fd_, buffer, m, 0 );
            }
        } while( m > 0 );
    }
    //} while( n > 0 );

    if( encryption_enabled ) {
        std::string last = cryptostream_.flush();
        send( src_fd_, last.c_str(), last.size(), 0 );
    }

getout:
    t5 = std::chrono::steady_clock::now();
    close( src_fd_ );
    close( dst_fd_ );
    printinfo("Closed %d and %d\n", src_fd_, dst_fd_ );
    t6 = std::chrono::steady_clock::now();
    logger.add( LogEntry(req_sz, rep_sz, t1, t2, t3, t4, t5, t6) );
}

//------------------------------------------------------------------------------
int ConnectionHandler::handle_cipher_chunk( const char *buff, size_t n ) {
    int ret = 0;
    bool cipher = is_cipher( buff, n );
    if( !cipher ) {
        printf( "Data probably sent in clear [%s]. Crypto turned on\n",
                std::string(buff,n).c_str() );
    }
    cryptostream_.add( buff, n );
static int count = 0;
    while( !cryptostream_.empty() ) {
        std::string pl = cryptostream_.get();
        cipher = is_cipher( pl.c_str(), pl.size() );
        if( cipher ) {
            printf("Decryption did not provide clear text\n");
            return -1;
        }
        ret += send( src_fd_, pl.c_str(), pl.size(), 0 );
    }
    return ret;
}

//------------------------------------------------------------------------------
void connection_handler( int fd ) {
    ConnectionHandler ch( fd );
    ch.main_loop();
    printinfo("Thread %lX died\n", (long)pthread_self());
}

