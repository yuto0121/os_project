#ifndef _SENSOR_DB_H_
#define _SENSOR_DB_H_

#include <stdio.h>
#include "config.h"
#include "sbuffer.h"

/**
 * \brief sbuffer からセンサデータを取り出し、CSVファイルへ追記する
 *        (無限ループし、sbuffer が終了指示を出すまで動き続ける)
 *
 * \param sbuf           共有バッファ
 * \param fp_csv         "data.csv" 用ファイルポインタ (既に fopen されたもの)
 * \param logger_func    ログ出力用関数ポインタ
 */
void sensor_db_run(sbuffer_t *sbuf, FILE *fp_csv, void (*logger_func)(const char *));

#endif
