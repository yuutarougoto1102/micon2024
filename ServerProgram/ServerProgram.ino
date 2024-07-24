/*
* メインプログラム（親機用）
* ・ゲームの進行
* ・LEDセグの制御
* ・時間用のインジゲータ制御
* Created by K22056
*
* 変更履歴：
*   2024.07.04 初版
*   2024.07.24 デモ用　最終
*
* UART通信仕様：
*   255(0xFF) 0~254 0~254
*   Header 子機番号 命令
*   子機はヘッダーを受信すると命令受信を待機する。
*   各子機にスクラッチを書き込む際は子機番号を必ず指定する
*/

#include "Micon2024_CustomLib.h"

// システム設定
#define S_INTERRUPTION_SENSE 100 // 割り込み猶予時間の設定(ms)

// メカ設定
#define M_SENSOR_THRESHOLD 200 // 圧力センサ閾値
#define M_SENSOR_THRESHOLD_RANGE 20 // 閾値の幅設定

// ゲーム設定
#define G_WAIT_TIME 5000 // ゲーム開始前の待機時間(ms)
#define G_DEFAULT_TIME 20000 // デフォルトの制限時間(ms)
#define G_DISTANCE_MIN 1000 // モグラの出現時間の最小値(ms)
#define G_DISTANCE_MAX 2000 // モグラの出現時間の最大値(ms)
#define G_MAX_MOGURA_PICK 4 // モグラの最大同時出現数(個)
#define G_MOGURA_UPDATE_RATE 800 // 時間(ms)

// STATE
#define STATE_INIT 0
#define STATE_READY 1
#define STATE_GAME 2
#define STATE_RESULT 3

// GAME STATE
#define GAME_STATE_COUNTDOWN 0
#define GAME_STATE_PLAYING 1
#define GAME_STATE_FEAVOR 2
#define GAME_STATE_END 3

// 使用ピン設定
const int START_BUTTON_PIN = 2; // スタートボタン用ピン
const int OPTION_BUTTON_PIN = -1; // オプションボタン用ピン
const int LED_PIN = -1; //LED用ピン
const int STATE_INDICATOR_LED = 13; //インジケータ用LEDピン
// 7セグ制御ピン
const int SEG_CONTROLLER_1[] = {4,5,6,7};
//const int SEG_CONTROLLER_1_BI_PIN = A0;
const int SEG_CONTROLLER_0[] = {8,9,10,11};
//const int SEG_CONTROLLER_0_BI_PIN = A1;

// 割り込み管理
volatile bool startButtonPressed = false;
volatile bool optionButtonPressed = false;

// メインプログラムの状態管理用
int _state = STATE_READY; // 0:初期状態 1:Ready状態 2:ゲーム中 3:リザルト

// ゲーム関連
int _gameState = GAME_PREGAME; // 0:カウントダウン 1:ゲーム中 2:フィーバー 3:終了
bool _gameEndFlag = false;

unsigned long _gameStartTime = 0; // ゲーム開始時間の保持
int _score = 0; // ゲームスコア保持

int _pickCount = 0; //現在の点灯数を保持

int pickedMogura[5] = {0, 0, 0, 0, 0}; // 点灯中のモグラデータ
// モグラの状態を最後に更新した時間を保持（ループが早すぎて、制限しないと更新回数が多く
// なりすぎてしまう->モグラが常に最大数点灯してしまう　これを解決するために更新頻度を抑える
uint32_t lastUpdateMogura = 0;

struct MoguraState
{
  int id; // 0~4
  uint32_t startTime; // 点灯開始時間
  uint32_t distance; // 点灯時間
};
MoguraState moguraStateArr[5]; // モグラの点灯情報を保持する配列

// セグメント管理
int _nowSegNumber = 0;

// 初期化
void setup() {
  // シリアル通信準備
  Serial.begin(UART_BITRATE);
  // ピンモード設定
  pinMode(START_BUTTON_PIN, INPUT_PULLUP);
  pinMode(STATE_INDICATOR_LED, OUTPUT);
  // セグのピン
  for(int i = 0; i<4; i++)
  {
    pinMode(SEG_CONTROLLER_0[i], OUTPUT);
    pinMode(SEG_CONTROLLER_1[i], OUTPUT);
  }
  // 割り込み設定
  attachInterrupt(digitalPinToInterrupt(2),StartPushed,RISING);
  // 子機の初期化待ち
  while(true)
  {
    // 本体LEDを点灯させる
    digitalWrite(13, HIGH);
    // 通信待ち
    while (Serial.available() == 0)
    {
      // delayを入れる
      delay(10);
    }
    // 受信処理
    uint8_t data = (uint8_t)Serial.read();
    // ヘッダー認識
    if (data == 0xFF) {
      // データ待機
      while (Serial.available() != 1) {
        // 1byte分のデータ受信を待機
      }
      // データ受信
      int arg = (int)Serial.read();
      // 命令の認識
      if (arg == NOTICE_FINISH_INIT) {
        // Ready状態へ
        break;
      }
    }
  }
  // Ready状態へ移行
  digitalWrite(13, LOW);
  _state = STATE_READY;
}

