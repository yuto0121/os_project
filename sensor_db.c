#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "sensor_db.h"
#include "sbuffer.h"

static void (*g_logger)(const char *) = NULL;

void sensor_db_run(sbuffer_t *sbuf, FILE *fp_csv, void (*logger_func)(const char *))
{
    g_logger = logger_func;
    // CSVヘッダ行など書いてもよい
    // fprintf(fp_csv, "sensor_id,value,timestamp\n");

    while (1)
    {
        sensor_data_t data;
        int res = sbuffer_remove_reader(sbuf, &data);
        if (res == SBUFFER_NO_DATA)
        {
            // データが無い => 適当にsleep
            usleep(200000);
            continue;
        }
        else if (res == SBUFFER_STOP_THREAD)
        {
            // sbuffer 側で終了指示
            break;
        }
        else if (res == SBUFFER_SUCCESS)
        {
            // CSVへ書き込み
            fprintf(fp_csv, "%u,%.2f,%ld\n", data.id, data.value, (long)data.ts);

            // ログ出力 (例)
            char msg[128];
            snprintf(msg, sizeof(msg), "Data insertion from sensor %u succeeded.", data.id);
            g_logger(msg);
        }
    }
}
