// FILE: TNTSC.h
// Arduino STM32 用 NTSCビデオ出力ライブラリ by たま吉さん
// 作成日 2017/02/20, Blue Pillボード(STM32F103C8)にて動作確認
// 更新日 2017/02/27, delay_frame()の追加、
// 更新日 2017/02/27, フック登録関数追加
// 更新日 2017/03/03, 解像度モード追加
// 更新日 2017/04/05, クロック48MHz対応
// 更新日 2017/04/27, NTSC走査線数補正関数adjust()追加
// 更新日 2017/04/30, SPI1,SPI2の選択指定を可能に修正
// 更新日 2017/06/25, 外部確保VRAMの指定を可能に修正
// 更新日 2018/08/05, 水平・垂直表示位置補正の追加
//

#ifndef __TNTSCHAR_H__
#define __TNTSCHAR_H__

#include <Arduino.h>

	#define SC_224x216  0 // 224x216
	#define SC_448x216  1 // 448x216
	#define SC_896x216  2 // 448x216
    #define SC_DEFAULT  SC_896x216

// ntscビデオ表示クラス定義
class TNTSChar_class {    
  
  public:
    void begin(uint8_t mode=SC_DEFAULT,uint8_t spino = 1);  // NTSCビデオ表示開始
    void end();                            // NTSCビデオ表示終了 
    void delay_frame(uint16_t x);          // フレーム換算時間待ち
    uint16_t width() ;
    uint16_t height() ;
    uint16_t rows() ;
    uint16_t cols() ;
    uint16_t framect() ;
	uint8_t *screendata();
    void get_cursor_ptr(volatile uint16_t **xpos, volatile uint16_t **ypos);
    uint16_t screen();
	void adjust(int16_t cnt, int16_t hcnt=0, int16_t vcnt=0); 
  private:
    void recalculate_internal();
};

extern TNTSChar_class TNTSChar; // グローバルオブジェクト利用宣言

#endif
