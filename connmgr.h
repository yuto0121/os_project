#ifndef _CONNMGR_H_
#define _CONNMGR_H_

#include "lib/tcpsock.h"
#include "sbuffer.h"
#include "config.h"

typedef struct
{
    tcpsock_t *client;
    sbuffer_t *sbuf;
    void (*logger)(const char *);
} connmgr_thread_args_t;

/**
 * \brief
 * \param port_number
 * \param max_clients
 * \param sbuf
 * \param logger_func
 */
void connmgr_listen(int port_number, int max_clients, sbuffer_t *sbuf,
                    void (*logger_func)(const char *));

#endif
