/*
* 子機プログラム
* ・親機からのLED信号の受信
* ・叩く箇所を示すLEDの制御
* ・センサーの生データ処理（閾値での押した判定をとる）
* Created by K22056
*
* 変更履歴：
*   2024.07.04 初版
*   2024.07.24 デモ用　最終
*
* UART通信仕様：
*   255(0xFF) 0~254
*   Header 命令
*   子機はヘッダーを受信すると命令受信を待機する。
*/

#include "Micon2024_CustomLib.h"

#define ENABLE_SENSOR_DEBUG 0

#define S_RESULT_SWITCH_TIME 1000

// 閾値設定
const int SENSOR_BASE_TH[] = {200, 200, 200, 200, 200};

// 使用ピン設定
const int SENSOR_PIN[] = {A0, A1, A2, A3, A4};
const int LED_PIN[] = {4, 5, 6, 7, 8};

// プログラムの状態管理用（親機と同期するために使用）
int _state = STATE_INIT; // 0:初期状態 1:Ready状態 2:ゲーム中 3:リザルト

void setup()
{
  // シリアル通信準備
  Serial.begin(UART_BITRATE);
  // ピンモード設定
  for(int i = 0; i < 5; i++)
  {
    pinMode(SENSOR_PIN[i], INPUT);
    pinMode(LED_PIN[i], OUTPUT);
  }
  // 子機の方が初期化はやいので遅延させる
  delay(1000);
  // 初期化終了を通知する
  _send(NOTICE_FINISH_INIT);
  // Ready状態へ移行
  _state = STATE_READY;
}

void loop()
{
  if(!ENABLE_SENSOR_DEBUG)
  {
    // メインループ
    switch(_state)
    {
      case STATE_READY: // Ready状態
        // 待機アニメーションなど
        Ready();
        break;
      case STATE_GAME:
        // ゲーム中
        Game();
        break;
      case STATE_RESULT:
        Result();
        break;
      case 4:
        //
        break;
    }
  }else{
    // 圧力センサーデバッグ用
    // 閾値の確認・設定に使用する
    // 閾値を超えたセンサーについてLEDで通知する
    int value[5];
    int result[5];
    for(int i = 0; i < 5; i++)
    {
      value[i] = analogRead(SENSOR_PIN[i]);
      result[i] = TH_Check(i, value[i]);
    }
    // LED 制御
    for(int i = 0; i < 5; i++)
    {
      digitalWrite(LED_PIN[i], result[i]);
    }
  }
}

void Ready()
{
  for(int i = 0; i < 5; i++)
  {
    digitalWrite(LED_PIN[i], HIGH);
    delay(100);
    digitalWrite(LED_PIN[i], LOW);
    delay(100);
  }
  // 命令受信確認
  if(Serial.available() != 0)
  {
    // 受信処理
    uint8_t data = (uint8_t)Serial.read();
    // ヘッダー認識
    if (data == 0xFF) {
      // データ待機
      while (Serial.available() == 0) {
        // 1byte分のデータ受信を待機
      }
      // データ受信
      int arg = (int)Serial.read();
      // 命令の認識
      switch(arg)
      {
        case MEIREI_CHANGE_STATE_READY:
          _state = STATE_READY;
          break;
        case MEIREI_CHANGE_STATE_GAME:
          _state = STATE_GAME;
          break;
        case MEIREI_CHANGE_STATE_RESULT:
          _state = STATE_RESULT;
          break;
      }
    }
  }
}

