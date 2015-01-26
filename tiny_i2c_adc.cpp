/* HARDWARE:
                          ATTiny85
                        +----------+
               RESET -- | 1      8 | -- VCC
                 PB3 -- | 2      7 | -- PB2/SCL \
  INPUT --> ADC2/PB4 -- | 3      6 | -- PB1     | I2C
                 GND -- | 4      5 | -- PB0/SDA /
                        +----------+

*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

// --- hardware connections ---

const uint8_t INPUT_ADC = 2;
const uint8_t ADC_PRESCALER = _BV(ADPS1) | _BV(ADPS0); // Set prescaler to 8 for 1MHz CPU = 125KHz

const uint8_t SCL_PIN = 2;
const uint8_t SDA_PIN = 0;

#define SCL (PORTB & _BV(SCL_PIN))
#define SDA (PORTB & _BV(SDA_PIN))

// --- i2c address ---

const uint8_t I2C_ADC_READ_ADDR = ('A' << 1) & 1;

// --- state ---

const uint8_t OVF_STATE_RECEIVING_ADDR = 1;
const uint8_t OVF_STATE_SENDING_ACK_ADDR = 2;
const uint8_t OVF_STATE_SENDING_BYTES = 3;
const uint8_t OVF_STATE_RECEIVING_ACK_BYTES = 4;

const uint8_t N_BYTES = 2;

volatile uint8_t ovfState;
volatile uint8_t byteIndex;
volatile uint8_t bytes[N_BYTES];

// --- code ---

inline void usiSleepMode() {
  MCUCR = _BV(SE) | _BV(SM1); // power-down
}

inline void adcSleepMode() {
  MCUCR = _BV(SE) | _BV(SM0); // ADC noise reduction
}

inline void usiWaitStart() {
  // input on SDA
  DDRB &= ~_BV(SDA_PIN);
  // Set USI in Two-wire waiting for START interrupt, hold SCL low on START, external clock source, 4-Bit Counter on both edges
  USICR = _BV(USISIE) | _BV(USIWM1) | _BV(USICS1);
  // Clear all interrupt flags and reset overflow counter
  USISR = _BV(USISIF) | _BV(USIOIF) | _BV(USIPF) | _BV(USIDC);
}

inline void usiReceiveAddr() {
  // input on SDA
  DDRB &= ~_BV(SDA_PIN);
  // Set USI in Two-wire waiting for START & OVF, hold SCL low on START & OVF, external clock source, 4-Bit Counter on both edges
  USICR = _BV(USISIE) | _BV(USIOIE) | _BV(USIWM1) | _BV(USIWM0) |_BV(USICS1);
  // Clear all interrupt flags and reset overflow counter
  USISR = _BV(USISIF) | _BV(USIOIF) | _BV(USIPF) | _BV(USIDC);
}

inline void usiSendAck() {
  // ack with zero bit
  USIDR = 0;
  // output on SDA
  DDRB |= _BV(SDA_PIN);
  // Clear overflow and set USI counter to shift 1 bit 
  USISR = _BV(USIOIF) | _BV(USIPF) | _BV(USIDC) | (0x0E << USICNT0);
}

inline void usiSendBytes() {
  // byte to send
  if (byteIndex < N_BYTES) 
    USIDR = bytes[byteIndex++];
  else
    USIDR = 0; // all other bytes are zero if master wants more
  // output on SDA
  DDRB |= _BV(SDA_PIN);
  // Set USI counter to shift all bits
  USISR = _BV(USIOIF) | _BV(USIPF) | _BV(USIDC);
}

inline void usiReceiveAck() {
  // clear data register
  USIDR = 0;
  // input on SDA
  DDRB &= ~_BV(SDA_PIN);
  // Clear overflow and set USI counter to shift 1 bit 
  USISR = _BV(USIOIF) | _BV(USIPF) | _BV(USIDC) | (0x0E << USICNT0);
}

// USI start condition interrupt vector
ISR(USI_START_vect) {
  // wait while start condition is in progress (SCL is high and SDA is low)
  while (SCL & !SDA);

  if (SDA) {
    // stop did occur, get back to waiting start condition
    usiWaitStart();
    return;
  }
  // stop did not occur, wait for address bytes
  usiReceiveAddr();
  ovfState = OVF_STATE_RECEIVING_ADDR;
}

// USI counter overflow interrupt vector
ISR(USI_OVF_vect) {
  if (USISR & _BV(USIPF)) {
    // stop did occur, get back to waiting start condition
    usiWaitStart();
    return;
  }
  // stop did not occur, check state
  switch (ovfState) {
    case OVF_STATE_RECEIVING_ADDR:
      // address received
      if (USIBR == I2C_ADC_READ_ADDR) {
         // our address received -- acknowledge
         usiSendAck();
         ovfState = OVF_STATE_SENDING_ACK_ADDR;
      } else {
         // something else -- wait start again
         usiWaitStart();
      }
      break;

    case OVF_STATE_SENDING_ACK_ADDR:
      // addr ack sent -- start ADC reading, 
      PRR &= ~_BV(PRADC); // power on ADC
      ADMUX = INPUT_ADC; // Use VCC as reference, single-ended on input adc
      ADCSRA = _BV(ADEN) | _BV(ADSC) | _BV(ADIF) | _BV(ADIE) | ADC_PRESCALER; // enable ADC & start conversion
      // Change sleep mode to to ADC Noise Reduction
      adcSleepMode();
      // don't reset flags -- keep SCL low while doing ADC (stretch clock)
      break;

    case OVF_STATE_SENDING_BYTES:
      usiReceiveAck();
      ovfState = OVF_STATE_RECEIVING_ACK_BYTES;
      break;

    case OVF_STATE_RECEIVING_ACK_BYTES:
      if (USIDR) {
         // NACK from master
         usiWaitStart();
      } else {
         // ACK from master
         usiSendBytes();
         ovfState = OVF_STATE_SENDING_BYTES;
      }
      break;

    default:
      // just in case of anything else -- go to wating for start again
      usiWaitStart();
  }
}

// ADC interrupt vector
ISR(ADC_vect) {
  bytes[0] = ADCH;
  bytes[1] = ADCL;
  ADCSRA = 0; // turn off ADC
  PRR |= _BV(PRADC); // power off ADC
  byteIndex = 0;
  usiSendBytes();
  ovfState = OVF_STATE_SENDING_BYTES;
  // deep sleep again
  usiSleepMode();
}

// --- setup and main loop ---

int main() {
  // Power configuration
  PRR = _BV(PRTIM0) | _BV(PRTIM1) | _BV(PRADC); // Turn off all timers (don't need them) and ADC until needed
  ACSR = _BV(ACD); // Disable analog comparator
  // Configure USI -- initialy wait for start condition
  usiWaitStart();
  // Set output to 1 on SCL ia intermediate pull-up enabled state (will not pull-up or write one because of TwoWire USI mode)
  PORTB |= SCL_PIN;
  DDRB |= SCL_PIN;
  // Enable sleep and configure "power down sleep" (USI can wake up)
  usiSleepMode();
  // Enable interrupts at last
  sei(); 
  // Sleep until interrupted
  while (1)
    sleep_cpu();
  return 0;
}