void loop() {
  // メインループ
  switch(_state)
  {
    case STATE_READY: // Ready状態
      // 待機アニメーション
      Ready();
      break;
    case STATE_GAME:
      // ゲーム中
      GameLoop();
      break;
    case STATE_RESULT:
      Result();
      break;
    case 4:
      //
      break;
  }
}

// スタートボタンのハンドラ
// ここでststeの変更処理などを実施
void StartPushed()
{
  static uint32_t lastInterruptTime = 0; // 前回の割り込み時刻を記憶
  uint32_t currentInterruptTime = millis();
  // チャタリングの防止
  if (currentInterruptTime - lastInterruptTime > S_INTERRUPTION_SENSE) { // 前回の割り込みから規定時間以上経過した場合
    // ボタン入力受付
    startButtonPressed = true; // ボタン状態を反転させる
  }
  lastInterruptTime = currentInterruptTime; // 前回の割り込み時刻を更新する
}

void Ready()
{
  // 待機時のアニメーション処理などを記述
  // 現状は、７セグを無限カウントアップ
  int d1 = 0;
  int d2 = 0;
  while(true)
  {
    _SegControl(0, d1);
    _SegControl(1, d2);
    delay(500);
    d1++;
    if(d1 == 10){
      d1 = 0;
      d2++;
    }
    if(d2 == 10){
      d2 = 0;
    }
    // ボタンインタラクト
    if(startButtonPressed)
    {
      // 状態リセット
      startButtonPressed = false;
      // 遷移
      ChangeState(STATE_GAME);
      break;
    }
  }
}

void GameLoop()
{
  switch(_gameState)
  {
    case GAME_STATE_COUNTDOWN: // カウントダウン
      // ゲーム初期化
      GameInit(); 
      // カウントダウン処理
      Wait(G_WAIT_TIME, true);
      // GAME_PLAYINGへ遷移
      ChangeGameState(GAME_PLAYING);
      break;
    case GAME_STATE_PLAYING: // ゲーム中
      if(_gameStartTime == 0)
      {
        // 開始時刻の保持
        _gameStartTime = millis();
      }
      // このフレームにおける基準時間の取得
      unsigned long origin = millis();// 評価基準になる時間の取得
      // 終了判定
      if(_gameStartTime + G_DEFAULT_TIME < origin)
      {
        // 終了フラグオン
        _gameEndFlag = true;
      }
      // ここで一旦子機と通信する（ゲームを続けるかどうかの情報を送る）
      if(_gameEndFlag)
      {
        // 終了
        _send(MEIREI_GAME_END);
        // 状態の遷移
        ChangeGameState(GAME_STATE_END);
      }else{
        // 続行
        _send(MEIREI_GAME_CONTINUE);
      }
      // 前フレームからのモグラの状態更新
      for(int i = 0; i < 5; i++)
      {
        if(pickedMogura[i] == 1) // 点灯中のモグラのみ
        {
          // 点灯時間の評価
          if(origin > moguraStateArr[i].startTime + moguraStateArr[i].distance)
          {
            // 消灯処理
            _unpick(i);
          }
        }
      }
      // 新規に点灯処理するモグラの選定
      // ゲーム性を決めるファクター
      // ・同時に点灯する数
      // ・点灯している（出現時間）時間

      // 新規のモグラピックアップは一定周期で実施
      // lastUpdateMogura
      //G_MOGURA_UPDATE_RATE
      if(origin > lastUpdateMogura + G_MOGURA_UPDATE_RATE)
      {
        Pick(origin);
        // モグラを更新した時間を保持しておく
        lastUpdateMogura = origin;
      }
      // センサ結果の取得
      int resultArr[5];
      while(true) // 子機からのデータ受信待ちループ
      {
        // 本体LEDを点灯させる
        digitalWrite(13, HIGH);
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
          while (Serial.available() < 5) {
            // 15byte分のデータ受信を待機
            // センサー５台分の判定結果
          }
          // データ受信
          for(int i = 0; i < 5; i++)
          {
            resultArr[i] = (int) Serial.read();
          }
          // 受信完了を通知する
          _send(MEIREI_VOID);
          break;
        }
      }
      // 判定と得点処理の実施
      for(int i = 0; i < 5; i++)
      {
        if(resultArr[i] == 1 && pickedMogura[i] == 1) // 押されている
        {
          // 得点の加算処理
          _score++;
          // 状態の更新
          _unpick(i);
        }
      }
      // 得点の表示処理
      ViewNumToSeg(_score);
      // LED点灯情報の送信
      Serial.write(255);
      for(int i = 0; i < 5; i++)
      {
        Serial.write(pickedMogura[i]);
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
          while (Serial.available() != 1) {
            // 1byte分のデータ受信を待機
          }
          // 破棄
          int _ = (int)Serial.read();
          // 全処理完了
          break;
        }
      }
      break;
    case GAME_STATE_FEAVOR: // フィーバー（実装しない可能性大）
      break;
    case GAME_STATE_END: // 終了
      // ゲーム終了フラグの初期化
      _gameEndFlag = false;
      // リザルトへ遷移
      ChangeState(STATE_RESULT);
      break;
  }
}

