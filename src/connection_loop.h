#ifndef _CONNECTION_LOOP_H_
#define _CONNECTION_LOOP_H_

#if 0
#if defined(__cplusplus)
extern "C" {
#endif

    void *connection_handler(void *);

#if defined(__cplusplus)
}
#endif
#else
void connection_handler( int sock );
#endif

#endif

