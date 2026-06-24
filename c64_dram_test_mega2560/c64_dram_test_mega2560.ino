#include <stdint.h>

/*
 64K x 4 bit DRAM test for: Nec D41464C, TMS4464, etc.

 Wiring diagram:
 Robotdyn Mega Pro CH340G /Atmega2560 (Arduino MEGA compatible)

                   +---------------
                   | VIN[ ][ ]VIN  \                  
                   | GND[ ][ ]GND  /                  
                   |  5V[ ][ ]5V   \                  
                   | 3V3[ ][ ]3V3  /                  
                   | RST[ ][ ]AREF \                  
               E1  |  TX[ ][ ]RX   /  E0
               E5  |  D3[ ][ ]D2   \  E4
               E3  |  D5[ ][ ]D4   /  G5
               H4  |  D7[ ][ ]D6   \  H3
               H6  |  D9[ ][ ]D8   /  H5
               B5  | D11[ ][ ]D10  \  B4
               B7  | D13[ ][ ]D12  /  B6
               J0  | D15[ ][ ]D14  \  J1
               H0  | D17[ ][ ]D16  /  H1              
               D2  | D19[ ][ ]D18  \  D3              
               D0  | D21[ ][ ]D20  /  D1              
    RAM A1 --- A1  | D23[ ][ ]D22  \  A0 --- RAM A0
    RAM A3 --- A3  | D25[ ][ ]D24  /  A2 --- RAM A2
    RAM A5 --- A5  | D27[ ][ ]D26  \  A4 --- RAM A4
    RAM A7 --- A7  | D29[ ][ ]D28  /  A6 --- RAM A6
               C6  | D31[ ][ ]D30  \  C7
                   +----------------

                    --+-----+-------+
                    \ |     |       |
                    / | USB | [RST] |
                    \ +-----+       |
                    /               |
                    \               |
    RAM IO2 --- F1  /   A1[ ][ ]A0  |  F0 --- RAM IO1
    RAM IO4 --- F3  \   A3[ ][ ]A2  |  F2 --- RAM IO3
                F5  /   A5[ ][ ]A4  |  F4
                F7  \   A7[ ][ ]A6  |  F6
                K1  /   A9[ ][ ]A8  |  K0
                K3  \  A11[ ][ ]A10 |  K2
                K5  /  A13[ ][ ]A12 |  K4
                K7  \  A15[ ][ ]A14 |  K6
                C4  /  D33[ ][ ]D32 |  C5
   RAM /RAS --- C2  \  D35[ ][ ]D34 |  C3 --- RAM /CAS
    RAM /OE --- C0  /  D37[ ][ ]D36 |  C1 --- RAM /WE
                G2  \  D39[ ][ ]D38 |  D7
                G0  /  D41[ ][ ]D40 |  G1
                L6  \  D43[ ][ ]D42 |  L7
                L4  /  D45[ ][ ]D44 |  L5
                L2  \  D47[ ][ ]D46 |  L3
                     ---------------+


  DRAM chip pinout (NEC 41464, TMS 4464, etc.):

                     +-----\/-----+
                   1 | /OE    GND | 18
                   2 | I/O1  I/O4 | 17
                   3 | I/O2  /CAS | 16
                   4 | /WE   I/O3 | 15
                   5 | /RAS    A0 | 14
                   6 | A6      A1 | 13
                   7 | A5      A2 | 12
                   8 | A4      A3 | 11
                   9 | VCC     A7 | 10
                     +------------+
  DRAM power pins:

  - pin 9  (Vcc) -> +5v
  - pin 18 (GND) -> GND

  How to diagnose:

  1. Test electrical condition of the chip **FIRST**:

  - Check for shorted Vcc
  - Inputs Ax, /OE, /WE, /RAS, /CAS must be high-Z

  2. Run this test and check serial output (115200 bps) for PASS/FAIL test cases.
     Output of the test in progress may look like the following contrived example
     made by messing with interconnect wires while the rand test was happening:

      1111... PASS
      0000... PASS
      0101... PASS
      1010... PASS
      rand seed: B0490D30
      rand... PASS
      Test loops: 311

      1111... PASS
      0000... PASS
      0101... PASS
      1010... PASS
      rand seed: BC23EA0F
      rand... Error at row 144 column 61 Expected: B Read: 1
      FAIL!!!

     If not connected via serial, watch built in LED for steady on condition.  If
     an error occurs, it will blink "SOS" in Morse code.  Three short, three long,
     and three short blinks, repeatedly.
 */

