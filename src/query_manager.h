#ifndef _QUERY_MANAGER_H_DEFINED_
#define _QUERY_MANAGER_H_DEFINED_

#include <string>
#include <vector>
#include <list>

#ifdef ENABLE_SGX
#include <sgx_thread.h>
#else
#include <mutex>
#endif

class QueryManager {
public:
    struct Setup {
        Setup(size_t n, const std::string &s, size_t p, bool e, bool nw,
              size_t o ) : ohost(s), fakes(n), obfuscate(o), oport(p), enc(e), 
                           nwait(nw) {}
        std::string ohost;
        size_t fakes, obfuscate, oport;
        bool enc, nwait;
    };

    QueryManager( int cfd ) : clt_socket_(cfd) {}
    int handle_request( const std::string & );

    static void check_override( std::string &hostname, int &port );
    static void config( const Setup &s ) { setup = s; }
#ifdef ENABLE_SGX
    typedef sgx_thread_mutex_t Mutex;
#else
    typedef std::mutex Mutex;
#endif
private:
    void add2dictionary( const std::string & );
    std::vector<std::string> get_fakes();
    int forward_response(int srv_fd, int clt_fd );
    int exec_fakes( int sck, const std::vector<std::string> &fakes,
                    const std::string &data );
    int make_request( int srv_fd, const std::string &data );
    int real_query( int srv_fd, int clt_fd, const std::string & data );
    std::string get_obfuscated( const std::string & );
    int send_chunk( int fd, const char *buff, size_t n );

    typedef std::list<std::string> DictionaryType;
    static DictionaryType dictionary;
    static Setup setup;

    void mlock( Mutex &mutex );
    void munlock( Mutex &mutex );
    static Mutex dict_mutex;

    // Object state
    int clt_socket_;
    char inbuffer[20000];
};

#endif

