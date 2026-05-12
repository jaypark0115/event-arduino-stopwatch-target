#include <SPI.h>

// =========================
// 사용자 설정
// =========================
// 여기 입력하는 값은 "숫자가 바뀌는 속도"
// 예: 1000 넣으면
// 실제 타이머는 2배(2000 Hz)로 동작하면서
// [표시]-[끔]-[다음표시]-[끔] 반복
static const uint16_t UPDATE_HZ = 1000;

// =========================
// 핀 설정
// =========================
static const uint8_t PIN_LOAD = 10;

// common-anode용 7세그 비트맵
// bit0=a, bit1=b, ..., bit6=g, bit7=dp
static const uint8_t DIGIT_CC[10] = {
  0b00111111, // 0
  0b00000110, // 1
  0b01011011, // 2
  0b01001111, // 3
  0b01100110, // 4
  0b01101101, // 5
  0b01111101, // 6
  0b00000111, // 7
  0b01111111, // 8
  0b01101111  // 9
};

volatile bool tickFlag = false;
volatile uint16_t counterValue = 0;
volatile bool showPhase = true;   // true: 숫자 표시, false: 전체 끔

ISR(TIMER1_COMPA_vect) {
  tickFlag = true;
}

static inline uint8_t encodeDigitCA(uint8_t d, bool dp = false) {
  uint8_t seg = DIGIT_CC[d % 10];
  if (dp) seg |= 0b10000000;
  return ~seg;   // common anode: LOW가 켜짐
}

// common-anode에서 전체 끔 = 모든 세그먼트 HIGH
static inline void displayBlank() {
  send4Bytes(0xFF, 0xFF, 0xFF, 0xFF);
}

static inline void send4Bytes(uint8_t b3, uint8_t b2, uint8_t b1, uint8_t b0) {
  digitalWrite(PIN_LOAD, LOW);
  SPI.transfer(b3);
  SPI.transfer(b2);
  SPI.transfer(b1);
  SPI.transfer(b0);
  digitalWrite(PIN_LOAD, HIGH);   // 이 순간 4자리 동시 변경
}

static inline void displayNumber(uint16_t value) {
  uint8_t d0 = value % 10;
  uint8_t d1 = (value / 10) % 10;
  uint8_t d2 = (value / 100) % 10;
  uint8_t d3 = (value / 1000) % 10;

  uint8_t b3 = encodeDigitCA(d0, false);
  uint8_t b2 = encodeDigitCA(d1, false);
  uint8_t b1 = encodeDigitCA(d2, false);
  uint8_t b0 = encodeDigitCA(d3, false);

  send4Bytes(b3, b2, b1, b0);
}

void setupTimer1(uint16_t hz) {
  noInterrupts();

  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;

  // UNO 16MHz / 64 = 250000 Hz
  // OCR1A = 250000 / hz - 1
  uint32_t compare = 250000UL / hz - 1UL;
  OCR1A = (uint16_t)compare;

  TCCR1B |= (1 << WGM12);              // CTC mode
  TCCR1B |= (1 << CS11) | (1 << CS10); // prescaler 64
  TIMSK1 |= (1 << OCIE1A);             // interrupt enable

  interrupts();
}

void setup() {
  pinMode(PIN_LOAD, OUTPUT);
  digitalWrite(PIN_LOAD, HIGH);

  SPI.begin();
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));

  displayBlank();

  // 실제 타이머는 2배로 동작
  setupTimer1(UPDATE_HZ * 2);
}

void loop() {
  if (tickFlag) {
    noInterrupts();
    tickFlag = false;
    interrupts();

    if (showPhase) {
      // 숫자 표시
      displayNumber(counterValue);

      // 다음 표시 숫자 준비
      counterValue++;
      if (counterValue > 9999) {
        counterValue = 0;
      }
    } else {
      // 전체 끔
      displayBlank();
    }

    showPhase = !showPhase;
  }
}