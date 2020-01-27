#ifndef _CRYPTO_STREAM_H_
#define _CRYPTO_STREAM_H_

#include <string>
#include <queue>

static const size_t crypto_chunk = 1000;

class CryptoStream {
public:
    CryptoStream( bool encrypt = true ) : enc_(encrypt) {}

    void add( const char *b, size_t len );
    bool empty() { return output_.empty(); }
    std::string get();
    std::string flush();

private:
    void apply( char *dst, const char *src, size_t len );

    bool enc_;
    std::queue< std::string > output_;
    std::string incomplete_;
};

#endif

