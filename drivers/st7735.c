#include "st7735.h"

static const uint8_t init_cmds1[] = { // Init for 7735R, part 1 (red or green tab)
    15,                               // 15 commands in list:
    ST7735_SWRESET, DELAY,            //  1: Software reset, 0 args, w/delay
    150,                              //     150 ms delay
    ST7735_SLPOUT, DELAY,             //  2: Out of sleep mode, 0 args, w/delay
    255,                              //     500 ms delay
    ST7735_FRMCTR1, 3,                //  3: Frame rate ctrl - normal mode, 3 args:
    0x01, 0x2C, 0x2D,                 //     Rate = fosc/(1x2+40) * (LINE+2C+2D)
    ST7735_FRMCTR2, 3,                //  4: Frame rate control - idle mode, 3 args:
    0x01, 0x2C, 0x2D,                 //     Rate = fosc/(1x2+40) * (LINE+2C+2D)
    ST7735_FRMCTR3, 6,                //  5: Frame rate ctrl - partial mode, 6 args:
    0x01, 0x2C, 0x2D,                 //     Dot inversion mode
    0x01, 0x2C, 0x2D,                 //     Line inversion mode
    ST7735_INVCTR, 1,                 //  6: Display inversion ctrl, 1 arg, no delay:
    0x07,                             //     No inversion
    ST7735_PWCTR1, 3,                 //  7: Power control, 3 args, no delay:
    0xA2, 0x02,                       //     -4.6V
    0x84,                             //     AUTO mode
    ST7735_PWCTR2, 1,                 //  8: Power control, 1 arg, no delay:
    0xC5,                             //     VGH25 = 2.4C VGSEL = -10 VGH = 3 * AVDD
    ST7735_PWCTR3, 2,                 //  9: Power control, 2 args, no delay:
    0x0A,                             //     Opamp current small
    0x00,                             //     Boost frequency
    ST7735_PWCTR4, 2,                 // 10: Power control, 2 args, no delay:
    0x8A,                             //     BCLK/2, Opamp current small & Medium low
    0x2A,
    ST7735_PWCTR5, 2, // 11: Power control, 2 args, no delay:
    0x8A, 0xEE,
    ST7735_VMCTR1, 1, // 12: Power control, 1 arg, no delay:
    0x0E,
    ST7735_INVOFF, 0,     // 13: Don't invert display, no args, no delay
    ST7735_MADCTL, 1,     // 14: Memory access control (directions), 1 arg:
    ST7735_DATA_ROTATION, //     row addr/col addr, bottom to top refresh
    ST7735_COLMOD, 1,     // 15: set color mode, 1 arg, no delay:
    0x05},                //     16-bit color

    init_cmds2[] = {     // Init for 7735R, part 2 (1.44" display)
        2,               //  2 commands in list:
        ST7735_CASET, 4, //  1: Column addr set, 4 args, no delay:
        0x00, 0x00,      //     XSTART = 0
        0x00, 0x7F,      //     XEND = 127
        ST7735_RASET, 4, //  2: Row addr set, 4 args, no delay:
        0x00, 0x00,      //     XSTART = 0
        0x00, 0x7F},     //     XEND = 127

    init_cmds3[] = { // Init for 7735R, part 3 (red or green tab)
        4,           //  4 commands in list:
        ST7735_GMCTRP1,
        16, //  1: Magical unicorn dust, 16 args, no delay:
        0x02, 0x1c, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2d, 0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10, ST7735_GMCTRN1,
        16,                                                                                                                  //  2: Sparkles and rainbows, 16 args, no delay:
        0x03, 0x1d, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D, 0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10, ST7735_NORON, DELAY, //  3: Normal display on, no args, w/delay
        10,                                                                                                                  //     10 ms delay
        ST7735_DISPON, DELAY,                                                                                                //  4: Main screen turn on, no args w/delay
        100};                                                                                                                //     100 ms delay

static int16_t _height = ST7735_HEIGHT, _width = ST7735_WIDTH;
static uint8_t _xstart = ST7735_XSTART, _ystart = ST7735_YSTART;
static uint8_t _data_rotation[4] = {ST7735_MADCTL_MX, ST7735_MADCTL_MY, ST7735_MADCTL_MV, ST7735_MADCTL_BGR};

