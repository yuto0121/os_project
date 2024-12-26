#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include "sbuffer.h"

// 読み手の数を固定(2スレッド: datamgr, storagemgr)
#define NUM_READERS 2

typedef struct sbuffer_node
{
    sensor_data_t data;
    int readers_left; // まだ読み取っていないリーダ数
    struct sbuffer_node *next;
} sbuffer_node_t;

struct sbuffer
{
    sbuffer_node_t *head;
    sbuffer_node_t *tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond; // データが追加された or 状態変化があったら通知
    int stop;            // 終了指示フラグ
};

int sbuffer_init(sbuffer_t **buffer)
{
    *buffer = malloc(sizeof(sbuffer_t));
    if (!(*buffer))
        return -1;
    (*buffer)->head = NULL;
    (*buffer)->tail = NULL;
    pthread_mutex_init(&(*buffer)->mutex, NULL);
    pthread_cond_init(&(*buffer)->cond, NULL);
    (*buffer)->stop = 0;
    return 0;
}

int sbuffer_free(sbuffer_t **buffer)
{
    if (!buffer || !*buffer)
        return -1;

    // ノードを全て開放
    sbuffer_node_t *dummy, *curr = (*buffer)->head;
    while (curr)
    {
        dummy = curr;
        curr = curr->next;
        free(dummy);
    }
    pthread_mutex_destroy(&(*buffer)->mutex);
    pthread_cond_destroy(&(*buffer)->cond);
    free(*buffer);
    *buffer = NULL;
    return 0;
}

// ----------------------------------------------------------------------------
// データ追加 (コネクションマネージャから呼ばれる想定)
//   新たなノードを末尾に追加し、通知
// ----------------------------------------------------------------------------
int sbuffer_insert(sbuffer_t *buffer, sensor_data_t *data)
{
    if (!buffer)
        return -1;

    pthread_mutex_lock(&buffer->mutex);

    sbuffer_node_t *new_node = malloc(sizeof(sbuffer_node_t));
    if (!new_node)
    {
        pthread_mutex_unlock(&buffer->mutex);
        return -1;
    }
    new_node->data = *data;
    new_node->readers_left = NUM_READERS;
    new_node->next = NULL;

    if (!buffer->head)
    {
        buffer->head = new_node;
        buffer->tail = new_node;
    }
    else
    {
        buffer->tail->next = new_node;
        buffer->tail = new_node;
    }

    pthread_cond_broadcast(&buffer->cond); // データ追加を通知
    pthread_mutex_unlock(&buffer->mutex);
    return 0;
}

int sbuffer_remove_reader(sbuffer_t *buffer, sensor_data_t *data)
{
    if (!buffer)
        return -1;

    pthread_mutex_lock(&buffer->mutex);

    // stopフラグが立っていたら終了
    if (buffer->stop)
    {
        pthread_mutex_unlock(&buffer->mutex);
        return SBUFFER_STOP_THREAD;
    }

    // ノードが無いならデータなし
    if (!buffer->head)
    {
        pthread_mutex_unlock(&buffer->mutex);
        return SBUFFER_NO_DATA;
    }

    // 先頭ノードからデータ取得
    sbuffer_node_t *node = buffer->head;
    *data = node->data;

    // 読み終わったので readers_left を decrement
    node->readers_left--;
    if (node->readers_left <= 0)
    {
        // このノードをリストから外してfree
        buffer->head = node->next;
        if (!buffer->head)
        {
            buffer->tail = NULL;
        }
        free(node);
    }

    pthread_mutex_unlock(&buffer->mutex);
    return SBUFFER_SUCCESS;
}

void sbuffer_set_stop(sbuffer_t *buffer)
{
    pthread_mutex_lock(&buffer->mutex);
    buffer->stop = 1;
    pthread_cond_broadcast(&buffer->cond);
    pthread_mutex_unlock(&buffer->mutex);
}