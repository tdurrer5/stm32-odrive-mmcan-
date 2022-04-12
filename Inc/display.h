#ifndef __DISPLAY_H
#define __DISPLAY_H

#include "u8g2.h"

#define DISPLAY_MAX_PREFIX_LENGTH 6

uint8_t u8x8_byte_3wire_hw_spi(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
uint8_t u8g2_gpio_and_delay_stm32(U8X8_UNUSED u8x8_t *u8x8, U8X8_UNUSED uint8_t msg,
                                  U8X8_UNUSED uint8_t arg_int, U8X8_UNUSED void *arg_ptr);

void display_init(void);
void display_update(void);
uint8_t display_add_float_line(char *prefix, float value, uint8_t line_number);
uint8_t display_add_string_line(char *string, uint8_t line_number);

#endif /* __DISPLAY_H */