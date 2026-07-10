#include <avr/io.h>

#include "avr_mcu_section.h"

AVR_MCU(F_CPU, "atmega2560");
AVR_MCU_SIMAVR_COMMAND(&GPIOR0);
AVR_MCU_VCD_FILE(".pio/timing.vcd", 1000);
AVR_MCU_VCD_REGISTER(PORTA);
AVR_MCU_VCD_REGISTER(GPIOR1);
AVR_MCU_VCD_PORT_PIN('C', 2, "RAS");

void simavr_start_trace(void)
{
  GPIOR0 = SIMAVR_CMD_VCD_START_TRACE;
}

void simavr_stop_trace(void)
{
  GPIOR0 = SIMAVR_CMD_VCD_STOP_TRACE;
}

void simavr_exit_success(void)
{
  GPIOR0 = SIMAVR_CMD_EXIT_CODE_0;
}

void simavr_exit_failure(void)
{
  GPIOR0 = SIMAVR_CMD_EXIT_CODE_1;
}