void Result()
{
  // リザルト処理
  // 得点の表示（本当はセグを点滅させたいところ）
  ViewNumToSeg(_score);
  // ボタンが押されるのを待機
  if(startButtonPressed)
  {
    // 状態リセット
    startButtonPressed = false;
    // 子機に通知
    _send(MEIREI_CHANGE_STATE_READY);
    // Readyに移動
    ChangeState(STATE_READY);
  }
}

// 指定時間待機
// ms:待機時間(ms) use7seg:７セグ表示をするかどうか
void Wait(int ms, bool use7seg)
{
  uint32_t startTime = millis();
  while(millis() - startTime < ms){
    int num = (millis() - startTime) / 1000;
    if(use7seg)
    {
      ViewNumToSeg(num);
    }
  };
  ViewNumToSeg(ms / 1000);
}

// 99以上を弾く処理未実装　時間あれば
void ViewNumToSeg(int num)
{
  if(num > 99)
  {
    _SegControl(0, 9);
    _SegControl(1, 9);
    return;
  }
  if(_nowSegNumber != num)
  {
    int d1 = num % 10;
    int d2 = num / 10;
    _SegControl(0, d1);
    _SegControl(1, d2);
    _nowSegNumber = num;
  }
}

void ChangeState(int state)
{
  _state = state;
}

void ChangeGameState(int state)
{
  _gameState = state;
}

void _SegControl(int id, int number)
{
  if(id == 0)
  {
    // セグ制御
    for(int i = 0;i < 4; i++){
      digitalWrite(SEG_CONTROLLER_0[i],number & (0x01 << i));
    }
  }else if(id == 1){
    // セグ制御
    for(int i = 0;i < 4; i++){
      digitalWrite(SEG_CONTROLLER_1[i],number & (0x01 << i));
    }
  }else{
    // Nothing To Do
  }
}

void GameInit()
{
  // 乱数初期化
  // 未使用のピンのノイズを使うべきとりあえずは0を入れておく
  randomSeed(analogRead(0));
  // 変数の初期化
  _gameStartTime = 0;
  _score = 0;
  // 子機に通知
  _send(MEIREI_CHANGE_STATE_GAME);
}

// モグラの更新（新規に点灯するモグラの選定と保持）
void Pick(uint32_t origin)
{
  // 点灯数のピックアップ
  if(_pickCount >= G_MAX_MOGURA_PICK) // 最大点灯数を超えている場合
  {
    // 抜ける
    return;
  }
  int pickAmount = random(1, G_MAX_MOGURA_PICK - _pickCount); // 1~5でピックアップ
  int index = 0; // INDEX保持
  for(int i = 0; i < pickAmount; i++)
  {
    for(int j = 0; j < 5; j++)
    {
      if(pickedMogura[j] == 0)
      {
        index = j;
        break;
      }
    }
    // 出現時間の選定
    int distance = random(G_DISTANCE_MIN, G_DISTANCE_MAX);
    // 点灯処理
    _pick(index, origin, distance);
  }
}

// 指定IDのモグラのピックアップ（点灯と判定有効化）処理をする関数
void _pick(int id, uint32_t s_time, uint32_t distance)
{
  //配列データ更新処理
  pickedMogura[id] = 1;
  moguraStateArr[id].startTime = s_time;
  moguraStateArr[id].distance = distance;
  //カウントアップ
  _pickCount++;
}

// 指定IDのモグラのピックアップ（点灯と判定有効化）を解消する関数
void _unpick(int id)
{
  //配列データ更新処理
  pickedMogura[id] = 0;
  moguraStateArr[id].startTime = 0;
  moguraStateArr[id].distance = 0;
  // カウントダウン
  _pickCount--;
}

// UART通信をする関数
void _send(uint8_t order)
{
  // ヘッダー送信
  Serial.write(255);
  // データ送信
  Serial.write(order);
}