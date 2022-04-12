
#include "u8g2.h"
#include "cmsis_os.h"
#include "main.h"
#include "stm32f4xx_hal.h"
#include "spi.h"
#include "string.h"
#include "freertos_vars.h"
#include "display.h"

u8g2_t _u8g2;

uint8_t u8x8_byte_3wire_hw_spi(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    switch (msg)
    {
    case U8X8_MSG_BYTE_SEND:
        HAL_SPI_Transmit(&hspi2, (uint8_t *)arg_ptr, arg_int, HAL_MAX_DELAY);
        break;
    case U8X8_MSG_BYTE_INIT:
        break;
    case U8X8_MSG_BYTE_START_TRANSFER:
        u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_enable_level);
        u8x8->gpio_and_delay_cb(u8x8, U8X8_MSG_DELAY_NANO, u8x8->display_info->post_chip_enable_wait_ns, NULL);
        break;
    case U8X8_MSG_BYTE_END_TRANSFER:
        u8x8->gpio_and_delay_cb(u8x8, U8X8_MSG_DELAY_NANO, u8x8->display_info->pre_chip_disable_wait_ns, NULL);
        u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_disable_level);
        break;
    default:
        return 0;
    }
    return 1;
}

uint8_t u8g2_gpio_and_delay_stm32(U8X8_UNUSED u8x8_t *u8x8, U8X8_UNUSED uint8_t msg, U8X8_UNUSED uint8_t arg_int, U8X8_UNUSED void *arg_ptr)
{
    switch (msg)
    {
    //Initialize SPI peripheral
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
        /* HAL initialization contains all what we need so we can skip this part. */
        break;

    //Function which implements a delay, arg_int contains the amount of ms
    case U8X8_MSG_DELAY_MILLI:
        osDelay(arg_int);
        break;

    //Function which delays 10us
    case U8X8_MSG_DELAY_10MICRO:
        for (uint16_t n = 0; n <= 280; n++)
        {
            __NOP();
        }
        break;

    case U8X8_MSG_DELAY_NANO:
        for (uint16_t n = 0; n <= (arg_int / 36); n++)
        {
            __NOP();
        }

    //Function which delays 100ns
    case U8X8_MSG_DELAY_100NANO:
        for (uint16_t n = 0; n <= 3; n++)
        {
            __NOP();
        }
        break;

    // Function to define the logic level of the CS line
    case U8X8_MSG_GPIO_CS:
        if (arg_int)
            HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, SET);
        else
            HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, RESET);

        break;

    //Function to define the logic level of the RESET line
    case U8X8_MSG_GPIO_RESET:
        if (arg_int)
            HAL_GPIO_WritePin(LCD_RESET_GPIO_Port, LCD_RESET_Pin, SET);
        else
            HAL_GPIO_WritePin(LCD_RESET_GPIO_Port, LCD_RESET_Pin, RESET);
        break;

    default:
        return 0; //A message was received which is not implemented, return 0 to indicate an error
    }

    return 1; // command processed successfully.
}

void display_init(void)
{
    u8g2_Setup_st7920_s_128x64_f(&_u8g2, U8G2_R0, u8x8_byte_3wire_hw_spi, u8g2_gpio_and_delay_stm32);
    u8g2_InitDisplay(&_u8g2);     // send init sequence to the display, display is in sleep mode after this,
    u8g2_SetPowerSave(&_u8g2, 0); // wake up display
    HAL_GPIO_WritePin(LCD_RESET_GPIO_Port, LCD_RESET_Pin, RESET);
    osDelay(100);
    HAL_GPIO_WritePin(LCD_RESET_GPIO_Port, LCD_RESET_Pin, SET);
    osDelay(200);

    u8g2_ClearDisplay(&_u8g2);
    u8g2_SetFont(&_u8g2, u8g2_font_amstrad_cpc_extended_8r);
}

uint8_t display_add_float_line(char *prefix, float value, uint8_t line_number)
{

    uint8_t prefix_length = strlen(prefix);

    if (prefix_length > DISPLAY_MAX_PREFIX_LENGTH)
    {
        // Prefix is too long to support a single row
        return 1;
    }

    if (line_number > 8)
    {
        // Only 8 lines are supported
        return 1;
    }

    u8g2_DrawStr(&_u8g2, 0, line_number * 8, prefix);

    for (uint8_t i = 0; i < DISPLAY_MAX_PREFIX_LENGTH - prefix_length; i++)
    {
        u8g2_DrawStr(&_u8g2, (prefix_length * 8 + i + 1), line_number * 8, " ");
    }

    char value_buf[10];
    gcvt(value, 4, value_buf);
    // Clear the previous string
    u8g2_DrawStr(&_u8g2, DISPLAY_MAX_PREFIX_LENGTH * 8, line_number * 8, "          ");
    // Place the float value in the last 9 digits
    u8g2_DrawStr(&_u8g2, DISPLAY_MAX_PREFIX_LENGTH * 8, line_number * 8, value_buf);

    return 0;
}

uint8_t display_add_string_line(char *string, uint8_t line_number)
{
    uint8_t string_length = strlen(string);

    if (string_length > 16)
    {
        // string is too long to support a single row, so truncate it
        return 1;
    }

    if (line_number > 8)
    {
        // Only 8 lines are supported
        return 1;
    }

    u8g2_DrawStr(&_u8g2, 0, line_number * 8, string);

    return 0;
}

void display_update(void)
{
    u8g2_SendBuffer(&_u8g2);
}