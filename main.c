/* DRAM Tester */

#include "cy_pdl.h"
#include "cy_retarget_io.h"
#include "cyhal.h"
#include "cybsp.h"

#include <unistd.h>

// GPIO assigmnents

// P9_0-P9_6, P10_0-P10_2   A0-A9
// P10_3-P10_6              DQ1-DQ4
// P5_2                     OE
// P5_3                     WE
// P5_4                     RAS
// P5_5                     CAS

// Pinout on IDC26

//  1 A0
//  2 A1
//  3 A2
//  4 A3
//  5 A4
//  6 A5
//  7 A6
//  8 A7
//  9 A8
// 10 A9
// 11 DQ1
// 12 DQ2
// 13 DQ3
// 14 DQ4
// 15 OE
// 16 WE
// 17 RAS
// 18 CAS
// 19 GND
// 20 +5V

#define FPM

#define ADDR_BITS    10
#define SIZE         (1 << ADDR_BITS)

#define ADDR_L_HAL_PORT  CYHAL_PORT_9
#define ADDR_L_PORT      GPIO_PRT9
#define ADDR_L_MASK      0x3f
#define ADDR_L_BITS      7

#define ADDR_H_HAL_PORT  CYHAL_PORT_10
#define ADDR_H_PORT      GPIO_PRT10

#define DQ1 P10_3
#define DQ2 P10_4
#define DQ3 P10_5
#define DQ4 P10_6

#define OE  P5_2
#define WE  P5_3
#define RAS P5_4
#define CAS P5_5

void
init_hardware()
{
  cy_rslt_t result;

  /* Initialize the device and board peripherals */
  result = cybsp_init();
    
  /* Board init failed. Stop program execution */
  if (result != CY_RSLT_SUCCESS) {
    CY_ASSERT(0);
  }

  /* Initialize retarget-io to use the debug UART port */
  cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);

  /* Initialize the user LED */
  cyhal_gpio_init(CYBSP_USER_LED, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);

  /* Initialize the user button */
  cyhal_gpio_init(CYBSP_USER_BTN, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_PULLUP, CYBSP_BTN_OFF);

  /* Initialize GPIO output ports */

  for (uint8_t bit = 0; bit < ADDR_L_BITS; bit++) {
    cyhal_gpio_init(CYHAL_GET_GPIO(ADDR_L_HAL_PORT, bit), CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, 0);
  }
  for (uint8_t bit = 0; bit < ADDR_BITS - ADDR_L_BITS; bit++) {
    cyhal_gpio_init(CYHAL_GET_GPIO(ADDR_H_HAL_PORT, bit), CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, 0);
  }

  cyhal_gpio_init(WE, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, 0);
  cyhal_gpio_write(WE, 1);
  cyhal_gpio_init(OE, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, 0);
  cyhal_gpio_write(OE, 1);
  cyhal_gpio_init(RAS, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, 0);
  cyhal_gpio_write(RAS, 1);
  cyhal_gpio_init(CAS, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, 0);
  cyhal_gpio_write(CAS, 1);
}

int
wait_for_key(const char* prompt)
{
  write(0, prompt, strlen(prompt));
  char s[1];
  read(0, s, 1);
  write(0, s, 1);
  write(0, "\r\n", 2);
  return s[0];
}

void
say(const char* s)
{
  write(0, s, strlen(s));
  write(0, "\r\n", 2);
}

void
wait_for_button()
{
  while (cyhal_gpio_read(CYBSP_USER_BTN)) {
    cyhal_gpio_write(CYBSP_USER_LED, CYBSP_LED_STATE_ON);
    cyhal_system_delay_ms(100);
    if (!cyhal_gpio_read(CYBSP_USER_BTN)) {
      break;
    }
    cyhal_gpio_write(CYBSP_USER_LED, CYBSP_LED_STATE_OFF);
    cyhal_system_delay_ms(300);
  }
}

