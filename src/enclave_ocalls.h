#ifndef _ENCLAVE_OCALLS_H_
#define _ENCLAVE_OCALLS_H_

#ifdef __cplusplus
extern "C" {
#endif
int ocall_connect2host( const char *hname, int port );
int ocall_forward( int from, int to );
int ocall_send( int to, const char *data, size_t len );
int ocall_recv( int from, char *data, size_t len );
void ocall_print( const char* str );
void ocall_system( const char *cmd );
void ocall_close( int fd );
#ifdef __cplusplus
}
#endif

#endif