#define nOE  0
#define nWE  1
#define nRAS 2
#define nCAS 3

#define SETB(port, b)  port |= _BV(b)
#define CLRB(port, b)  port &= ~_BV(b)
#define ISSET(port, b) ((port & _BV(b)) != 0)

uint32_t prng_state = 0xdecafbad;

/* When DO_TESTING is defined a timing check of generating a rows 
 * worth of random data and writing it to a row is performed and
 * then a timing of writing the constant values is done.  Those
 * are displayed at the beginning of the test loop.
 * 
 * Sample output, numbers are microseconds and can/will vary.
 *
 * Time to generate and fill a row: 1060
 * Time to fill with a constant   : 732
 *
 */
//define DO_TESTING

/* The inline assembly version is about 3.5 times faster than
 * the pure C version.  The pure C works fine, but does take up
 * some valuable time.  Results of the two were compared with
 * the same starting seed and 10000 iterations.
 */
#define USE_INLINE_ASM_PRNG

#define LED_OFF (PORTB &= 0b01111111)
#define LED_ON  (PORTB |= 0b10000000) 

/*
 * A union to hold the pre-computed PRNG data for the row being tested.
 * An optimized inline assembly xorshift32 prng() is now used which can
 * generate a whole row of 256 nybbles (32 qwords, 128 bytes) in about
 * 216 microseconds.  This is fast enough to generate a row of random
 * data and write that row to ram for all 256 columns at once, which
 * takes about 1100 microseconds per row.
 */
#define PRNG_DATA_SIZE 32
typedef union {
    uint32_t u32[PRNG_DATA_SIZE]; 
    uint8_t  b[PRNG_DATA_SIZE*4]; // 128 bytes, used as 256 nybbles
} prng_bytes;
prng_bytes prng_data;

/* Not sure what's wrong with Arduino but you MUST split the return type 
 * and function name in separate lines or you'll get weird compiler errors
 * on perfectly okay C code.
 */

/* 23 JUN 2026: [PWC] - The problem is apparently a bug in the IDE when 
 * it tries to generate function prototypes for you.  A workaround is to
 * provide a function prototype for the first function in your code.
 */
inline void w(uint8_t row, uint8_t col, uint8_t v);

inline void w(uint8_t row, uint8_t col, uint8_t v) 
{
    cli();                                            // disable interrupts where needed
    PORTF = (PORTF & 0b11110000) | (v & 0b00001111);  // set i/o data
    PORTA = row;                                      // set row
    CLRB(PORTC, nRAS);                                // RAS down
    PORTA = col;                                      // hold nWE on
    CLRB(PORTC, nCAS);                                // CAS down
    asm("nop");                                       // > th(CLD) = 45 ns (TMS4464)
    CLRB(PORTC, nWE);                                 // WE down
    asm("nop");
    SETB(PORTC, nWE);                                 // WE up
    SETB(PORTC, nRAS);                                // RAS up
    SETB(PORTC, nCAS);                                // CAS up
    sei();
}

inline uint8_t r(uint8_t row, uint8_t col)
{
    // read
    cli();                                            // disable interrupts where needed
    PORTA = row;                                      // set row
    CLRB(PORTC, nRAS);                                // RAS down
    asm("nop");                                       // hold >15n (TMS4464 -10)
    PORTA = col;                                      // set col
    CLRB(PORTC, nCAS);                                // CAS down
    CLRB(PORTC, nOE);                                 // OE down
    asm("nop");   
    uint8_t v = PINF & 0b00001111;                    // do READ
    SETB(PORTC, nOE);                                 // OE up
    SETB(PORTC, nRAS);                                // RAS up
    SETB(PORTC, nCAS);                                // CAS up
    sei();
    return v;
}

#ifdef USE_INLINE_ASM_PRNG

