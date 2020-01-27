#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <poll.h>

#include <tls.h>

int main(int argc, char **argv) {

    struct tls *tls = NULL;
    struct tls_config *config = NULL;
    char *msg = "HELLO TLS SERVER!\n";
    ssize_t outlen = 0;
    char bufs[1000], bufc[1000];
    struct sockaddr_in server;
    struct pollfd pfd[2];
    int sock;

    tls_init();

    tls = tls_client();

    config = tls_config_new();

    tls_config_insecure_noverifycert(config);
    tls_config_insecure_noverifyname(config);

    tls_configure(tls, config);

    sock = socket(AF_INET, SOCK_STREAM, 0);

    server.sin_port = htons(9000);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;

    if(connect(sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
        printf("erro socket\n");
        exit(1);
    }

    if(tls_connect_socket(tls, sock, "localhost") < 0) {
        printf("tls_connect error\n");
        printf("%s\n", tls_error(tls));
        exit(1);
    }


    tls_write(tls, msg, strlen(msg));

    pfd[0].fd = 0;
    pfd[0].events = POLLIN;
    pfd[1].fd = sock;
    pfd[1].events = POLLIN;

    while(bufc[0] != ':' && bufc[1] != 'q') {
        bzero(bufs, 1000);
        bzero(bufc, 1000);

        poll(pfd, 2, -1);

        if(pfd[0].revents & POLLIN) {
            int q = read(0, bufc, 1000);
            tls_write(tls, bufc, q);
        }

        if(pfd[1].revents & POLLIN) {
            if((outlen = tls_read(tls, bufs, 1000)) <= 0) break;
            printf("Mensagem (%lu): %s\n", outlen, bufs);
        }

    }

    tls_close(tls);
    tls_free(tls);
    tls_config_free(config);

    return 0;
}

