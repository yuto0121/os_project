#ifndef _DATAMGR_H_
#define _DATAMGR_H_

#include "config.h"
#include "sbuffer.h"

/**
 * \brief
 * \param sbuf
 * \param room_sensor_map
 * \param logger_func
 */
void datamgr_parse_sensor_data(sbuffer_t *sbuf, const char *room_sensor_map,
                               void (*logger_func)(const char *));

#endif
