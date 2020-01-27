#include <query_manager.h>
#include <string.h> // bzero bcopy
#include <unistd.h> // close
#include <stdio.h> 
#include <cstdlib>
#include <crypto_stream.h>

QueryManager::DictionaryType QueryManager::dictionary;
QueryManager::Setup QueryManager::setup(0,"",0,false,false,0);
#ifdef ENABLE_SGX
#include <enclave_xsearch_t.h>
#include <sgx_trts.h>
QueryManager::Mutex QueryManager::dict_mutex = SGX_THREAD_MUTEX_INITIALIZER;

void printf(const char *fmt, ...) {
    char buf[BUFSIZ] = {'\0'};
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    ocall_print(buf);
}

int rand() {
    int ret;
    sgx_status_t st = sgx_read_rand((unsigned char*)&ret, sizeof(ret));
    if (st != SGX_SUCCESS) {
        printf("mbedtls_sgx_drbg fails with %d\n", st);
        return -1;
    }
    return ret; 
}
#define printinfo printf
#else
#include <enclave_ocalls.h>
QueryManager::Mutex QueryManager::dict_mutex;
#define NONENCLAVE_MATCHING
extern void printinfo( const char *format , ... );
#endif
#include <sgx_utiles.h>

//------------------------------------------------------------------------------
int QueryManager::handle_request( const std::string &dat ){
    int ret = -1, port;
    size_t qpos, qlen;
    std::string data, hostname, query, qprfix;
    bool cipher = is_cipher(dat.c_str(),dat.size()), real_done;
    std::vector<std::string> fakes;
    if( setup.enc ) {
        if( !cipher ) {
            printf("Data probably sent in clear. Crypto turned on\n");
            //goto getout;
        }
        char *dst = (char*)malloc(dat.size());
        decrypt(dat.c_str(),dst,dat.size());
        data = std::string(dst,dat.size());
        free(dst);
    } else {
        if( cipher ) {
            printf("Data is encrypted. Crypto turned off\n");
            goto getout;
        }
        data = dat;
    }
    //printf("<%s>\n", data.c_str());

    qpos = data.find("//")+2;
    qlen = data.find('/', qpos) - qpos;
    hostname = data.substr(qpos, qlen);
    port = 80;

    qprfix = "search?q=";
    qpos = data.find(qprfix, qpos+qlen) + qprfix.size();
    qlen = data.find(' ', qpos) - qpos;
    query = data.substr(qpos,qlen);

    if( setup.fakes > 0 && hostname == "www.bing.com" ) {
        add2dictionary( query );
        if( !setup.nwait )
            fakes = get_fakes();
        printinfo("\nWe're about to make %ld fakes now\n", fakes.size());
    }

    check_override( hostname, port );
    int sockfd;
    if( setup.nwait ) {
        sockfd = 0;
    } else {
        #ifdef ENABLE_SGX
        if( ocall_connect2host( &sockfd, hostname.c_str(), port ) 
                                                              != SGX_SUCCESS ) {
            printf("Unable to make ocall_connect2host\n");
            return ret;
        }
        #else
        sockfd = ocall_connect2host( hostname.c_str(), port ) ;
        #endif
        if( sockfd < 0 ) {
            printf("ocall_connect2host returned an invalid fd\n");
            return ret;
        }
    }

    if( setup.fakes > 0 && !setup.nwait ) {
        real_done = (ret = exec_fakes( sockfd, fakes, data )) > 0;
        if( ret < 0 ) goto getout;
    } else if( setup.obfuscate > 0 ) {
        std::string obf = get_obfuscated( query );
        if( !obf.empty() ) data.replace( qpos, qlen, obf );
    }

    if( !real_done )
        ret = real_query( sockfd, clt_socket_, data );

getout:
    if( !setup.nwait )  {
        #ifdef ENABLE_SGX
        ocall_close( sockfd );
        #else
        close( sockfd );
        #endif
    }
    return ret;
}

//------------------------------------------------------------------------------
std::string QueryManager::get_obfuscated( const std::string &q ) {
    std::string ret;
    std::vector<std::string> qry;
    bool real_done = false;
    size_t n = 1 + rand() % setup.obfuscate;
    if( dictionary.size() < n ) goto getout;
    while( n-- ) {
        if( !real_done && rand() % (1+n) == 0 ) { // avoid div by 0
            qry.push_back(q);
            real_done = true;
        } else {
            auto vi = dictionary.begin();
            std::advance( vi, rand() % dictionary.size() );
            qry.push_back( *vi );
        }
    }

     if( !real_done ) qry.push_back( q );

    for( int i = 0; i < qry.size(); ++i ) 
        ret += (i==0 ? "" : "+OR+") + qry[i];

getout:
    dictionary.push_back( q );
    return ret;
}

//------------------------------------------------------------------------------
int QueryManager::exec_fakes( int sock, const std::vector<std::string> &fakes,
                              const std::string &data ) {
    int ret = 0;
    for( auto const &req : fakes ) {
        if( !ret && rand() % setup.fakes == 0 ) {
            ret = real_query( sock, clt_socket_, data );
            if( ret < 0 ) goto getout;
        }
        std::string command("wget -q -O /dev/null --page-requisites "
                            "www.bing.com/search?q=");
        command += req;
        printf("FAKE %s\n", command.c_str());
        #ifdef ENABLE_SGX
        ocall_system( command.c_str() );
        #else
        int x = system( command.c_str() );
        #endif
    }
getout:
    return ret;
}

