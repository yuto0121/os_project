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

static int g_pipefd[2];
pthread_mutex_t g_log_pipe_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_t g_connmgr_thread;
pthread_t g_datamgr_thread;
pthread_t g_storage_thread;

sbuffer_t *g_sbuffer = NULL;

void write_log_event(const char *msg)
{
    pthread_mutex_lock(&g_log_pipe_mutex);
    dprintf(g_pipefd[1], "%s\n", msg);
    pthread_mutex_unlock(&g_log_pipe_mutex);
}

static void log_process_loop(void)
{
    FILE *fp_log = fopen("gateway.log", "w");
    if (!fp_log)
    {
        perror("fopen gateway.log");
        exit(EXIT_FAILURE);
    }

    close(g_pipefd[1]);

    char buffer[512];
    long sequence_num = 1;
    time_t now;
    while (1)
    {
        ssize_t n = 0;
        memset(buffer, 0, sizeof(buffer));
        n = read(g_pipefd[0], buffer, sizeof(buffer) - 1);
        if (n > 0)
        {
            time(&now);
            buffer[strcspn(buffer, "\r\n")] = 0;

            fprintf(fp_log, "%ld %ld %s\n", sequence_num, (long)now, buffer);
            fflush(fp_log);
            sequence_num++;
        }
        else if (n == 0)
        {
            break;
        }
        else
        {
            if (errno == EINTR)
                continue;
            break;
        }
    }
    fclose(fp_log);
    close(g_pipefd[0]);
    exit(EXIT_SUCCESS);
}

static void *connection_manager_thread(void *arg)
{
    int *p_args = (int *)arg;
    int port_number = p_args[0];
    int max_clients = p_args[1];
    free(p_args);

    connmgr_listen(port_number, max_clients, g_sbuffer, write_log_event);

    write_log_event("Connection manager thread finished");
    pthread_exit(NULL);
}

static void *data_manager_thread(void *arg)
{
    datamgr_parse_sensor_data(g_sbuffer, "room_sensor.map", write_log_event);
    write_log_event("Data manager thread finished");
    pthread_exit(NULL);
}

static void *storage_manager_thread(void *arg)
{
    FILE *fp_csv = fopen("data.csv", "w");
    if (!fp_csv)
    {
        write_log_event("Could not open data.csv for writing");
        pthread_exit(NULL);
    }
    write_log_event("A new data.csv file has been created.");

    sensor_db_run(g_sbuffer, fp_csv, write_log_event);

    write_log_event("The data.csv file has been closed.");
    fclose(fp_csv);
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <port_number> <max_clients>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port_number = atoi(argv[1]);
    int max_clients = atoi(argv[2]);

    if (pipe(g_pipefd) < 0)
    {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0)
    {
        log_process_loop();
    }

    close(g_pipefd[0]);

    if (sbuffer_init(&g_sbuffer) != 0)
    {
        perror("sbuffer_init failed");
        exit(EXIT_FAILURE);
    }

    int *thread_args = (int *)malloc(sizeof(int) * 2);
    thread_args[0] = port_number;
    thread_args[1] = max_clients;

    pthread_create(&g_connmgr_thread, NULL, connection_manager_thread, (void *)thread_args);

    pthread_create(&g_datamgr_thread, NULL, data_manager_thread, NULL);

    pthread_create(&g_storage_thread, NULL, storage_manager_thread, NULL);

    pthread_join(g_connmgr_thread, NULL);
    pthread_join(g_datamgr_thread, NULL);
    pthread_join(g_storage_thread, NULL);

    sbuffer_free(&g_sbuffer);

    close(g_pipefd[1]);

    wait(NULL);

    return 0;
}
