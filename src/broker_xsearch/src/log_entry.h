#ifndef _LOG_ENTRY_H_
#define _LOG_ENTRY_H_
#include <ctime>
#include <chrono>

struct LogEntry {
    typedef std::chrono::steady_clock::time_point Timetype;
    LogEntry() {}
    LogEntry( size_t l1, size_t l2, Timetype l3, Timetype l4, Timetype l5,
                                    Timetype l6, Timetype l7, Timetype l8 ) :
       req_sz(l1), rep_sz(l2), t1(l3), t2(l4), t3(l5), t4(l6), t5(l7), t6(l8) {}

    template< typename T >
    friend T& operator<<( T &out, const LogEntry &e ) {
        out << e.req_sz << "\t" << e.rep_sz << "\t"
            << std::chrono::duration <double, std::milli> (e.t6 - e.t1).count()
            << "\t" 
            << std::chrono::duration <double, std::milli> (e.t6 - t0).count();
        return out;
    }

    static Timetype t0;
    static void set_reference( const LogEntry &l ) {
        t0 = l.t1;
    }

    size_t req_sz, rep_sz;
    Timetype t1, t2, t3, t4, t5, t6;
};

#endif