// game中の処理
void Game()
{
  // 親機からゲームを続行するかどうかの情報を受け取る
  // 通信待機
  while(true) // 子機からのデータ受信待ちループ
  {
    // 通信待ち
    while (Serial.available() == 0)
    {
      // delayを入れる
      delay(5);
    }
    // 受信処理
    uint8_t data = (uint8_t)Serial.read();
    // ヘッダー認識
    if (data == 0xFF) {
      // データ待機
      while (Serial.available() < 1) {
        // 1byte分のデータ受信を待機
        // センサー５台分の判定結果
      }
      // データ受信
      int order = Serial.read();
      if(order == MEIREI_GAME_END)
      {
        // ゲーム終了
        _state = STATE_RESULT;
      }else{
        // MEIREI_GAME_END以外の命令は破棄する
      }
      // ここではタイミングを同期する必要がないので受信通知はしない
      break;
    }
  }

  // センサデータ取得
  int value[5]; // センサデータ
  int result[5]; // 閾値判定結果
  for(int i = 0; i < 5; i++)
  {
    // センサデータ取得
    value[i] = analogRead(SENSOR_PIN[i]);
    // 閾値判定
    result[i] = TH_Check(i, value[i]);
  }

  // 判定結果送信
  Serial.write(255);
  for(int i = 0; i < 5; i++)
  {
    Serial.write(result[i]);
  }
  // 受信完了待ち
  while(true)
  {
    while (Serial.available() == 0)
    {
      // delayを入れる
      delay(5);
    }
    // 受信処理
    uint8_t data = (uint8_t)Serial.read();
    if(data == 0xFF)
    {
      while (Serial.available() == 0) {
        // 1byte分のデータ受信を待機
      }
      // 破棄
      int _ = (int)Serial.read();
      // 受信完了
      break;
    }
  }

  // LEDの制御
  int resultArr[5];
  while(true) // データ受信待機ループ
  {
    // 受信待ち
    while (Serial.available() == 0)
    {
      // delayを入れる
      delay(5);
    }
    // 受信処理
    uint8_t data = (uint8_t)Serial.read();
    // ヘッダー認識
    if (data == 0xFF) {
      // データ待機
      while (Serial.available() < 5) {
        // 5byte分のデータ受信を待機
      }
      // データ受信
      for(int i = 0; i < 5; i++)
      {
        resultArr[i] = (int)Serial.read();
      }
      // ここでは受信完了通知を発行しない
      // LEDの制御完了後に発行する
      break;
    }
  }
  // 受信データを元にLED制御
  for(int i = 0; i < 5; i++)
  {
    digitalWrite(LED_PIN[i], resultArr[i]);
  }
  // 全処理完了
  _send(MEIREI_VOID);
}

void Result()
{
  // リザルト処理

  // LED点滅処理
  static bool _resultState = false; // ResultにおけるLEDの状態保持
  static uint32_t _lastLedSwitchTime = 0; // 前回の切り替え評価時間を記憶
  uint32_t _currentSwitchEvaluationTime = millis(); // 評価時間
  if(_currentSwitchEvaluationTime - _lastLedSwitchTime > S_RESULT_SWITCH_TIME)
  {
    _resultState = !_resultState; // LEDの状態反転
  }
  // 前回の切り替え評価時間を更新
  _currentSwitchEvaluationTime = _currentSwitchEvaluationTime;
  // 全モグラの目を点滅制御
  for(int i = 0; i < 5; i++)
  {
    digitalWrite(LED_PIN[i], _resultState);
  }
  // 親機からの命令受待機
  if(Serial.available() != 0)
  {
    // 受信処理
    uint8_t data = (uint8_t)Serial.read();
    // ヘッダー認識
    if (data == 0xFF) {
      // データ待機
      while (Serial.available() < 1) {
        // 1byte分のデータ受信を待機
      }
      // データ受信
      int order = (int)Serial.read();
      // 命令の識別
      if(order == MEIREI_CHANGE_STATE_READY)
      {
        // Ready状態に遷移
        _state = STATE_READY;
      }
    }
  }
}

// UART通信をする関数
void _send(uint8_t order)
{
  // ヘッダー送信
  Serial.write(255);
  // データ送信
  Serial.write(order);
}

// 閾値判定用のメソッド
bool TH_Check(int id, int value)
{
  if(value > SENSOR_BASE_TH)
  {
    return true;
  }
  return false;
}