static inline void
write_address(uint16_t address)
{
  ADDR_L_PORT->OUT = address & ADDR_L_MASK;
  ADDR_H_PORT->OUT = address >> ADDR_L_BITS;
}

static inline void
set_data_lines_to_output()
{
  cyhal_gpio_init(DQ1, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, 0);
  cyhal_gpio_init(DQ2, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, 0);
  cyhal_gpio_init(DQ3, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, 0);
  cyhal_gpio_init(DQ4, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, 0);
}

static inline void
set_data_lines_to_input()
{
  cyhal_gpio_init(DQ1, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_NONE, 0);
  cyhal_gpio_init(DQ2, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_NONE, 0);
  cyhal_gpio_init(DQ3, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_NONE, 0);
  cyhal_gpio_init(DQ4, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_NONE, 0);
}

static inline uint8_t
read_data()
{
  return
    (cyhal_gpio_read(DQ1) ? 1 : 0)
    | (cyhal_gpio_read(DQ2) ? 2 : 0)
    | (cyhal_gpio_read(DQ3) ? 4 : 0)
    | (cyhal_gpio_read(DQ4) ? 8 : 0);
}

static inline void
write_data(uint8_t value)
{
  cyhal_gpio_write(DQ1, value & 1 ? 1 : 0);
  cyhal_gpio_write(DQ2, value & 2 ? 1 : 0);
  cyhal_gpio_write(DQ3, value & 4 ? 1 : 0);
  cyhal_gpio_write(DQ4, value & 8 ? 1 : 0);
}

void
fixed_value_test(uint8_t value)
{
  printf("Running fixed value 0x%02x test\r\n", value);

  printf("Writing..."); fflush(stdout);
  set_data_lines_to_output();
  cyhal_gpio_write(WE, 0);
  for (uint16_t row = 0; row < SIZE; row++) {
#if defined(FPM)
      write_address(row);
      cyhal_gpio_write(RAS, 0);
#endif
    for (uint16_t col = 0; col < SIZE; col++) {
      write_data(value);
#if !defined(FPM)
      write_address(row);
      cyhal_gpio_write(RAS, 0);
#endif
      write_address(col);
      cyhal_gpio_write(CAS, 0);
      cyhal_gpio_write(CAS, 1);
#if !defined(FPM)
      cyhal_gpio_write(RAS, 1);
#endif
    }
#if defined(FPM)
    cyhal_gpio_write(RAS, 1);
#endif
  }
  cyhal_gpio_write(WE, 1);

  printf(" Reading..."); fflush(stdout);
  set_data_lines_to_input();
  cyhal_gpio_write(OE, 0);
  {
    uint8_t error = 0;
    for (uint16_t row = 0; !error && row < SIZE; row++) {
#if defined(FPM)
      write_address(row);
      cyhal_gpio_write(RAS, 0);
#endif
      for (uint16_t col = 0; !error && col < SIZE; col++) {
        uint8_t read_value;
#if !defined(FPM)
        write_address(row);
        cyhal_gpio_write(RAS, 0);
#endif
        write_address(col);
        cyhal_gpio_write(CAS, 0);
        read_value = read_data();
        cyhal_gpio_write(CAS, 1);
        if (value != read_value) {
          printf("\r\nRead %d/%d returned unexpected 0x%02x\007\r\n", row, col, read_value);
          error = 1;
        }
#if !defined(FPM)
        cyhal_gpio_write(RAS, 1);
#endif
      }
#if defined(FPM)
      cyhal_gpio_write(RAS, 1);
#endif
    }
  }
  cyhal_gpio_write(OE, 1);
  printf(" Done\r\n");
}

int
main(void)
{
  init_hardware();

  say("\n\n");
  say("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-");
  say("");
  say("FPM DRAM test");

  for (;;) {
    fixed_value_test(0xf);
    fixed_value_test(0x5);
    fixed_value_test(0xa);
    fixed_value_test(0x0);
  }
}