// Инициализация SPI
static void ST7735_SPI_Init()
{
    spi_init(spi_default, 62500 * 1000); // 62.5 MHz (максимум для RP2040)

    gpio_set_function(PIN_LCD_DIN, GPIO_FUNC_SPI);
    gpio_set_function(PIN_LCD_CLK, GPIO_FUNC_SPI);

    // Инициализация управляющих пинов
    gpio_init(PIN_LCD_CS);
    gpio_init(PIN_LCD_DC);
    gpio_init(PIN_LCD_RST);
    gpio_init(PIN_LCD_BL);

    gpio_set_dir(PIN_LCD_CS, GPIO_OUT);
    gpio_set_dir(PIN_LCD_DC, GPIO_OUT);
    gpio_set_dir(PIN_LCD_RST, GPIO_OUT);
    gpio_set_dir(PIN_LCD_BL, GPIO_OUT);

    gpio_put(PIN_LCD_CS, 1);
    gpio_put(PIN_LCD_DC, 1);
    // gpio_put(PIN_LCD_BL, 1); // Включить подсветку
}

static void ST7735_Reset()
{
    gpio_put(PIN_LCD_RST, 0);
    sleep_ms(100);
    gpio_put(PIN_LCD_RST, 1);
    sleep_ms(100);
}

// Отправка команды на дисплей
static void ST7735_WriteCommand(uint8_t cmd)
{
    gpio_put(PIN_LCD_CS, 0); // Активировать чип
    gpio_put(PIN_LCD_DC, 0); // Командный режим
    spi_write_blocking(spi_default, &cmd, 1);
    gpio_put(PIN_LCD_CS, 1); // Деактивировать чип
}

// Отправка данных на дисплей
static void ST7735_WriteData(uint8_t *data, size_t buff_size)
{
    gpio_put(PIN_LCD_CS, 0); // Активировать чип
    gpio_put(PIN_LCD_DC, 1); // Режим данных
    spi_write_blocking(spi_default, data, buff_size);
    gpio_put(PIN_LCD_CS, 1); // Деактивировать чип
}

static void ST7735_SetAddressWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1)
{
    // column address set
    ST7735_WriteCommand(ST7735_CASET);
    uint8_t data[] = {0x00, x0 + _xstart, 0x00, x1 + _xstart};
    ST7735_WriteData(data, sizeof(data));

    // row address set
    ST7735_WriteCommand(ST7735_RASET);
    data[1] = y0 + _ystart;
    data[3] = y1 + _ystart;
    ST7735_WriteData(data, sizeof(data));

    // write to RAM
    ST7735_WriteCommand(ST7735_RAMWR);
}

static void ST7735_ExecuteCommandList(const uint8_t *addr)
{
    uint8_t numCommands, numArgs;
    uint16_t ms;

    numCommands = *addr++;
    while (numCommands--)
    {
        uint8_t cmd = *addr++;

        ST7735_WriteCommand(cmd);

        numArgs = *addr++;
        // If high bit set, delay follows args
        ms = numArgs & DELAY;
        numArgs &= ~DELAY;

        if (numArgs)
        {
            ST7735_WriteData((uint8_t *)addr, numArgs);
            addr += numArgs;
        }

        if (ms)
        {
            ms = *addr++;
            if (ms == 255)
                ms = 500;
            sleep_ms(ms);
        }
    }
}

void ST7735_Init(void)
{
    ST7735_SPI_Init();

    gpio_put(PIN_LCD_CS, 0);

    ST7735_Reset();

    ST7735_ExecuteCommandList(init_cmds1);
    ST7735_ExecuteCommandList(init_cmds2);
    ST7735_ExecuteCommandList(init_cmds3);

    gpio_put(PIN_LCD_CS, 1);
}

void ST7735_DrawChar(uint16_t x, uint16_t y, char ch, FontDef font, uint16_t color)
{
    uint32_t i, b, j;
    for (i = 0; i < font.height; i++)
    {
        b = font.data[(ch - 32) * font.height + i];
        for (j = 0; j < font.width; j++)
        {
            if ((b << j) & 0x8000)
            {
                ST7735_DrawPixel((x + j), (y + i), color);

                // ST7735_DrawPixel((x + j) * 2, (y + i) * 2, color);
                // ST7735_DrawPixel((x + j) * 2 + 1, (y + i) * 2 + 1, color);
                // ST7735_DrawPixel((x + j) * 2, (y + i) * 2 + 1, color);
                // ST7735_DrawPixel((x + j) * 2 + 1, (y + i) * 2, color);
            }
        }
    }
}

