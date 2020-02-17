#include <thread>
#include <csignal>
#include <argp.h>
#include <arpa/inet.h>
#include <string.h>
#include <iostream>
#include <connection_handler.h>
#include <logger.h>
#include <log_entry.h>
#include <fstream>
#include <sstream>

Logger<LogEntry, std::ofstream> logger;
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
const char *argp_program_version = "Proxy broker v0.9";
const char *argp_program_bug_address = "<rafael.pires@unine.ch>";

/* Program documentation. */
static char doc[] = "Proxy broker" ;
static struct argp_option options[] = {
    { "lport",    'l', "port", 0, "Defines listening port. Default 6666" },
    { "encrypt",  'e', 0, 0, "Encrypts the channel" },
    { "verbose",  'v', 0, 0, "Prints stuff" },
    { "proxy",    'p', "poxyhost:port", 0, "Defines the proxy where requests will be forwarded to" },
    { "logfile",  'f', "logfile", 0, "Defines the logging file. Default 'broker_delays.dat'" },
    { 0 }
};

//------------------------------------------------------------------------------
struct Arguments {
    Arguments() : rhost("localhost"), logfname("broker_delays.dat"), 
                  lport(6666), rport(9000), encrypt(false), verbose(false) {}
    std::string rhost, logfname;
    size_t lport, rport;
    bool encrypt, verbose;
};

//------------------------------------------------------------------------------
static error_t parse_opt (int key, char *arg, struct argp_state *state) {
    Arguments *args = (Arguments*)state->input;
    switch(key) {
    case 'l':
        args->lport = std::stoi(arg);
        break;
    case 'f':
        args->logfname = arg;
        break;
    case 'e':
        args->encrypt = true;
        break;
    case 'v':
        args->verbose = true;
        break;
    case 'p':
    {
        size_t pos;
        args->rhost = std::string(arg);
        args->rport = 
                std::stoi( args->rhost.substr((pos=args->rhost.find(':'))+1) );
        args->rhost = args->rhost.substr( 0, pos );
        break;
    }
    default:
        return ARGP_ERR_UNKNOWN;
    };
    return 0;
}

static struct argp argp = { options, parse_opt, 0, doc };
//------------------------------------------------------------------------------
#include <stdarg.h>
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
    argp_parse(&argp, argc, argv, 0, 0, &args);
    global_verbose = args.verbose;
    
    std::signal( SIGINT, ctrlc_handler );
    ConnectionHandler::init( args.rhost, args.rport, args.encrypt );

    std::ofstream logfile( args.logfname.c_str() );
    logger.setoutput( &logfile );

    struct sockaddr_in server, client;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    const char *localh = "*"; //"127.0.0.1";
    bzero(&server, sizeof(server));
    server.sin_addr.s_addr = htonl(INADDR_ANY); //inet_addr(localh);
    server.sin_port = htons( args.lport );
    server.sin_family = AF_INET;

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, 4);
    if( bind(sock, (struct sockaddr *) &server, sizeof(server)) < 0 ) {
        printf("Error binding to %s:%ld\n", localh, args.lport);
        exit(1);
    }
    printinfo("Listening %s:%ld\n", localh, args.lport);
    listen(sock, 10);

    int sock_fd;
    socklen_t client_size = sizeof(client);

    while((sock_fd =
                 accept(sock, (struct sockaddr *) &client, &client_size)) > 0) {
        std::thread t( connection_handler, sock_fd );
        std::stringstream ss; ss << std::hex << t.get_id();
        printinfo("%s Thread created socket %d\n", ss.str().c_str(), sock_fd );
        t.detach();
    }


    perror("Accept failed");
    return 2;
}

