#ifndef _DELAY_LOGGER_H_
#define _DELAY_LOGGER_H_
#include <vector>
#include <mutex>

template< typename T, typename StreamType >
class Logger {
public:
    Logger() : outstream_(0) {}

    void flush() {
        if( !outstream_ ) return;
        mutex_.lock();
        for( auto const &t : logstore_ ) {
            *outstream_ << t << '\n';
        }
        mutex_.unlock();
        outstream_->close();
    }

    void add( const T &t ) {
        mutex_.lock();
        logstore_.push_back( t );
        mutex_.unlock();
    }

    void setoutput( StreamType *s ) {
        outstream_ = s;
    }

    const T& first() {
        return logstore_.front();
    } 

    bool empty() {
        return logstore_.empty();
    }

private:
    std::vector<T> logstore_;
    StreamType *outstream_;
    std::mutex mutex_;
};

#endif

