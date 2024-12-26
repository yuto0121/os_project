#ifndef _CONNMGR_H_
#define _CONNMGR_H_

#include "lib/tcpsock.h"
#include "sbuffer.h"
#include "config.h"

// クライアントハンドリングに渡すためのスレッド引数構造体
typedef struct
{
    tcpsock_t *client;
    sbuffer_t *sbuf;
    void (*logger)(const char *);
} connmgr_thread_args_t;

/**
 * \brief サーバソケットを立ち上げ、クライアントとの接続を待機する。
 * \param port_number   サーバが待ち受けるポート番号
 * \param max_clients   受け付けるクライアント回数(接続数)
 * \param sbuf          共有バッファへのポインタ
 * \param logger_func   ログ出力用関数ポインタ
 */
void connmgr_listen(int port_number, int max_clients, sbuffer_t *sbuf,
                    void (*logger_func)(const char *));

#endif