//------------------------------------------------------------------------------
int QueryManager::real_query( int srv_fd, int clt_fd, 
                              const std::string & data ) {
    int ret;
    std::string realqr = data.substr(0,data.find("\r\n"));
    #ifdef ENABLE_SGX
    printf("REAL [%s]\n", realqr.c_str());
    #else
    printinfo("%lX REAL [%s]\n", pthread_self(), realqr.c_str());
    #endif
    if( (ret = make_request( srv_fd, data )) < 0 ) 
        return ret;
    ret = forward_response( srv_fd, clt_fd );
    return ret;
}

//------------------------------------------------------------------------------
int QueryManager::make_request( int srv_fd, const std::string &data ) {
    int ret;
    if( setup.nwait ) return data.size();
#ifdef ENABLE_SGX
    if( ocall_send( &ret, srv_fd, data.c_str(), data.size() ) != SGX_SUCCESS )
        printf("Error during ocall_mkrequest\n");
#else
     ret = ocall_send( srv_fd, data.c_str(), data.size() );
#endif
    return ret;
}

#if 0
#include <crypto++/hex.h>
//------------------------------------------------------------------------------
float countchars( const std::string &s ) {
    unsigned count = 0;
    for( std::string::const_iterator it = s.begin(); it != s.end(); ++it )
        if( isgraph(*it) || isspace(*it) ) count++;
    return float(count)/s.size();
}
//------------------------------------------------------------------------------
std::string printable( const std::string &s ) {
    if( countchars(s) < 0.99 ) {
        std::string ret;
        StringSource ssrc( s, true /*pump all*/,
                              new CryptoPP::HexEncoder( new StringSink(ret) ) );
        return ret;
    }
    return s;
}
#endif

//------------------------------------------------------------------------------
int QueryManager::forward_response(int srv_fd, int clt_fd ) {
    int ret = 0, n, sent;
    bool once = false;
    CryptoStream cryptostream;
    do {
        if( !setup.nwait ) {
#ifdef ENABLE_SGX
            if( ocall_recv( &n, srv_fd, inbuffer, sizeof(inbuffer) ) 
                                                                != SGX_SUCCESS )
                printf("Error during ocall_recv\n");
#else
            n = ocall_recv( srv_fd, inbuffer, sizeof(inbuffer) );
#endif
        } else if( !once ) {
            snprintf( inbuffer, sizeof(inbuffer), " %X\n", rand() );
            std::string rply = std::string("HTTP/1.1 200 OK\n\nDummy reply")
                                                                     + inbuffer;
            memcpy( inbuffer, rply.c_str(), rply.size() );
            n = rply.size();
            once = true;
        } else
            n = 0;

        if( n > 0 ) {
            if( setup.enc ) {
                cryptostream.add( inbuffer, n );
                while( !cryptostream.empty() ) {
                    std::string chk = cryptostream.get();
                    ret += send_chunk( clt_fd, chk.c_str(), chk.size() );
                }
            } else {
                ret += send_chunk( clt_fd, inbuffer, n );
            }
        }
    } while( n > 0 );

    if( n != 0 ) ret = -ret;
    else if( setup.enc ) {
        std::string last = cryptostream.flush(); 
        ret += send_chunk( clt_fd, last.c_str(), last.size() );
    }

    return ret;
}

//------------------------------------------------------------------------------
int QueryManager::send_chunk( int fd, const char *buff, size_t n ) {
    static int count = 0;
    int sent = -1;
#ifdef ENABLE_SGX
    if( ocall_send( &sent, fd, buff, n ) != SGX_SUCCESS )
        printf("Error during ocall_send\n");
#else
    sent = ocall_send( fd, buff, n );
#endif
    return sent;
}

//------------------------------------------------------------------------------
void QueryManager::mlock( QueryManager::Mutex &mutex ) {
#ifdef ENABLE_SGX
    sgx_thread_mutex_lock(&mutex);
#else
    mutex.lock();
#endif
}

void QueryManager::munlock( QueryManager::Mutex &mutex ) {
#ifdef ENABLE_SGX
    sgx_thread_mutex_unlock(&mutex);
#else
    mutex.unlock();
#endif
}

//------------------------------------------------------------------------------
void QueryManager::add2dictionary( const std::string &query ) {
    mlock( dict_mutex );
    dictionary.push_back(query);
    munlock( dict_mutex );
}

//------------------------------------------------------------------------------
std::vector<std::string> QueryManager::get_fakes() {
    std::vector<std::string> ret;
    size_t fqueries = 1 + rand() % setup.fakes;

    mlock( dict_mutex );
    size_t n = dictionary.size();
    if( n < 3 ) goto getout;
    while( fqueries-- > 0 ) {
        bool two = rand() % 2; // either 1 or 2 combined
        int idx1 = rand() % (n-1), // the last is the real one
            idx2 = rand() % (n-1);
        auto vi = dictionary.begin(), vj = vi;
        std::advance( vi, idx1 );
        std::advance( vj, idx2 );
        ret.push_back( *vi + (two?("+"+(*vj)):"") );
    }
getout:
    munlock( dict_mutex );
    return ret;
}

//------------------------------------------------------------------------------
void QueryManager::check_override( std::string &hostname, int &port ) {
    if( setup.ohost.size() == 0 ) return;
    hostname = setup.ohost;
    port = setup.oport;
}

//==============================================================================
// ONLY SGX HEREAFTER
//==============================================================================
#ifdef ENABLE_SGX
void ecall_sgxproxy_init( size_t n, const char *overrideaddr, 
                          size_t overrideport, char e, char nw, size_t o ) {
    QueryManager::Setup s( n, overrideaddr, overrideport, (bool)e, (bool)nw,
                           o );
    QueryManager::config( s );
}

int ecall_handlerequest( int fd, const char *b, size_t len ) {
    QueryManager qm(fd);
    return qm.handle_request( std::string(b,len) );
}
#endif

