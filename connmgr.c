/**
 * \author Yuto Yamaguchi
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include "connmgr.h"
#include "config.h"
#include "lib/tcpsock.h"
#include "sbuffer.h"

#ifndef TIMEOUT
#define TIMEOUT 5
#endif

static void (*g_logger)(const char *) = NULL;

static void *client_handler_thread(void *arg)
{
    connmgr_thread_args_t *p = (connmgr_thread_args_t *)arg;

    tcpsock_t *client = p->client;
    sbuffer_t *sbuf = p->sbuf;
    void (*logger)(const char *) = p->logger;
    free(p);

    sensor_data_t data;
    memset(&data, 0, sizeof(data));

    int bytes;
    int result;
    int first_time = 1;
    time_t last_recv_time = time(NULL);

    while (1)
    {
        bytes = sizeof(data.id);
        result = tcp_receive(client, &data.id, &bytes);
        if (result == TCP_CONNECTION_CLOSED || bytes == 0)
        {
            if (!first_time)
            {
                char msg[128];
                snprintf(msg, sizeof(msg), "Sensor node %u has closed the connection", data.id);
                logger(msg);
            }
            break;
        }
        else if (result != TCP_NO_ERROR)
        {
            break;
        }
        if (time(NULL) - last_recv_time > TIMEOUT)
        {
            char msg[128];
            snprintf(msg, sizeof(msg), "Sensor node %u timed out", data.id);
            logger(msg);
            break;
        }
        last_recv_time = time(NULL);

        bytes = sizeof(data.value);
        result = tcp_receive(client, &data.value, &bytes);
        if (result == TCP_CONNECTION_CLOSED || bytes == 0)
        {
            if (!first_time)
            {
                char msg[128];
                snprintf(msg, sizeof(msg), "Sensor node %u has closed the connection", data.id);
                logger(msg);
            }
            break;
        }
        else if (result != TCP_NO_ERROR)
        {
            break;
        }

        bytes = sizeof(data.ts);
        result = tcp_receive(client, &data.ts, &bytes);
        if (result == TCP_CONNECTION_CLOSED || bytes == 0)
        {
            if (!first_time)
            {
                char msg[128];
                snprintf(msg, sizeof(msg), "Sensor node %u has closed the connection", data.id);
                logger(msg);
            }
            break;
        }
        else if (result != TCP_NO_ERROR)
        {
            break;
        }

        if (first_time)
        {
            char msg[128];
            snprintf(msg, sizeof(msg), "Sensor node %u has opened a new connection", data.id);
            logger(msg);
            first_time = 0;
        }

        sbuffer_insert(sbuf, &data);
    }

    tcp_close(&client);
    pthread_exit(NULL);
}

void connmgr_listen(int port_number, int max_clients, sbuffer_t *sbuf,
                    void (*logger_function)(const char *))
{
    g_logger = logger_function;

    tcpsock_t *server = NULL;
    if (tcp_passive_open(&server, port_number) != TCP_NO_ERROR)
    {
        g_logger("Could not open server socket");
        return;
    }

    int client_count = 0;
    while (client_count < max_clients)
    {
        tcpsock_t *client = NULL;
        if (tcp_wait_for_connection(server, &client) != TCP_NO_ERROR)
        {
            g_logger("Error while waiting for connection");
            break;
        }

        connmgr_thread_args_t *args = malloc(sizeof(connmgr_thread_args_t));
        args->client = client;
        args->sbuf = sbuf;
        args->logger = g_logger;

        pthread_t tid;
        pthread_create(&tid, NULL, client_handler_thread, args);
        pthread_detach(tid);

        client_count++;
    }

    tcp_close(&server);
    g_logger("Connmgr: max_clients reached, listening ended.");
}
