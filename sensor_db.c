/**
 * \author Yuto Yamaguchi
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "sensor_db.h"
#include "sbuffer.h"

static void (*g_logger)(const char *) = NULL;

void sensor_db_run(sbuffer_t *s, FILE *fp, void (*f)(const char *))
{
    g_logger = f;
    while (1)
    {
        sensor_data_t d;
        int r = sbuffer_remove_reader(s, &d);
        if (r == SBUFFER_NO_DATA)
        {
            usleep(10000);
            continue;
        }
        else if (r == SBUFFER_STOP_THREAD)
            break;
        else if (r == SBUFFER_SUCCESS)
        {
            struct tm *tm_info;
            char time_str[26];
            tm_info = localtime(&(d.ts));
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
            fprintf(fp, "%u,%.2f,%s\n", d.id, d.value, time_str);
            fflush(fp);
            char msg[128];
            snprintf(msg, sizeof(msg), "Data insertion from sensor %u succeeded.", d.id);
            g_logger(msg);
        }
    }
}