inline uint32_t prng(uint32_t x) 
{
    uint32_t temp = prng_state;
    asm volatile(
        // ==========================================
        // 1. p ^= p << 13
        // ==========================================
        "movw %A1, %A0    \n\t" 
        "movw %C1, %C0    \n\t" 
        // Byte shift left by 8 bits
        "mov %D1, %C1     \n\t"
        "mov %C1, %B1     \n\t"
        "mov %B1, %A1     \n\t"
        "clr %A1          \n\t"
        // Unrolled 5-bit left shift (No counter loop!)
        "lsl %A1 \n\t rol %B1 \n\t rol %C1 \n\t rol %D1 \n\t"
        "lsl %A1 \n\t rol %B1 \n\t rol %C1 \n\t rol %D1 \n\t"
        "lsl %A1 \n\t rol %B1 \n\t rol %C1 \n\t rol %D1 \n\t"
        "lsl %A1 \n\t rol %B1 \n\t rol %C1 \n\t rol %D1 \n\t"
        "lsl %A1 \n\t rol %B1 \n\t rol %C1 \n\t rol %D1 \n\t"
        // XOR result
        "eor %A0, %A1     \n\t"
        "eor %B0, %B1     \n\t"
        "eor %C0, %C1     \n\t"
        "eor %D0, %D1     \n\t"

        // ==========================================
        // 2. p ^= p >> 17
        // ==========================================
        "movw %A1, %A0    \n\t" 
        "movw %C1, %C0    \n\t"
        // Byte shift right by 16 bits
        "mov %A1, %C1     \n\t"
        "mov %B1, %D1     \n\t"
        "clr %C1          \n\t"
        "clr %D1          \n\t"
        // 1-bit right shift
        "lsr %B1          \n\t"
        "ror %A1          \n\t"
        // XOR result
        "eor %A0, %A1     \n\t"
        "eor %B0, %B1     \n\t"
        "eor %C0, %C1     \n\t"
        "eor %D0, %D1     \n\t"

        // ==========================================
        // 3. p ^= p << 5
        // ==========================================
        "movw %A1, %A0    \n\t" 
        "movw %C1, %C0    \n\t"
        // Unrolled 5-bit left shift
        "lsl %A1 \n\t rol %B1 \n\t rol %C1 \n\t rol %D1 \n\t"
        "lsl %A1 \n\t rol %B1 \n\t rol %C1 \n\t rol %D1 \n\t"
        "lsl %A1 \n\t rol %B1 \n\t rol %C1 \n\t rol %D1 \n\t"
        "lsl %A1 \n\t rol %B1 \n\t rol %C1 \n\t rol %D1 \n\t"
        "lsl %A1 \n\t rol %B1 \n\t rol %C1 \n\t rol %D1 \n\t"
        // XOR result
        "eor %A0, %A1     \n\t"
        "eor %B0, %B1     \n\t"
        "eor %C0, %C1     \n\t"
        "eor %D0, %D1     \n\t"

        : "+r" (x), "=&r" (temp) 
        :                        
        : // No clobbers needed anymore! r26 is free.
    );
    prng_state = x;
    return x;
}
#else
// modified to match function signature of asm version
inline uint32_t prng(uint32_t seed)
{
  uint32_t x = seed;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  prng_state = x;
  return x;
}
#endif

void fill_prng_data()
{
  for (int i = 0; i < PRNG_DATA_SIZE; i++) {
    prng_data.u32[i] = prng(prng_state);
  }   
}

