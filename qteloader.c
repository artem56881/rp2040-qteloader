#include <stdio.h>
#include "pico/stdlib.h"
#include "drivers/st7735.h"

#define background_color ST7735_BLACK
#define draw_color ST7735_WHITE

#define ok_pin 11
#define up_pin 13
#define down_pin 12
// #define led_pin 20
static int program_addreses[] = {12288, 143360};
#define PROGRAM_A_OFFSET 12288 // 12* 1024
#define PROGRAM_B_OFFSET 143360 // 128*1024 + 12288

void draw_brim(int program_pointer, uint16_t color) {
    ST7735_DrawRect(6, 21*program_pointer+1, 100, 21, color);

}

static inline void goto_app(uint32_t app_offset) {
    gpio_deinit(up_pin);
    gpio_deinit(down_pin);
    gpio_deinit(ok_pin);

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

    gpio_init(up_pin);
    gpio_init(down_pin);
    gpio_init(ok_pin);

    gpio_set_dir(up_pin, GPIO_IN);
    gpio_set_dir(down_pin, GPIO_IN);
    gpio_set_dir(ok_pin, GPIO_IN);

    gpio_pull_up(up_pin);
    gpio_pull_up(down_pin);
    gpio_pull_up(ok_pin);

    int program_pointer = 0;
    ST7735_Init();
    ST7735_SetRotation(3);
    ST7735_FillScreen(background_color);
    ST7735_DrawRectFill(0, 0, 128, 160, background_color);
    ST7735_DrawString(10, 10, "Tracer", Font_7x10, draw_color);
    ST7735_DrawString(10, 30, "Tetris", Font_7x10, draw_color);

    draw_brim(program_pointer, draw_color);

    while (true)
    {
        if(!gpio_get(up_pin) & (program_pointer != 0)) {
            draw_brim(program_pointer, background_color);
            program_pointer--;
            draw_brim(program_pointer, draw_color);
        }
        if(!gpio_get(down_pin) & (program_pointer+1 < sizeof(program_addreses) / sizeof(program_addreses[0]))) {
            draw_brim(program_pointer, background_color);
            program_pointer++;
            draw_brim(program_pointer, draw_color);
        };
        if(!gpio_get(ok_pin)) {goto_app(program_addreses[program_pointer]);}

    }
    
}
