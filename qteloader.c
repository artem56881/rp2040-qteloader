#include <stdio.h>
#include "pico/stdlib.h"

#define A_pin 11
#define B_pin 17
#define led_pin 20

#define PROGRAM_A_OFFSET 12288 // 12* 1024
#define PROGRAM_B_OFFSET 274432 // 256*1024 + 12288

static inline void goto_app_A() {

    asm volatile (
    "mov r0, %[start]\n"
    "ldr r1, =%[vtable]\n"
    "str r0, [r1]\n"
    "ldmia r0, {r0, r1}\n"
    "msr msp, r0\n"
    "bx r1\n"
    :
    : [start] "r" (XIP_BASE + PROGRAM_A_OFFSET), [vtable] "X" (PPB_BASE + M0PLUS_VTOR_OFFSET)
    :
    );
}

static inline void goto_app_B() {

    asm volatile (
    "mov r0, %[start]\n"
    "ldr r1, =%[vtable]\n"
    "str r0, [r1]\n"
    "ldmia r0, {r0, r1}\n"
    "msr msp, r0\n"
    "bx r1\n"
    :
    : [start] "r" (XIP_BASE + PROGRAM_B_OFFSET), [vtable] "X" (PPB_BASE + M0PLUS_VTOR_OFFSET)
    :
    );
}

int main()
{
    stdio_init_all();

    gpio_init(A_pin);
    gpio_set_dir(A_pin, GPIO_IN);

    gpio_pull_up(A_pin);


    gpio_init(led_pin);
    gpio_set_dir(led_pin, GPIO_OUT);


    gpio_init(B_pin);
    gpio_set_dir(B_pin, GPIO_IN);

    gpio_pull_up(B_pin);

    while (true)
    {

        if (!gpio_get(A_pin)) {
            goto_app_A();
            // gpio_put(led_pin, true);
    
        }

        if (!gpio_get(B_pin)) {
            goto_app_B();
            // gpio_put(led_pin, true);

    
        }
        // else{gpio_put(led_pin, false);}
    }
}
