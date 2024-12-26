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

// TIMEOUT は Makefile に -DTIMEOUT=5 のように定義がある想定
#ifndef TIMEOUT
#define TIMEOUT 5
#endif

// ログ出力関数ポインタ
static void (*g_logger)(const char *) = NULL;

static void *client_handler_thread(void *arg)
{
    // 引数として tcpsock_t* や sbuffer_t* 等をまとめた構造体を想定
    connmgr_thread_args_t *p = (connmgr_thread_args_t *)arg;

    tcpsock_t *client = p->client;
    sbuffer_t *sbuf = p->sbuf;
    void (*logger)(const char *) = p->logger;
    free(p); // 引数構造体はもう不要

    // センサIDがわかるのは最初の受信データを読んだ後なので
    // "Sensor node <ID> has opened a new connection" ログを出す場合は
    // まず最初の受信でIDを読み取る必要がある。
    // ただし、受信ループをシンプルに書くために、ここでは最初に read する形にする。

    sensor_data_t data;
    memset(&data, 0, sizeof(data));

    int bytes;
    int result;
    int first_time = 1;
    time_t last_recv_time = time(NULL);

    while (1)
    {
        // 3つのフィールド (id, value, ts) を順番に受信
        // ----------------------------------------
        // 1) sensor_id
        bytes = sizeof(data.id);
        result = tcp_receive(client, &data.id, &bytes);
        if (result == TCP_CONNECTION_CLOSED || bytes == 0)
        {
            // 接続終了
            if (!first_time)
            {
                // クライアント終了ログ
                char msg[128];
                snprintf(msg, sizeof(msg), "Sensor node %u has closed the connection", data.id);
                logger(msg);
            }
            break;
        }
        else if (result != TCP_NO_ERROR)
        {
            // なんらかのエラー
            break;
        }
        // TIMEOUTチェック
        if (time(NULL) - last_recv_time > TIMEOUT)
        {
            // タイムアウトしたので接続終了
            char msg[128];
            snprintf(msg, sizeof(msg), "Sensor node %u timed out", data.id);
            logger(msg);
            break;
        }
        last_recv_time = time(NULL);

        // 2) sensor_value
        bytes = sizeof(data.value);
        result = tcp_receive(client, &data.value, &bytes);
        if (result == TCP_CONNECTION_CLOSED || bytes == 0)
        {
            // 接続終了
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

        // 3) sensor_ts
        bytes = sizeof(data.ts);
        result = tcp_receive(client, &data.ts, &bytes);
        if (result == TCP_CONNECTION_CLOSED || bytes == 0)
        {
            // 接続終了
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

        // 初回だけ "opened connection" ログを出す
        if (first_time)
        {
            char msg[128];
            snprintf(msg, sizeof(msg), "Sensor node %u has opened a new connection", data.id);
            logger(msg);
            first_time = 0;
        }

        // sbufferへ書き込み
        sbuffer_insert(sbuf, &data);
    }

    // ソケットクローズ
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
            // エラーか何かが発生
            g_logger("Error while waiting for connection");
            break;
        }

        // クライアント受け付けたので、handlerスレッドを作る
        connmgr_thread_args_t *args = malloc(sizeof(connmgr_thread_args_t));
        args->client = client;
        args->sbuf = sbuf;
        args->logger = g_logger;

        pthread_t tid;
        pthread_create(&tid, NULL, client_handler_thread, args);
        pthread_detach(tid); // joinしない場合はdetachしておく

        client_count++;
    }

    // max_clients 回クライアントがきたらサーバ終了
    // ここでは「max_clients回 accept したらすぐに終了」とするが、
    // 課題要件に合わせて「clientがdisconnectし終わるまで待つ」等の調整が必要
    tcp_close(&server);
    g_logger("Connmgr: max_clients reached, listening ended.");
}
