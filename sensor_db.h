#ifndef _SENSOR_DB_H_
#define _SENSOR_DB_H_

#include <stdio.h>
#include "config.h"
#include "sbuffer.h"

/**
 * \brief
 * \param sbuf
 * \param fp_csv
 * \param logger_func
 */
void sensor_db_run(sbuffer_t *sbuf, FILE *fp_csv, void (*logger_func)(const char *));

#endif
