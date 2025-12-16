#include <stdio.h>
#include "pico/stdlib.h"
#include "drivers/st7735.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <string.h>

#define background_color ST7735_BLACK
#define draw_color ST7735_WHITE

#define ok_pin 11
#define up_pin 13
#define down_pin 12
#define scan_pin 15

#define FLASH_SIZE_KB 4096          // flash chip size in kb. Typical is 2048, 4096, 8192, 16384. (2048 for pico)

#define NUMBER_OF_APPS 10

#define BOOTLOADER_FLASH_SIZE 128   // kb

#define FLASH_SECTOR_SIZE 4096
#define FLASH_PAGE_SIZE   256       //bytes
#define NVS_SIZE 4096               //bytes

#define NVS_FLASH_OFFSET (BOOTLOADER_FLASH_SIZE * 1024 - NVS_SIZE)
#define NVS_FLASH_ADDR (XIP_BASE + NVS_FLASH_OFFSET)

#define NVS_MAGIC 0xA55A1234

static int program_addreses[NUMBER_OF_APPS] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

typedef struct {
    uint32_t magic;
    int32_t program_addresses[NUMBER_OF_APPS];
} nvs_t;

const nvs_t *nvs_flash = (const nvs_t *)NVS_FLASH_ADDR;

void draw_app_list(){
    for(int i = 0; i < NUMBER_OF_APPS; i++){
        if(program_addreses[i] == -1){
            return;
        }
        else{
        char app_address_string[40];
        snprintf(app_address_string, sizeof(app_address_string), "%d", program_addreses[i]);
        ST7735_DrawString(10, 21*i+10, app_address_string, Font_7x10, draw_color);
        }
    }
}

void load_program_addresses_from_flash(void) {
    if (nvs_flash->magic == NVS_MAGIC) {
        memcpy(program_addreses,
               nvs_flash->program_addresses,
               sizeof(program_addreses));
    } else {
        for (int i = 0; i < NUMBER_OF_APPS; i++)
            program_addreses[i] = -1;
    }
    char apps_on_flash_string[24];
    snprintf(apps_on_flash_string, sizeof(apps_on_flash_string), "%s%d", "apps on flash: ", sizeof(program_addreses)/sizeof(program_addreses[0]));
    ST7735_DrawString(0, 115, apps_on_flash_string, Font_7x10, draw_color);
    draw_app_list();

}

void save_program_addresses_to_flash(void) {
    nvs_t nvs;

    nvs.magic = NVS_MAGIC;
    memcpy(nvs.program_addresses,
           program_addreses,
           sizeof(program_addreses));

    uint32_t ints = save_and_disable_interrupts();

    flash_range_erase(NVS_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(NVS_FLASH_OFFSET,
                        (uint8_t *)&nvs,
                        sizeof(nvs));

    restore_interrupts(ints);
}

void draw_brim(int program_pointer, uint16_t color) {
    ST7735_DrawRect(6, 21*program_pointer+1, 100, 21, color);

}


void add_new_app_address(int app_address) {
    for (int i = 0; i < NUMBER_OF_APPS; i++) {
        if (program_addreses[i] == app_address) {
            return;
        }
        if (program_addreses[i] == -1) {
            program_addreses[i] = app_address;
            return;
        }
    }
}

void scan_flash_for_executables(){
    int executables_found = 0;
    for (int i = 1000; i < FLASH_SIZE_KB * 1024; i++) {
        uint8_t *flash_target_contents = (uint8_t *) (XIP_BASE + i + 0xD4);
        if ((flash_target_contents[0]==0xF2) &&
            (flash_target_contents[1]==0xEB) &&
            (flash_target_contents[2]==0x88) &&
            (flash_target_contents[3]==0x71)) {
                printf("kek\n");
                printf("%s%d\n", "app found!: ", i);
                add_new_app_address(i);
                executables_found += 1;
            }
    }
    // save_program_addresses_to_flash();

}

static inline void goto_app(uint32_t app_offset) {
    gpio_deinit(up_pin);
    gpio_deinit(down_pin);
    gpio_deinit(ok_pin);
    uint8_t *flash_target_contents = (uint8_t *) (XIP_BASE + app_offset + 0xD4);

    if ((flash_target_contents[0]==0xF2) &&
        (flash_target_contents[1]==0xEB) &&
        (flash_target_contents[2]==0x88) &&
        (flash_target_contents[3]==0x71)) {
    
        ST7735_FillScreen(ST7735_BLACK);
            
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
}

void init_button(uint pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
};

int main() {
    stdio_init_all();

    init_button(up_pin);
    init_button(down_pin);
    init_button(ok_pin);
    init_button(scan_pin);

    // load_program_addresses_from_flash();

    int program_pointer = 0;
    ST7735_Init();
    ST7735_SetRotation(3);
    ST7735_FillScreen(background_color);
    ST7735_BacklightOn();
    ST7735_DrawRectFill(0, 0, 128, 160, background_color);

    draw_brim(program_pointer, draw_color);
    while (true)
    {
        printf("running\n");
        if(!gpio_get(up_pin) && (program_pointer != 0)) {
            sleep_ms(100);
            draw_brim(program_pointer, background_color);
            program_pointer--;
            draw_brim(program_pointer, draw_color);
        }
        else if(!gpio_get(down_pin) && (program_pointer+1 < sizeof(program_addreses) / sizeof(program_addreses[0])) && (program_addreses[program_pointer+1] != -1)) {
            sleep_ms(100);
            draw_brim(program_pointer, background_color);
            program_pointer++;
            draw_brim(program_pointer, draw_color);
        };
        if(!gpio_get(ok_pin)) {goto_app(program_addreses[program_pointer]);}
        
        if(!gpio_get(scan_pin)) {
            ST7735_DrawRectFill(0, 115, 128, 10, background_color);
            scan_flash_for_executables();
            // load_program_addresses_from_flash();
            draw_app_list();


            sleep_ms(100);
        }

    }
    
}
