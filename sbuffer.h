// #ifndef SBUFFER_H
// #define SBUFFER_H

// #include <time.h>

// // センサデータ構造
// typedef struct
// {
//     int id;
//     double value; // 温度
//     time_t ts;    // タイムスタンプ
// } sensor_data_t;

// // sbuffer の前方宣言
// typedef struct sbuffer sbuffer_t;

// // sbuffer の関数プロトタイプ
// int sbuffer_init(sbuffer_t **buffer);
// int sbuffer_free(sbuffer_t **buffer);
// int sbuffer_insert(sbuffer_t *buffer, sensor_data_t *data);
// int sbuffer_remove(sbuffer_t *buffer, sensor_data_t *data);

// #endif /* SBUFFER_H */
// /**
//  * \file sbuffer.h
//  * \brief 共有バッファ(sbuffer)のインタフェース
//  */
// #ifndef _SBUFFER_H_
// #define _SBUFFER_H_

// #include "config.h"

// #define SBUFFER_SUCCESS 0
// #define SBUFFER_NO_DATA 10
// #define SBUFFER_STOP_THREAD 20

// typedef struct sbuffer sbuffer_t;

// /**
//  * \brief sbuffer 構造体のメモリ確保と初期化
//  * \param buffer [out] 作成されたバッファへの二重ポインタ
//  * \return 0 if ok, -1 if error
//  */
// int sbuffer_init(sbuffer_t **buffer);

// /**
//  * \brief sbuffer 解放
//  * \param buffer [in,out] 対象バッファ(二重ポインタ)。成功後は *buffer=NULL となる
//  * \return 0 if ok, -1 if error
//  */
// int sbuffer_free(sbuffer_t **buffer);

// /**
//  * \brief バッファへのデータ追加(書き込み)
//  * \param buffer [in]
//  * \param data   [in]  追加したいセンサデータ
//  * \return 0 if ok, -1 if error
//  */
// int sbuffer_insert(sbuffer_t *buffer, sensor_data_t *data);

// /**
//  * \brief バッファから1件読み取り(リーダ用)。読み取ったノードは内部カウンタを減らす
//  * \param buffer [in]
//  * \param data   [out]
//  * \return
//  *  - SBUFFER_SUCCESS      => 1件読み取り成功
//  *  - SBUFFER_NO_DATA      => データが無い
//  *  - SBUFFER_STOP_THREAD  => sbuffer_set_stop()が呼ばれた後
//  */
// int sbuffer_remove_reader(sbuffer_t *buffer, sensor_data_t *data);

// /**
//  * \brief バッファに格納されている全データを破棄し、以後リーダに終了を促す
//  * \param buffer
//  */
// void sbuffer_set_stop(sbuffer_t *buffer);

// #endif

#ifndef _SBUFFER_H_
#define _SBUFFER_H_

#include "config.h" // ★ここでsensor_data_tを使う

#define SBUFFER_SUCCESS 0
#define SBUFFER_NO_DATA 10
#define SBUFFER_STOP_THREAD 20

typedef struct sbuffer sbuffer_t;

int sbuffer_init(sbuffer_t **buffer);
int sbuffer_free(sbuffer_t **buffer);
int sbuffer_insert(sbuffer_t *buffer, sensor_data_t *data);
int sbuffer_remove_reader(sbuffer_t *buffer, sensor_data_t *data);
void sbuffer_set_stop(sbuffer_t *buffer);

#endif // _SBUFFER_H_
