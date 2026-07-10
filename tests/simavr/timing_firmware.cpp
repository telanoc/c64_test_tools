#include <Arduino.h>
#include <avr/io.h>

// Compile the production sketch in this translation unit so the simulator
// exercises the same functions and optimizer output as the uploaded firmware.
#define setup dram_test_setup
#define loop dram_test_loop
#include "../../c64_dram_test_mega2560/c64_dram_test_mega2560.ino"
#undef loop
#undef setup

extern "C" void simavr_start_trace(void);
extern "C" void simavr_stop_trace(void);
extern "C" void simavr_exit_success(void);
extern "C" void simavr_exit_failure(void);

static void run_pattern_write(uint8_t phase, uint8_t pattern)
{
  GPIOR1 = phase;
  DDRF |= 0b00001111;

  for (uint8_t col = 0; col < 4; col++) {
    uint8_t row = 0;
    do {
      w(row, col, pattern);
      row++;
    } while (row);
  }
}

static void run_pattern_verify(uint8_t phase)
{
  GPIOR1 = phase;
  DDRF &= 0b11110000;
  PORTF |= 0b00001111;

  for (uint8_t col = 0; col < 4; col++) {
    uint8_t row = 0;
    do {
      if (r(row, col) != 0x0f) {
        simavr_stop_trace();
        simavr_exit_failure();
      }
      row++;
    } while (row);
  }
}

void setup()
{
  dram_test_setup();

  cli();
  simavr_start_trace();
  PORTA = 0xff; // Ensure the first row address produces a trace transition.

  run_pattern_write(1, 0b1111);
  run_pattern_write(2, 0b0000);
  run_pattern_write(3, 0b0101);
  run_pattern_write(4, 0b1010);
  run_pattern_verify(5);

  GPIOR1 = 6;
  DDRF |= 0b00001111;
  for (uint8_t col = 0; col < 4; col++) {
    fill_prng_data();
    rand_row_write(col);
  }

  DDRF &= 0b11110000;
  PORTF |= 0b00001111;

  GPIOR1 = 7;
  for (uint8_t col = 0; col < 4; col++) {
    fill_prng_data();
    memset(prng_data.b, 0xff, sizeof(prng_data.b));
    if (!rand_row_verify(col)) {
      simavr_stop_trace();
      simavr_exit_failure();
    }
  }

  simavr_stop_trace();
  simavr_exit_success();
}

void loop()
{
}