void ST7735_DrawString(uint16_t x, uint16_t y, const char *str, FontDef font, uint16_t color)
{
    while (*str)
    {
        if (x + font.width >= _width)
        {
            x = 0;
            y += font.height;
            if (y + font.height >= _height)
            {
                break;
            }

            if (*str == ' ')
            {
                // skip spaces in the beginning of the new line
                str++;
                continue;
            }
        }

        ST7735_DrawChar(x, y, *str, font, color);
        x += font.width;
        str++;
    }
}

void ST7735_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    if ((x >= _width) || (y >= _height))
        return;

    uint8_t data[] = {color >> 8, color & 0xFF};

    ST7735_SetAddressWindow(x, y, x + 1, y + 1);

    ST7735_WriteData(data, sizeof(data));
}

void ST7735_FillScreen(uint16_t color)
{
    ST7735_SetAddressWindow(0, 0, ST7735_WIDTH - 1, ST7735_HEIGHT - 1);

    uint8_t data[] = {color >> 8, color & 0xFF};
    ;

    gpio_put(PIN_LCD_CS, 0);
    gpio_put(PIN_LCD_DC, 1);

    for (uint32_t i = 0; i < ST7735_WIDTH * ST7735_HEIGHT; i++)
    {
        spi_write_blocking(spi_default, data, sizeof(data));
    }

    gpio_put(PIN_LCD_CS, 1);
}

void ST7735_DrawRectFill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if ((x >= _width) || (y >= _height))
        return;
    if ((x + w - 1) >= _width)
        w = _width - x;
    if ((y + h - 1) >= _height)
        h = _height - y;

    ST7735_SetAddressWindow(x, y, x + w - 1, y + h - 1);

    uint8_t data[2] = {color >> 8, color & 0xFF};

    gpio_put(PIN_LCD_CS, 0);
    gpio_put(PIN_LCD_DC, 1);

    for (y = h; y > 0; y--)
    {
        for (x = w; x > 0; x--)
        {
            spi_write_blocking(spi_default, data, 2);
        }
    }

    gpio_put(PIN_LCD_CS, 1);
}

void ST7735_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data)
{
    if ((x >= _width) || (y >= _height))
        return;
    if ((x + w - 1) >= _width)
        return;
    if ((y + h - 1) >= _height)
        return;

    ST7735_SetAddressWindow(x, y, x + w - 1, y + h - 1);
    ST7735_WriteData((uint8_t *)data, sizeof(uint16_t) * w * h);
}

void ST7735_BacklightOn()
{
    gpio_put(PIN_LCD_BL, 1);
}

void ST7735_BacklightOff()
{
    gpio_put(PIN_LCD_BL, 0);
}

void ST7735_InvertColors(bool invert)
{
    ST7735_WriteCommand(invert ? ST7735_INVON : ST7735_INVOFF);
}

void ST7735_DrawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -r - r;
    int16_t x = 0;

    ST7735_DrawPixel(x0 + r, y0, color);
    ST7735_DrawPixel(x0 - r, y0, color);
    ST7735_DrawPixel(x0, y0 - r, color);
    ST7735_DrawPixel(x0, y0 + r, color);

    while (x < r)
    {
        if (f >= 0)
        {
            r--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        ST7735_DrawPixel(x0 + x, y0 + r, color);
        ST7735_DrawPixel(x0 - x, y0 + r, color);
        ST7735_DrawPixel(x0 - x, y0 - r, color);
        ST7735_DrawPixel(x0 + x, y0 - r, color);

        ST7735_DrawPixel(x0 + r, y0 + x, color);
        ST7735_DrawPixel(x0 - r, y0 + x, color);
        ST7735_DrawPixel(x0 - r, y0 - x, color);
        ST7735_DrawPixel(x0 + r, y0 - x, color);
    }
}

void ST7735_DrawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
{
    // Rudimentary clipping
    if ((x >= _width) || (y >= _height))
        return;
    if ((y + h - 1) >= _height)
        h = _height - y;

    ST7735_DrawRectFill(x, y, 1, h, color);
    // ST7735_DrawLine(x, y, 1, h, color);
}