void setup() 
{
  Serial.begin(115200);

  // Pin functions:
  
  // PA0..7 = address generator (RAM A0..7/8..15 multiplexed)
  // PC0..4 = RAM control
  // PF0..7 = I/O1..4
  // PB7    = Built-in LED
  
  // Pin direction: 0 = in/pullup, 1 = out
  DDRA     = 0b11111111;     DDRC     = 0b00001111;     DDRF    = 0b00000000;    
  // PA7 (D29) 1------- A7
  // PA6 (D28) -1------ A6
  // PA5 (D27) --1----- A5
  // PA4 (D26) ---1---- A4
  // PA3 (D25) ----1--- A3   // PC3 (D34) ----1--- /CAS // PF3 (A3) ----0--- I/O4
  // PA2 (D24) -----1-- A2   // PC2 (D35) -----1-- /RAS // PF2 (A2) -----0-- I/O3
  // PA1 (D23) ------1- A1   // PC1 (D36) ------1- /WE  // PF1 (A1) ------0- I/O2
  // PA0 (D22) -------1 A0   // PC0 (D37) -------1 /OE  // PF0 (A0) -------0 I/O1

  // if DDRD == 0 (in)  -> 0 = no pull-up, 1 = pull-up
  // if DDRD == 1 (out) -> 0 = low output, 1 = high output
  PORTA    = 0b00000000;     PORTC    = 0b00001111;     PORTF   = 0b00000000;
  // PA7 (D29) 0------- A7
  // PA6 (D28) -0------ A6
  // PA5 (D27) --0----- A5
  // PA4 (D26) ---0---- A4
  // PA3 (D25) ----0--- A3   // PC3 (D34) ----1--- A11  // PF3 (A3) ----0--- D3
  // PA2 (D24) -----0-- A2   // PC2 (D35) -----1-- A10  // PF2 (A2) -----0-- D2
  // PA1 (D23) ------0- A1   // PC1 (D36) ------1- A9   // PF1 (A1) ------0- D1
  // PA0 (D22) -------0 A0   // PC0 (D37) -------1 A8   // PF0 (A0) -------0 D0

  // Set up to write to LED and then turn it on
  DDRB     |= 0b10000000;  // Set LED pin (D13) as an output
  LED_ON                ;  // Say "we're testing"

  fill_prng_data();  // Fill data one time to start
}

int show_error_data(uint8_t e_row, uint8_t e_col, uint8_t e_rng, uint8_t e_ram)
{
  Serial.print("Error at row "); Serial.print(e_row);
  Serial.print(" column "); Serial.print(e_col);
  Serial.print(" Expected: "); Serial.print(e_rng, HEX);
  Serial.print(" Read: "); Serial.println(e_ram, HEX);
  return 0;
}

void led_blink(int len)
{
  // send an "S" or an "O"
  for (int i = 0; i < 3; i++)
  {
    LED_ON;
    delay(len);
    LED_OFF;
    delay(200);          // inter-(dot|dash) delay
  }
  delay(300);            // inter-char delay
}

void flash_led()
{
  LED_OFF;     // Turn LED off
  delay(2000); //   for just a moment

  // Send out an SOS forever
  while (1)
  {
    led_blink(200); // S
    led_blink(500); // O
    led_blink(200); // S
    delay(1500);
  }
}

void test_and_print(const char *title, int (*test_func)()) 
{
  Serial.print(title);
  Serial.print("... ");
  Serial.flush();

  if ((*test_func)()) {
    Serial.println("PASS");
  } else {
    Serial.println("FAIL!!!");
    Serial.flush(); // to be sure
    flash_led();
  }
}

int test(int v) 
{
  uint8_t col = 0, row = 0, x = 0;
  v &= 0b1111;

  DDRF |= 0b00001111;   // Set I/Ox as OUTPUT, moved out of w()
  // Write v
  do {
    do {
      w(row, col, v);
      row++;    
    } while (row);
    col++;
  } while (col);

  DDRF &= 0b11110000;   // Set I/Ox as INPUT again
  PORTF |= 0b00001111;  // keep pull-ups
  // Verify
  do {
    do {
      if (x = r(row, col) != v) 
        return show_error_data(row, col, v, x);
      row++;
    } while (row);
    col++;
  } while (col);

  return 1;
}

int test_1() {
  return test(0b1111);
}

int test_0() {
  return test(0b0000);
}

int test_01() {
  return test(0b0101);
}

int test_10() {
  return test(0b1010);
}

// Break out row write to be usable in test_rand() and test_speed().  We are using
// a data block with 256 nybbles.
inline void rand_row_write(uint8_t col) 
{
  uint8_t row = 0, pos = 0;
  do {
    w(row, col, prng_data.b[pos]); // w() does masking
    row++;
    w(row, col, prng_data.b[pos]>>4);
    row++;
    pos++;
  } while (row);
}

