#include <crypto_stream.h>
#ifndef ENABLE_SGX
#define NONENCLAVE_MATCHING
#else
extern void printf(const char *fmt, ...);
#endif
#include <sgx_utiles.h>

//------------------------------------------------------------------------------
std::string CryptoStream::flush() {
    if( !output_.empty() ) {
        printf( "CryptoStream::flush should only be called after all chunks "
                " are consumed\n");
        return "";
    }

    char buff[ crypto_chunk ];
    size_t n = incomplete_.size();
    apply( buff, incomplete_.c_str(), n );
    incomplete_.clear();
    return get();
}

//------------------------------------------------------------------------------
std::string CryptoStream::get() { 
    if( output_.empty() ) return "";
    std::string ret = output_.front();
    output_.pop();
    return ret;
 }

//------------------------------------------------------------------------------
void CryptoStream::apply( char *dst, const char *src, size_t len ) {
    if( enc_ ) {
       encrypt( src, dst, len ); 
    } else {
       decrypt( src, dst, len );
    }
    output_.push( std::string(dst,len) );
}

//------------------------------------------------------------------------------
void CryptoStream::add( const char *b, size_t len ) {
    char buff[ crypto_chunk ];

    if( incomplete_.size() + len < crypto_chunk ) {
        incomplete_ += std::string(b,len);
        return;
    } else if( !incomplete_.empty() ) {
        size_t n = crypto_chunk - incomplete_.size();
        incomplete_ += std::string( b, n );
        apply( buff, incomplete_.c_str(), crypto_chunk );
        incomplete_.clear();
        b += n;
        len -= n;
    }

    while( len > crypto_chunk ) {
        apply( buff, b, crypto_chunk );
        b += crypto_chunk;
        len -= crypto_chunk;
    }

    if( len )
        incomplete_ = std::string( b, len );
}
//------------------------------------------------------------------------------