void ST7735_DrawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
{
    // Rudimentary clipping
    if ((x >= _width) || (y >= _height))
        return;
    if ((x + w - 1) >= _width)
        w = _width - x;

    ST7735_DrawRectFill(x, y, w, 1, color);
}

static void ST7735_DrawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, uint16_t color)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;

    while (x < r)
    {
        if (f >= 0)
        {
            r--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;
        if (cornername & 0x8)
        {
            ST7735_DrawPixel(x0 - r, y0 + x, color);
            ST7735_DrawPixel(x0 - x, y0 + r, color);
        }
        if (cornername & 0x4)
        {
            ST7735_DrawPixel(x0 + x, y0 + r, color);
            ST7735_DrawPixel(x0 + r, y0 + x, color);
        }
        if (cornername & 0x2)
        {
            ST7735_DrawPixel(x0 + r, y0 - x, color);
            ST7735_DrawPixel(x0 + x, y0 - r, color);
        }
        if (cornername & 0x1)
        {
            ST7735_DrawPixel(x0 - x, y0 - r, color);
            ST7735_DrawPixel(x0 - r, y0 - x, color);
        }
    }
}

static void ST7735_FillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, int16_t delta, uint16_t color)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -r - r;
    int16_t x = 0;

    delta++;
    while (x < r)
    {
        if (f >= 0)
        {
            r--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        if (cornername & 0x1)
        {
            ST7735_DrawFastVLine(x0 + x, y0 - r, r + r + delta, color);
            ST7735_DrawFastVLine(x0 + r, y0 - x, x + x + delta, color);
        }
        if (cornername & 0x2)
        {
            ST7735_DrawFastVLine(x0 - x, y0 - r, r + r + delta, color);
            ST7735_DrawFastVLine(x0 - r, y0 - x, x + x + delta, color);
        }
    }
}

void ST7735_DrawCircleFill(int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
    ST7735_DrawFastVLine(x0, y0 - r, r + r + 1, color);
    ST7735_FillCircleHelper(x0, y0, r, 3, 0, color);
}

void ST7735_DrawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    ST7735_DrawFastHLine(x, y, w, color);
    ST7735_DrawFastHLine(x, y + h - 1, w, color);
    ST7735_DrawFastVLine(x, y, h, color);
    ST7735_DrawFastVLine(x + w - 1, y, h, color);
}

void ST7735_DrawRectRound(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color)
{
    // smarter version
    ST7735_DrawFastHLine(x + r, y, w - r - r, color);         // Top
    ST7735_DrawFastHLine(x + r, y + h - 1, w - r - r, color); // Bottom
    ST7735_DrawFastVLine(x, y + r, h - r - r, color);         // Left
    ST7735_DrawFastVLine(x + w - 1, y + r, h - r - r, color); // Right
    // draw four corners
    ST7735_DrawCircleHelper(x + r, y + r, r, 1, color);
    ST7735_DrawCircleHelper(x + r, y + h - r - 1, r, 8, color);
    ST7735_DrawCircleHelper(x + w - r - 1, y + r, r, 2, color);
    ST7735_DrawCircleHelper(x + w - r - 1, y + h - r - 1, r, 4, color);
}

void ST7735_DrawRectRoundFill(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color)
{
    // smarter version
    ST7735_DrawRectFill(x + r, y, w - r - r, h, color);

    // draw four corners
    ST7735_FillCircleHelper(x + w - r - 1, y + r, r, 1, h - r - r - 1, color);
    ST7735_FillCircleHelper(x + r, y + r, r, 2, h - r - r - 1, color);
}

