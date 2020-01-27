#include <connection_loop.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <server_protocol.h>
#include <argp.h>
#include <csignal>    // capture SIGINT
#include <log_entry.h>
#include <logger.h>
#include <fstream>
#include <iostream>
#include <stdarg.h>
#include <thread>
#include <sstream>
#ifdef ENABLE_SGX
#include <enclave_xsearch_u.h>
#include <sgx_initenclave.h>
#include <libgen.h>
#define DEFAULTLOGF "proxy_sgx_delays.dat"
#define PROGDESCRIPT "SGX HTTP proxy"
sgx_enclave_id_t global_eid = 0;
#else
#define PROGDESCRIPT "XSearch HTTP proxy"
#define DEFAULTLOGF "proxy_ntv_delays.dat"
#endif

Logger<LogEntry,std::ofstream> logger;
//------------------------------------------------------------------------------
LogEntry::Timetype LogEntry::t0;
void ctrlc_handler( int s ) {
    printf("\033[0m\n(%d) Normal exit!\n", s);
    if( !logger.empty() ) {
        LogEntry::set_reference( logger.first() );
        logger.flush();
    }
    exit(0);
}

//------------------------------------------------------------------------------
const char *argp_program_version = "SGX-capable HTTP(S) proxy + fake queries to bing v0.9";
const char *argp_program_bug_address = "<rafael.pires@unine.ch>";

/* Program documentation. */
static char doc[] =
  PROGDESCRIPT " - Forwards HTTP or HTTPS requests to servers. Optionally, "
#ifdef ENABLE_SGX
  "performs SGX authentication, "
#endif
  "encrypts the channel to client and performs "
  "fake queries to Bing aiming at preserving privacy" ;
static struct argp_option options[] = {
    { "fake",    'f', "max", 0, "Turns on fake queries and defines their "
                                "maximum amount per real query. "
                                "Mutually exclusive with obfuscation." },
    { "obfuscation",'b', "max", 0, "Turns on obfuscation and defines their "
                                "maximum amount per real query. "
                                "Mutually exclusive with fake queries." },
    { "port",    'p', "port", 0, "Defines listening port. Default 9000" },
    { "override",'o', "host:port", 0, "Override the destination of all queries (for evaluation purposes)" },
    { "nowait", 'n', 0, 0, "Immediately reply every query with empty results (for evaluation purposes)." },
    { "encrypt", 'e', 0, 0, "Encrypts the channel with broker." },
    { "verbose", 'v', 0, 0, "Prints information about execution." },
    { "log",     'l', "filename", 0, "Defines log filename. Default '" 
                                                     DEFAULTLOGF "'" },
    { 0 }
};

//------------------------------------------------------------------------------
struct Arguments {
    Arguments() : fake(0), port(9000), obfuscate(0), logfname(DEFAULTLOGF), 
                  encrypt(false), verbose(false), nowait(false)
                  {}
    size_t fake, port, obfuscate;
    std::string logfname, dstoverride;
    bool encrypt, verbose, nowait;
};

//------------------------------------------------------------------------------
/* Parse a single option. */
static error_t parse_opt (int key, char *arg, struct argp_state *state) {
    Arguments *args = (Arguments*)state->input;
    switch(key) {
    case 'p':
        args->port = std::stoi(arg);
        break;
    case 'f':
        args->fake = std::stoi(arg);
        break;
    case 'l':
        args->logfname = arg;
        break;
    case 'e':
        args->encrypt = true;
        break;
    case 'v':
        args->verbose = true;
        break;
    case 'o':
        args->dstoverride = arg;
        break;
    case 'n':
        args->nowait = true;
        break;
    case 'b':
        args->obfuscate = std::stoi(arg);
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    };
    return 0;
}

static struct argp argp = { options, parse_opt, 0, doc };
size_t global_max_fakequeries = 0;
//------------------------------------------------------------------------------
bool global_verbose;
void printinfo( const char *format , ... ){
    if( !global_verbose) return;
   va_list arglist;
   va_start( arglist, format );
   vprintf( format, arglist );
   va_end( arglist );
}

//------------------------------------------------------------------------------
int main(int argc, char **argv) {
    Arguments args;
    srand(time(0));

    argp_parse(&argp, argc, argv, 0, 0, &args);
    global_verbose = args.verbose;
    std::ofstream logfile( args.logfname.c_str() );
    logger.setoutput( &logfile ); 

    std::string overhost;
    size_t overport = 0;
    if ( args.dstoverride.size() > 0 ) {
        size_t pos;
        overhost = args.dstoverride.substr( 0, pos=args.dstoverride.find(':') );
        overport = std::stoi( args.dstoverride.substr(pos+1) );
        printf("Overriding queries to %s:%lu\n", overhost.c_str(), overport );
    }

    if( args.fake > 0 && args.obfuscate > 0 ) {
        std::cerr << "Fake queries and obfuscation are mutually exclusive.\n"
                     "Choose only one between them. \n"
                     "--help or -? for more information.\n";
        return -1;
    }

#ifdef ENABLE_SGX
    /* Changing dir to where the executable is.*/
    char absolutePath [200];
    char *ptr = NULL;

    ptr = realpath(dirname(argv[0]),absolutePath);

    if( chdir(absolutePath) != 0)
            abort();

    /* Initialize the enclave */
    if(initialize_enclave( global_eid,
                            "xsearch.signed.so","enclave.xsearch.token") < 0) {
        return -2;
    }
    ecall_sgxproxy_init( global_eid, args.fake, overhost.c_str(), overport, 
                         args.encrypt, args.nowait, args.obfuscate );
#else
    QueryManager::Setup s( args.fake, overhost, overport, args.encrypt,
                           args.nowait, args.obfuscate );
    QueryManager::config( s );
#endif

    std::signal( SIGINT, ctrlc_handler );
    ServerProtocol::init();

    struct sockaddr_in server, client;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    const char *localh = "*"; //"127.0.0.1";
    bzero(&server, sizeof(server));
    server.sin_addr.s_addr =  htonl(INADDR_ANY); //inet_addr(localh);
    server.sin_port = htons( args.port );
    server.sin_family = AF_INET;

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, 4);
    if( bind(sock, (struct sockaddr *) &server, sizeof(server)) < 0 ) {
        printf("Error binding to %s:%ld\n", localh, args.port);
        exit(1);
    }
    printinfo("Listening %s:%ld\n", localh, args.port);
    listen(sock, 10);

    socklen_t client_size = sizeof(client);
    int sock_fd;
    //pthread_t thread_id;
    while((sock_fd =
                 accept(sock, (struct sockaddr *) &client, &client_size)) > 0) {
#if 0
        if( pthread_create( &thread_id, NULL,
                            connection_handler, (void*) &sock_fd) < 0) {
            perror("Not able to create thread");
            return 1;
        } else {
            printinfo("%lX Thread created\n", (unsigned long)thread_id);
        }
#else
        std::thread t( connection_handler, sock_fd );
        std::stringstream ss; ss << std::hex << t.get_id();
        printinfo("%s Thread created socket %d\n", ss.str().c_str(), sock_fd );
        t.detach();
#endif

    }

    perror("Accept failed");
    return 2;
}

