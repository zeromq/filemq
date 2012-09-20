/*  =========================================================================
    fmq_server.h

    Generated header for fmq_server protocol server
    =========================================================================
*/

#ifndef __FMQ_SERVER_H_INCLUDED__
#define __FMQ_SERVER_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

//  Opaque class structure
typedef struct _fmq_server_t fmq_server_t;

//  Create a new fmq_server
fmq_server_t *
    fmq_server_new (void);

//  Destroy the fmq_server
void
    fmq_server_destroy (fmq_server_t **self_p);

//  Bind to endpoint
void
    fmq_server_bind (fmq_server_t *self, const char *endpoint);
    
//  Connect to endpoint
void
    fmq_server_connect (fmq_server_t *self, const char *endpoint);

//  Load server configuration data
void
    fmq_server_configure (fmq_server_t *self, const char *config_file);

//  Self test of this class
int
    fmq_server_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif
