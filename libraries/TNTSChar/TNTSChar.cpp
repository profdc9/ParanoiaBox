// FILE: TNTSChar_class.cpp
// Arduino STM32 用 NTSCビデオ出力ライブラリ by たま吉さん
// 作成日 2017/02/20, Blue Pillボード(STM32F103C8)にて動作確認
// 更新日 2017/02/27, delay_frame()の追加
// 更新日 2017/02/27, フック登録関数追加
// 更新日 2017/03/03, 解像度モード追加
// 更新日 2017/04/05, クロック48MHz対応
// 更新日 2017/04/18, SPI割り込みの廃止(動作確認中)
// 更新日 2017/04/27, NTSC走査線数補正関数追加
// 更新日 2017/04/30, SPI1,SPI2の選択指定を可能に修正
// 更新日 2017/06/25, 外部確保VRAMの指定を可能に修正

// 2020/03/05  Extensively modified by D. Marks to be a character buffer

#include <TNTSChar.h>
#include <TNTSCharfont.h>
#include <SPI.h>

#define gpio_write(pin,val) gpio_write_bit(PIN_MAP[pin].gpio_device, PIN_MAP[pin].gpio_bit, val)

static inline uint32_t gpio_get_bsrr_bitmask(uint8 pin, uint8 val)
{
	return (1U << PIN_MAP[pin].gpio_bit) << (16 * (!val));
}

static inline volatile uint32_t *gpio_get_bsrr_address(uint8 pin)
{
	return &(PIN_MAP[pin].gpio_device)->regs->BSRR;
}

static inline volatile uint32_t *gpio_get_odr_address(uint8 pin)
{
	return &(PIN_MAP[pin].gpio_device)->regs->ODR;
}

#define PWM_CLK PA1         // 同期信号出力ピン(PWM)
#define DAT PA7             // 映像信号出力ピン
#define NTSC_S_TOP 3        // 垂直同期開始ライン
#define NTSC_S_END 5        // 垂直同期終了ライン
#define NTSC_VTOP 30        // 映像表示開始ライン
#define IRQ_PRIORITY  2     // タイマー割り込み優先度
#define MYSPI1_DMA_CH DMA_CH3 // SPI1用DMAチャンネル
#define MYSPI2_DMA_CH DMA_CH5 // SPI2用DMAチャンネル
#define MYSPI_DMA DMA1        // SPI用DMA

// 画面解像度別パラメタ設定
typedef struct  {
  uint16_t width;   // 画面横ドット数
  uint16_t height;  // 画面縦ドット数
  uint16_t ntscH;   // NTSC画面縦ドット数
  uint16_t hsize;   // 横バイト数
  uint16_t rows;   // 横バイト数
  uint16_t cols;   // 横バイト数
  uint32_t spiDiv;  // SPIクロック分周
} SCREEN_SETUP;

#define NTSC_TIMER_DIV 1 // システムクロック分周 1/3
const SCREEN_SETUP screen_type[] __FLASH__ {
  { 224, 216, 216, 28, 24, 20, SPI_CLOCK_DIV16 }, // 224x216
  { 448, 216, 216, 56, 24, 40, SPI_CLOCK_DIV8  }, // 448x216 
  { 896, 216, 216, 112, 24, 80, SPI_CLOCK_DIV4  }, // 896x216 
};

#define NTSC_LINE (262+0)                     // 画面構成走査線数(一部のモニタ対応用に2本に追加)
#define SYNC(V)  gpio_write(PWM_CLK,V)        // 同期信号出力(PWM)
static volatile uint8_t* ptr;                 // ビデオ表示フレームバッファ参照用ポインタ
static volatile int count=1;                  // 走査線を数える変数

static uint8_t *_line1;
static uint8_t *_line2;
static uint8_t *_screendata;

static uint8_t  _screen;
static uint16_t _width;
static uint16_t _height;
static uint16_t _hsize;
static uint16_t _padding;
static uint16_t _rows;
static uint16_t _cols;
static uint16_t _xpos;
static uint16_t _ypos;
static volatile uint16_t _framect;
static uint16_t _buflen;
static uint16_t _ntscHeight;
static uint16_t _ntsc_line = NTSC_LINE;
static uint16_t _hAdjust = 0; // 横表示位置補正
static uint16_t _vAdjust = 0; // 縦表示位置補正
static uint8_t  _spino = 1;
static uint16_t _first_active_line;
static uint16_t _last_active_line;

static dma_channel  _spi_dma_ch = MYSPI1_DMA_CH;
static dma_dev* _spi_dma    = MYSPI_DMA;
static SPIClass* pSPI;
static uint32_t _dma_on;

uint16_t TNTSChar_class::width()  {return _width;;} ;
uint16_t TNTSChar_class::height() {return _height;} ;
uint16_t TNTSChar_class::screen() { return _screen;};
static dma_channel_reg_map *_chan_regs;
static volatile void *_chan_regs_CCR;
static volatile uint32_t *_CCR_bb;

// DMAを使ったデータ出力
void SPI_dmaSend(void) {
  dma_setup_transfer( 
    _spi_dma, _spi_dma_ch,  // SPI1用DMAチャンネル指定
    &pSPI->dev()->regs->DR, // 転送先アドレス    ：SPIデータレジスタを指定
    DMA_SIZE_8BITS,         // 転送先データサイズ : 1バイト
    (void *)ptr,            // 転送元アドレス     : SRAMアドレス
    DMA_SIZE_8BITS,         // 転送先データサイズ : 1バイト
    DMA_MINC_MODE|          // フラグ: サイクリック
    DMA_FROM_MEM |          //         メモリから周辺機器、転送完了割り込み呼び出しあり 
    DMA_TRNS_CMPLT          //         転送完了割り込み呼び出しあり  */
  );
  dma_set_num_transfers(_spi_dma, _spi_dma_ch, _hsize); // 転送サイズ指定
//  dma_enable(_spi_dma, _spi_dma_ch);  // DMA有効化
}

// DMA用割り込みハンドラ(データ出力をクリア)
void DMA1_CH3_handle() {
  while(pSPI->dev()->regs->SR & SPI_SR_BSY);
    pSPI->dev()->regs->DR = 0; 
  if (_dma_on)
	SPI_dmaSend();
}

void prepare_line(volatile uint8_t *buf)
{
	uint16_t scan_line = count - _first_active_line;
	uint16_t subscan = scan_line & 0x07;
	scan_line /= 8;
	buf += _padding;
	if (scan_line >= _rows)
	{
	   for (int i=0;i<_cols;i++) buf[i] = 0;
	   return;
	}
	uint8_t *scan_line_begin = _screendata + (scan_line * _cols);
	for (int i=0;i<_cols;i++) 
	{
		uint8_t c = scan_line_begin[i];
		buf[i] = raster88_font[c & 0x7F][subscan] ^ ((c & 0x80) ? 0xFF : 0) ;
    }
	if ((scan_line == _ypos) && (subscan>5) && ((_framect & 0x20) != 0) && (_xpos < _cols)) buf[_xpos] ^= 0xFF;
}

// ビデオ用データ表示(ラスタ出力）
void handle_vout()
{
  *_CCR_bb = _dma_on;
  if (_dma_on)
  {
	ptr = (ptr == _line1) ? _line2 : _line1;
	prepare_line(ptr);
  }
  // 次の走査線用同期パルス幅設定
  if(count >= NTSC_S_TOP-1 && count <= NTSC_S_END-1){
    // 垂直同期パルス(PWMパルス幅変更)
    TIMER2->regs.adv->CCR2 = 4236;
  } else {
    // 水平同期パルス(PWMパルス幅変更)
    TIMER2->regs.adv->CCR2 = 336;
  } 
  count++; 
  if( count > _ntsc_line ){
    count=1;
    ptr = _line1;    
  } 
  if (count == _first_active_line)
  {
	   _framect++;
	   prepare_line(ptr);
       SPI_dmaSend();
	   _dma_on = 1;
  } else if (count > _first_active_line && count <= _last_active_line) 
  {
	  _dma_on = 1;
  } else _dma_on = 0;
}

void TNTSChar_class::recalculate_internal()
{
	_first_active_line = NTSC_VTOP+_vAdjust;
	_last_active_line = NTSC_VTOP+_vAdjust+_ntscHeight-1;
}

void TNTSChar_class::adjust(int16_t cnt, int16_t hcnt, int16_t vcnt) {
  _ntsc_line = NTSC_LINE+cnt;
  _hAdjust = hcnt;
  _vAdjust = vcnt;
  recalculate_internal();
}

uint16_t TNTSChar_class::rows()
{
	return _rows;
}

uint16_t TNTSChar_class::cols()
{
	return _cols;
}

uint16_t TNTSChar_class::framect()
{
	return _framect;
}

uint8_t *TNTSChar_class::screendata()
{
	return _screendata;
}

void TNTSChar_class::get_cursor_ptr(volatile uint16_t **xpos, volatile uint16_t **ypos)
{
	*xpos = &_xpos;
	*ypos = &_ypos;
}

// NTSCビデオ表示開始
void TNTSChar_class::begin(uint8_t mode, uint8_t spino) {   
   // スクリーン設定	
   _screen = mode;
   _width  = screen_type[_screen].width;
   _height = screen_type[_screen].height;   
   _ntscHeight = screen_type[_screen].ntscH;
   _hsize = screen_type[_screen].hsize;   
   _cols = screen_type[_screen].cols;
   _rows = screen_type[_screen].rows;
   _padding = _hsize / 5;
   _line1 = (uint8_t *)malloc(_hsize*2);
   _line2 = _line1 + _hsize;
   _buflen = _rows*_cols;
   _screendata = (uint8_t *)malloc(_buflen);
   _spino = spino;
   _dma_on = 0;
   _xpos = _ypos = _framect = 0;
   recalculate_internal();
  
   ptr = _line1;  // ビデオ表示用フレームバッファ参照ポインタ
   count = 1;

  // SPIの初期化・設定
  if (spino == 2) {
    pSPI = new SPIClass(2);
    _spi_dma    = MYSPI_DMA;
    _spi_dma_ch = MYSPI2_DMA_CH;
  } else {
    pSPI = &SPI;
    _spi_dma    = MYSPI_DMA;
    _spi_dma_ch = MYSPI1_DMA_CH;
  };
  pSPI->begin(); 
  pSPI->setBitOrder(MSBFIRST);  // データ並びは上位ビットが先頭
  pSPI->setDataMode(SPI_MODE3); // MODE3(MODE1でも可)
	if (_spino == 2) {
      pSPI->setClockDivider(screen_type[_screen].spiDiv-1); // クロックをシステムクロック36MHzの1/8に設定
	} else {
      pSPI->setClockDivider(screen_type[_screen].spiDiv);    // クロックをシステムクロック72MHzの1/16に設定
	}
	pSPI->dev()->regs->CR1 |=SPI_CR1_BIDIMODE_1_LINE|SPI_CR1_BIDIOE; // 送信のみ利用の設定

  // SPIデータ転送用DMA設定
  dma_init(_spi_dma);
  dma_attach_interrupt(_spi_dma, _spi_dma_ch, &DMA1_CH3_handle);
  spi_tx_dma_enable(pSPI->dev());  
  _chan_regs = dma_channel_regs(_spi_dma, _spi_dma_ch);
  _chan_regs_CCR = &_chan_regs->CCR;
  _CCR_bb = __bb_addr(_chan_regs_CCR, DMA_CCR_EN_BIT, BB_PERI_BASE, BB_PERI_REF);
  
  /// タイマ2の初期設定
  nvic_irq_set_priority(NVIC_TIMER2, IRQ_PRIORITY); // 割り込み優先レベル設定
  Timer2.pause();                             // タイマー停止
  Timer2.setPrescaleFactor(NTSC_TIMER_DIV);   // システムクロック 72MHzを24MHzに分周 
  Timer2.setOverflow(4572);                   // カウンタ値1524でオーバーフロー発生 63.5us周期

  // +4.7us 水平同期信号出力設定
  pinMode(PWM_CLK,PWM);          // 同期信号出力ピン(PWM)
  timer_cc_set_pol(TIMER2,2,1);  // 出力をアクティブLOWに設定
  pwmWrite(PWM_CLK, 336);        // パルス幅を4.7usに設定(仮設定)
  
  // +9.4us 映像出力用 割り込みハンドラ登録
  Timer2.setCompare(1, 336);   // オーバーヘッド分等の差し引き
  Timer2.setMode(1,TIMER_OUTPUTCOMPARE);
  Timer2.attachInterrupt(1, handle_vout);   

  Timer2.setCount(0);
  Timer2.refresh();       // タイマーの更新
  Timer2.resume();        // タイマースタート  
  for (int i=0;i<_hsize;i++)
    _line1[i] = _line2[i] = 0;
}

// NTSCビデオ表示終了
void TNTSChar_class::end() {
  Timer2.pause();
  Timer2.detachInterrupt(1);
  spi_tx_dma_disable(pSPI->dev());  
  dma_detach_interrupt(_spi_dma, _spi_dma_ch);
  pSPI->end();
  free(_line1);
  free(_screendata);
  if (_spino == 2) {
  	delete pSPI;
  	//pSPI->~SPIClass();
  }	
}

// 画面クリア
// フレーム間待ち
void TNTSChar_class::delay_frame(uint16_t x) {
  while (x) {
    while (count != _ntscHeight + NTSC_VTOP);
    while (count == _ntscHeight + NTSC_VTOP);
    x--;
  }
}
	
TNTSChar_class TNTSChar;

