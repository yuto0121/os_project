#ifndef _DATAMGR_H_
#define _DATAMGR_H_

#include "config.h"
#include "sbuffer.h"

/**
 * \brief sbuffer 内のセンサデータを読み取り、room_sensor.map の情報を用いて
 *        適宜ログ出力等を行う
 *
 * \param sbuf           共有バッファ
 * \param room_sensor_map  "room_sensor.map" のファイルパス
 * \param logger_func    ログ出力用の関数ポインタ
 */
void datamgr_parse_sensor_data(sbuffer_t *sbuf, const char *room_sensor_map,
                               void (*logger_func)(const char *));

#endif