#ifdef FAST_LINE
void ST7735_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{

    uint8_t data[] = {color >> 8, color & 0xFF};

    int16_t steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep)
    {
        SWAP_INT16_T(x0, y0);
        SWAP_INT16_T(x1, y1);
    }

    if (x0 > x1)
    {
        SWAP_INT16_T(x0, x1);
        SWAP_INT16_T(y0, y1);
    }

    if (x1 < 0)
        return;

    int16_t dx, dy;
    dx = x1 - x0;
    dy = abs(y1 - y0);

    int16_t err = dx / 2;
    int8_t ystep = (y0 < y1) ? 1 : (-1);

    if (steep) // y increments every iteration (y0 is x-axis, and x0 is y-axis)
    {
        if (x1 >= _height)
            x1 = _height - 1;

        for (; x0 <= x1; x0++)
        {
            if ((x0 >= 0) && (y0 >= 0) && (y0 < _width))
                break;
            err -= dy;
            if (err < 0)
            {
                err += dx;
                y0 += ystep;
            }
        }

        if (x0 > x1)
            return;

        ST7735_SetAddressWindow(x0, y0, x1, y1);
        for (; x0 <= x1; x0++)
        {

            ST7735_WriteData(data, sizeof(data));

            err -= dy;
            if (err < 0)
            {
                y0 += ystep;
                if ((y0 < 0) || (y0 >= _width))
                    break;
                err += dx;

                ST7735_SetAddressWindow(y0, x0 + 1, y0, _height);
            }
        }
    }
    else // x increments every iteration (x0 is x-axis, and y0 is y-axis)
    {
        if (x1 >= _width)
            x1 = _width - 1;

        for (; x0 <= x1; x0++)
        {
            if ((x0 >= 0) && (y0 >= 0) && (y0 < _height))
                break;
            err -= dy;
            if (err < 0)
            {
                err += dx;
                y0 += ystep;
            }
        }

        if (x0 > x1)
            return;

        ST7735_SetAddressWindow(x0, y0, _width, y0);
        for (; x0 <= x1; x0++)
        {

            ST7735_WriteData(data, sizeof(data));

            err -= dy;
            if (err < 0)
            {
                y0 += ystep;
                if ((y0 < 0) || (y0 >= _height))
                    break;
                err += dx;

                ST7735_SetAddressWindow(x0 + 1, y0, _width, y0);
            }
        }
    }
}
#else
void ST7735_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{

    int16_t steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep)
    {
        SWAP_INT16_T(x0, y0);
        SWAP_INT16_T(x1, y1);
    }

    if (x0 > x1)
    {
        SWAP_INT16_T(x0, x1);
        SWAP_INT16_T(y0, y1);
    }

    int16_t dx, dy;
    dx = x1 - x0;
    dy = abs(y1 - y0);

    int16_t err = dx / 2;
    int16_t ystep;

    if (y0 < y1)
    {
        ystep = 1;
    }
    else
    {
        ystep = -1;
    }

    for (; x0 <= x1; x0++)
    {
        if (steep)
        {
            ST7735_DrawPixel(y0, x0, color);
        }
        else
        {
            ST7735_DrawPixel(x0, y0, color);
        }
        err -= dy;
        if (err < 0)
        {
            y0 += ystep;
            err += dx;
        }
    }
}
#endif


void ST7735_SetRotation(uint8_t rotation)
{
    ST7735_WriteCommand(ST7735_MADCTL);

    switch (rotation % 4)
    {
    case 0:
    {
        uint8_t d_r = (_data_rotation[0] | _data_rotation[1] | _data_rotation[3]);
        ST7735_WriteData(&d_r, sizeof(d_r));
        _width = ST7735_WIDTH;
        _height = ST7735_HEIGHT;
        _xstart = ST7735_XSTART;
        _ystart = ST7735_YSTART;
    }
    break;
    case 1:
    {
        uint8_t d_r =(_data_rotation[1] | _data_rotation[2] | _data_rotation[3]);
        ST7735_WriteData(&d_r, sizeof(d_r));
        _width = ST7735_HEIGHT;
        _height = ST7735_WIDTH;
        _xstart = ST7735_YSTART;
        _ystart = ST7735_XSTART;
    }
    break;
    case 2:
    {
        uint8_t d_r = _data_rotation[3];
        ST7735_WriteData(&d_r, sizeof(d_r));
        _width = ST7735_WIDTH;
        _height = ST7735_HEIGHT;
        _xstart = ST7735_XSTART;
        _ystart = ST7735_YSTART;
    }
    break;
    case 3:
    {
        uint8_t d_r = (_data_rotation[0] | _data_rotation[2] | _data_rotation[3]);
        ST7735_WriteData(&d_r, sizeof(d_r));
        _width = ST7735_HEIGHT;
        _height = ST7735_WIDTH;
        _xstart = ST7735_YSTART;
        _ystart = ST7735_XSTART;
    }
    break;
    }
}