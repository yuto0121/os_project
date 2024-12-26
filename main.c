#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include "config.h"
#include "connmgr.h"
#include "datamgr.h"
#include "sensor_db.h"
#include "sbuffer.h"

// ---- ロギングパイプ関連のグローバル変数  ----
static int g_pipefd[2]; // g_pipefd[0] = read end, g_pipefd[1] = write end
pthread_mutex_t g_log_pipe_mutex = PTHREAD_MUTEX_INITIALIZER;

// ---- スレッドIDを保持 ----
pthread_t g_connmgr_thread;
pthread_t g_datamgr_thread;
pthread_t g_storage_thread;

// ---- 共有バッファ ----
sbuffer_t *g_sbuffer = NULL;

// ---- 実行中フラグ ----
// static volatile int g_running = 1;

// ログ書き込み用のヘルパー関数
//   (複数スレッドから呼ばれる可能性があるので、
//    ここでは pipe へ書き込みする際にmutexで排他する)

void write_log_event(const char *msg)
{
    pthread_mutex_lock(&g_log_pipe_mutex);
    // パイプへ ASCII文字列を書き込み(末尾に改行しておく)
    dprintf(g_pipefd[1], "%s\n", msg);
    pthread_mutex_unlock(&g_log_pipe_mutex);
}

// 子プロセス(ログプロセス)用の関数
// パイプからログイベント文字列を受信し、gateway.log に書き込む

static void log_process_loop(void)
{
    // 新規作成モードで gateway.log を開き直す
    FILE *fp_log = fopen("gateway.log", "w");
    if (!fp_log)
    {
        perror("fopen gateway.log");
        exit(EXIT_FAILURE);
    }

    // パイプのwrite側は使わないので閉じる
    close(g_pipefd[1]);

    // ログイベントを1行ずつ読み取り、出力
    char buffer[512];
    long sequence_num = 1;
    time_t now;
    while (1)
    {
        // 一行ずつ読み取る
        ssize_t n = 0;
        memset(buffer, 0, sizeof(buffer));
        n = read(g_pipefd[0], buffer, sizeof(buffer) - 1);
        if (n > 0)
        {
            // タイムスタンプの取得
            time(&now);
            // 改行除去(一応)
            buffer[strcspn(buffer, "\r\n")] = 0;

            // ログファイルに出力: <sequence number> <timestamp> <msg>
            fprintf(fp_log, "%ld %ld %s\n", sequence_num, (long)now, buffer);
            fflush(fp_log);
            sequence_num++;
        }
        else if (n == 0)
        {
            // 親がパイプをclose => 書き込み終了
            break;
        }
        else
        {
            if (errno == EINTR)
                continue; // シグナル割込みなら続行
            break;
        }
    }
    fclose(fp_log);
    close(g_pipefd[0]);
    exit(EXIT_SUCCESS);
}

// コネクションマネージャスレッド

static void *connection_manager_thread(void *arg)
{
    // 引数の取得
    int *p_args = (int *)arg;
    int port_number = p_args[0];
    int max_clients = p_args[1];
    free(p_args); // 動的確保した引数を解放

    // connmgr_listen() はブロッキングでクライアント接続を受け付ける想定
    connmgr_listen(port_number, max_clients, g_sbuffer, write_log_event);

    // コネクションマネージャ終了処理
    write_log_event("Connection manager thread finished");
    pthread_exit(NULL);
}

// データマネージャスレッド
//   shared buffer から計測値を取り出して、room_sensor.map をもとに解析

static void *data_manager_thread(void *arg)
{
    datamgr_parse_sensor_data(g_sbuffer, "room_sensor.map", write_log_event);
    write_log_event("Data manager thread finished");
    pthread_exit(NULL);
}

// ストレージマネージャスレッド
//   shared buffer から計測値を取り出して csv に保存

static void *storage_manager_thread(void *arg)
{
    FILE *fp_csv = fopen("data.csv", "w");
    if (!fp_csv)
    {
        // ログ出力
        write_log_event("Could not open data.csv for writing");
        pthread_exit(NULL);
    }
    // 新規作成ができたならログ出力
    write_log_event("A new data.csv file has been created.");

    sensor_db_run(g_sbuffer, fp_csv, write_log_event);

    // 終了
    write_log_event("The data.csv file has been closed.");
    fclose(fp_csv);
    pthread_exit(NULL);
}

// メイン関数

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <port_number> <max_clients>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port_number = atoi(argv[1]);
    int max_clients = atoi(argv[2]);

    // ---- パイプ作成 ----
    if (pipe(g_pipefd) < 0)
    {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // ---- fork() で子プロセス生成 -> ログプロセス ----
    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0)
    {
        // 子プロセス => ログプロセス
        log_process_loop(); // 終了しない (ログプロセス側はこの関数内で exit() する)
    }

    // 親プロセス => メインプロセス
    // ログプロセス側で使う読み取りFDは閉じる
    close(g_pipefd[0]);

    // shared buffer の初期化
    if (sbuffer_init(&g_sbuffer) != 0)
    {
        perror("sbuffer_init failed");
        exit(EXIT_FAILURE);
    }

    // ---- コネクションマネージャスレッド起動 ----
    int *thread_args = (int *)malloc(sizeof(int) * 2);
    thread_args[0] = port_number;
    thread_args[1] = max_clients;

    pthread_create(&g_connmgr_thread, NULL, connection_manager_thread, (void *)thread_args);

    // ---- データマネージャスレッド起動 ----
    pthread_create(&g_datamgr_thread, NULL, data_manager_thread, NULL);

    // ---- ストレージマネージャスレッド起動 ----
    pthread_create(&g_storage_thread, NULL, storage_manager_thread, NULL);

    // ---- スレッド終了待ち ----
    pthread_join(g_connmgr_thread, NULL);
    pthread_join(g_datamgr_thread, NULL);
    pthread_join(g_storage_thread, NULL);

    // ---- shared buffer解放 ----
    sbuffer_free(&g_sbuffer);

    // パイプを閉じて、ログプロセスにEOFを送る => ログプロセス終了へ
    close(g_pipefd[1]);

    // 子プロセス(ログ)の終了待ち
    wait(NULL);

    return 0;
}
