#include <stdio.h>
#include "pico/stdlib.h"
#include "drivers/st7735.h"

#define A_pin 11
#define B_pin 17
// #define led_pin 20

#define PROGRAM_A_OFFSET 12288 // 12* 1024
#define PROGRAM_B_OFFSET 143360 // 128*1024 + 12288

static inline void goto_app(uint32_t app_offset) {
    gpio_deinit(A_pin);
    gpio_deinit(B_pin);
    uint32_t app_base = XIP_BASE + app_offset;

    asm volatile (
        "mov r0, %[base]\n"
        "ldr r1, =%[vtor]\n"
        "str r0, [r1]\n"
        "ldmia r0, {r0, r1}\n"
        "msr msp, r0\n"
        "bx  r1\n"
        :
        : [base] "r" (app_base),
          [vtor] "i" (PPB_BASE + M0PLUS_VTOR_OFFSET)
        : "r0", "r1"
    );
}

int main()
{
    stdio_init_all();

    ST7735_Init();
    ST7735_SetRotation(3);
    ST7735_FillScreen(ST7735_BLACK);
    ST7735_DrawRectFill(0, 0, 128, 160, ST7735_BLACK);
    ST7735_DrawString(10, 10, "Tetris", Font_7x10, ST7735_WHITE);
    ST7735_DrawString(10, 30, "Tracer", Font_7x10, ST7735_WHITE);
    gpio_init(A_pin);
    gpio_set_dir(A_pin, GPIO_IN);

    gpio_pull_up(A_pin);

    // gpio_init(led_pin);
    // gpio_set_dir(led_pin, GPIO_OUT);

    gpio_init(B_pin);
    gpio_set_dir(B_pin, GPIO_IN);

    gpio_pull_up(B_pin);

    while (true)
    {

        if (!gpio_get(A_pin)) {
            goto_app(PROGRAM_A_OFFSET);
            // gpio_put(led_pin, true);
    
        }

        if (!gpio_get(B_pin)) {
            goto_app(PROGRAM_B_OFFSET);
            // gpio_put(led_pin, true);

    
        }
        // else{gpio_put(led_pin, false);}
    }
}
