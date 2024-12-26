#define _GNU_SOURCE
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
typedef struct
{
    sensor_id_t sensor_id;
    uint16_t room_id;
} sensor_map_t;
static dplist_t *g_sensor_map = NULL;

static void *element_copy(void *src)
{
    sensor_map_t *o = (sensor_map_t *)src;
    sensor_map_t *c = malloc(sizeof(sensor_map_t));
    *c = *o;
    return c;
}
static void element_free(void **e)
{
    free(*e);
    *e = NULL;
}
static int element_compare(void *x, void *y)
{
    sensor_map_t *a = (sensor_map_t *)x, *b = (sensor_map_t *)y;
    if (a->sensor_id < b->sensor_id)
        return -1;
    if (a->sensor_id > b->sensor_id)
        return 1;
    return 0;
}

static void load_sensor_map(const char *f)
{
    FILE *fp = fopen(f, "r");
    if (!fp)
    {
        char msg[128];
        snprintf(msg, sizeof(msg), "Could not open map file: %s", f);
        g_logger(msg);
        return;
    }
    uint16_t r, s;
    while (!feof(fp))
    {
        if (fscanf(fp, "%hu %hu", &r, &s) == 2)
        {
            sensor_map_t m;
            m.room_id = r;
            m.sensor_id = s;
            g_sensor_map = dpl_insert_at_index(g_sensor_map, &m, -1, 1);
        }
    }
    fclose(fp);
}

void datamgr_parse_sensor_data(sbuffer_t *sbuf, const char *f, void (*logger_func)(const char *))
{
    g_logger = logger_func;
    g_sensor_map = dpl_create(element_copy, element_free, element_compare);
    load_sensor_map(f);
    for (;;)
    {
        sensor_data_t data;
        int res = sbuffer_remove_reader(sbuf, &data);
        if (res == SBUFFER_NO_DATA)
        {
            usleep(10000);
            continue;
        }
        else if (res == SBUFFER_STOP_THREAD)
            break;
        else if (res == SBUFFER_SUCCESS)
        {
            sensor_map_t tmp;
            tmp.sensor_id = data.id;
            tmp.room_id = 0;
            int idx = dpl_get_index_of_element(g_sensor_map, &tmp);
            if (idx < 0)
            {
                char msg[128];
                snprintf(msg, sizeof(msg), "Received sensor data with invalid sensor node ID %u", data.id);
                g_logger(msg);
                continue;
            }
            if (data.value < SET_MIN_TEMP)
            {
                char msg[128];
                snprintf(msg, sizeof(msg), "Sensor node %u reports it’s too cold (avg temp = %.2f)", data.id, data.value);
                g_logger(msg);
            }
            else if (data.value > SET_MAX_TEMP)
            {
                char msg[128];
                snprintf(msg, sizeof(msg), "Sensor node %u reports it’s too hot (avg temp = %.2f)", data.id, data.value);
                g_logger(msg);
            }
        }
    }
    dpl_free(&g_sensor_map, 1);
}
