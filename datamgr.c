#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "datamgr.h"
#include "sbuffer.h"
#include "lib/dplist.h"

#ifndef SET_MIN_TEMP
#define SET_MIN_TEMP 10
#endif

#ifndef SET_MAX_TEMP
#define SET_MAX_TEMP 30
#endif

static void (*g_logger)(const char *) = NULL;

// room_sensor.map の情報を保持する簡易構造体
typedef struct
{
    sensor_id_t sensor_id;
    uint16_t room_id;
} sensor_map_t;

// dplistのコールバック関数 (ここでは最小限)
static void *element_copy(void *src)
{
    sensor_map_t *orig = (sensor_map_t *)src;
    sensor_map_t *copy = malloc(sizeof(sensor_map_t));
    *copy = *orig;
    return copy;
}
static void element_free(void **element)
{
    free(*element);
    *element = NULL;
}
static int element_compare(void *x, void *y)
{
    sensor_map_t *a = (sensor_map_t *)x;
    sensor_map_t *b = (sensor_map_t *)y;
    if (a->sensor_id < b->sensor_id)
        return -1;
    if (a->sensor_id > b->sensor_id)
        return 1;
    return 0;
}

static dplist_t *g_sensor_map = NULL;

// ----------------------------------------------------------------------------
// room_sensor.map を読み込み、g_sensor_map (dplist) に詰め込む
// ----------------------------------------------------------------------------
static void load_sensor_map(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        char msg[128];
        snprintf(msg, sizeof(msg), "Could not open map file: %s", filename);
        g_logger(msg);
        return;
    }
    uint16_t room_id, sensor_id;
    while (!feof(fp))
    {
        if (fscanf(fp, "%hu %hu", &room_id, &sensor_id) == 2)
        {
            sensor_map_t map;
            map.room_id = room_id;
            map.sensor_id = sensor_id;
            // dplistに追加
            g_sensor_map = dpl_insert_at_index(g_sensor_map, &map, -1, true);
        }
    }
    fclose(fp);
}

// ----------------------------------------------------------------------------
// sbuffer から要素を取り出し、センサIDが有効かどうかチェックする。
// 適宜「too hot」「too cold」「invalid sensor node ID」などをログ出力
// ----------------------------------------------------------------------------
void datamgr_parse_sensor_data(sbuffer_t *sbuf, const char *room_sensor_map,
                               void (*logger_func)(const char *))
{
    g_logger = logger_func;
    // dplist作成
    g_sensor_map = dpl_create(element_copy, element_free, element_compare);
    load_sensor_map(room_sensor_map);

    // 共有バッファからの読み取りループ (抜け方は適宜工夫)
    while (1)
    {
        sensor_data_t data;
        int res = sbuffer_remove_reader(sbuf, &data);
        if (res == SBUFFER_NO_DATA)
        {
            // バッファにデータが無いだけ => 少し待ってリトライ
            usleep(200000);
            continue;
        }
        else if (res == SBUFFER_STOP_THREAD)
        {
            // sbuffer 側で終了指示が出たと判断しループ離脱
            break;
        }
        else if (res == SBUFFER_SUCCESS)
        {
            // データを取得できた: センサIDが valid か?
            // dplistから検索してみる
            sensor_map_t tmp;
            tmp.sensor_id = data.id;
            tmp.room_id = 0;
            int idx = dpl_get_index_of_element(g_sensor_map, &tmp);
            if (idx < 0)
            {
                // invalid sensor ID
                char msg[128];
                snprintf(msg, sizeof(msg), "Received sensor data with invalid sensor node ID %u", data.id);
                g_logger(msg);
                // 次へ
                continue;
            }
            // 有効センサであれば、温度が閾値を外れていないかチェック
            // (ランニングアベレージ計算は省略・一例として即時値で判定)
            if (data.value < SET_MIN_TEMP)
            {
                char msg[128];
                snprintf(msg, sizeof(msg), "Sensor node %u reports it’s too cold (avg temp = %g)", data.id, data.value);
                g_logger(msg);
            }
            else if (data.value > SET_MAX_TEMP)
            {
                char msg[128];
                snprintf(msg, sizeof(msg), "Sensor node %u reports it’s too hot (avg temp = %g)", data.id, data.value);
                g_logger(msg);
            }
            // 特になしの場合は何もしない
        }
    }

    // 終了処理
    dpl_free(&g_sensor_map, true);
}