// break out row read to work in test_rand() and test_speed().
inline int rand_row_verify(uint8_t col)
{
  uint8_t row = 0, pos = 0;
  uint8_t v, x;
  do {
    v = prng_data.b[pos];
    if (x = r(row, col) != (v & 0xf))
      return show_error_data (row, col, v & 0xf, x);
    row++;
    if (x = r(row, col) != (v >> 4))
      return show_error_data (row, col, v >> 4, x);
    row++;
    pos++;
  } while (row);
  return 1;
}

#ifdef DO_TESTING
void test_speed() {
  uint8_t col = 0, row = 0, v = 0;
  uint32_t start_time, end_time;

#if 0
  // Time the prng fill routine
  start_time = micros();
  fill_prng_data();
  end_time = micros();
  Serial.print("Time to generate random row buffer: ");
  Serial.println(end_time - start_time);

  // Time writing the row data in nybbles
  DDRF |= 0b00001111;   // Set I/Ox as OUTPUT, moved out of w()
  start_time = micros();
  rand_row_write(col);
  end_time = micros();
  Serial.print("Time to fill a row: ");
  Serial.println(end_time - start_time);

  DDRF &= 0b11110000;   // Set I/Ox as INPUT again
  PORTF |= 0b00001111;  // keep pull-ups

  start_time = micros();
  if (rand_row_verify(col) == 0) { // about 860 usec
    return 0;
  } else {
    end_time = micros();
    Serial.print("Time to verify a row: ");
    Serial.println(end_time - start_time);
  }
#endif

  DDRF |= 0b00001111;   // Set I/Ox as OUTPUT, moved out of w()
  // Time generate and fill done at once
  start_time = micros();
  fill_prng_data();
  rand_row_write(col);
  end_time = micros();
  Serial.print("Time to generate and fill a row: ");
  Serial.println(end_time - start_time);

  // Time the row write loop from test()
  row = 0;
  start_time = micros();
  // Write v
  do {
    w(row, col, v);
    row++;
  } while (row);
  end_time = micros();
  Serial.print("Time to fill with a constant   : ");
  Serial.println(end_time - start_time);
  DDRF &= 0b11110000;   // Set I/Ox as INPUT again
  PORTF |= 0b00001111;  // keep pull-ups
}
#endif

/* Improved version of test_rand().  Using an array of data sized to fit
 * a single row, the code to fill that array winds up taking around 172
 * microseconds.  This leaves plenty of time to write the row, generate a
 * new set of random row data, and move on to the next row.
 *
 * DDRF setting is moved from the w() function since this writes in one 
 * fell swoop.  The w() and r() functions now hold the cli() and sei()
 * calls to keep them limited to where they're needed.
 *
 * Results seen when DO_TESTING was defined:
 * Time to generate random row buffer: 172
 * Time to fill a column: 864
 * Time to verify a column: 852
 * Time to generate and fill a row: 1048
 *
 * As a data point, the time that test() takes to write a constant value
 * to a row of ram is about 732 microseconds.
 */
int test_rand() 
{
  uint8_t col = 0;
  uint8_t v;
  uint32_t seed = prng_state;

  DDRF |= 0b00001111;     // Set data pins as OUTPUT, moved out of w()
  // Send random
  do {
    fill_prng_data();     // Fill row buffer  - About 172 usec 
    rand_row_write(col);  // Write row buffer - About 864 usec.
    col++;
  } while (col);

  DDRF &= 0b11110000;     // Set data pins as INPUT again
  PORTF |= 0b00001111;    // keep pull-ups

  // Start verify with our same prng seed
  prng_state = seed;
  col = 0;
  // Verify what was written
  do {
    fill_prng_data();               // 172 usec again
    if (rand_row_verify(col) == 0)  // about 860 usec
      return 0;
    col++;
  } while (col);

  return 1;
}

uint32_t loop_count = 0;

void loop() {
  #ifdef DO_TESTING
  test_speed();
  Serial.println();
  delay(200);
  #endif
  test_and_print("1111", test_1);
  test_and_print("0000", test_0);
  test_and_print("0101", test_01);
  test_and_print("1010", test_10);

  Serial.print("rand seed: "); 
  Serial.println(prng_state, HEX);
  test_and_print("rand", test_rand); 

  // Print a count of how many times we've been through the loop
  Serial.print("Test loops: ");
  Serial.println(++loop_count);
  Serial.println();
}
