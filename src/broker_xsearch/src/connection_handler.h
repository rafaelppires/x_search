#ifndef _CONNECTION_HANDLER_H_
#define _CONNECTION_HANDLER_H_
#include <string>
#include <mutex>
#include <crypto_stream.h>

class ConnectionHandler {
public:
    ConnectionHandler( int from ) : src_fd_(from), dst_fd_(-1),
                                    cryptostream_(false) {}
    void main_loop();

    static void init( const std::string &h, int p, bool enc )  {
        host = h;
        port = p;
        encryption_enabled = enc;
    }

private:
    int connect2host( const std::string &hname, int port );
    int handle_cipher_chunk( const char *, size_t );

    int src_fd_, dst_fd_;
    CryptoStream cryptostream_;

    static int port;
    static std::string host;
    static bool encryption_enabled;
};

void connection_handler( int fd );

#endif

