#include "Audiolib.h"
#include "Peripherals.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/timer.h"
#include "u8g2.h"
#include "u8g2_esp32_hal.h"
#include "BT_font.c"
#include <algorithm>

#define TIMER_DIVIDER 16
#define TIMER_SCALE (TIMER_BASE_CLK / TIMER_DIVIDER)
#define INPUT_TIMER TIMER_0
#define DISPLAY_TIMER TIMER_1

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define INPUT_INTERVAL_S 0.01
#define DISPLAY_INTERVAL_S 0.05

#define POT1_CHANNEL ADC_CHANNEL_5
#define POT2_CHANNEL ADC_CHANNEL_6
#define POT3_CHANNEL ADC_CHANNEL_7
#define POT4_CHANNEL ADC_CHANNEL_8

#define BTN1_PIN GPIO_NUM_25
#define BTN2_PIN GPIO_NUM_26
#define BTN3_PIN GPIO_NUM_27

#define DISPLAY_SDA_PIN GPIO_NUM_21
#define DISPLAY_SCL_PIN GPIO_NUM_22

#define EXT_DAC_BCLK_PIN GPIO_NUM_16
#define EXT_DAC_WSEL_PIN GPIO_NUM_5
#define EXT_DAC_DATA_PIN GPIO_NUM_17

// -------------- Flags --------------
enum StatusBM
{
    BT = 1,
    PLAY = 2,
    STOP = 4,
    PAUSE = 8,
    METADATA = 16
};
enum eventBM
{
    DISPLAY = 1,
    INPUT = 2,
};
// -----------------------------------

// -------------- Configs --------------
timer_config_t timer_config = {
    .alarm_en = TIMER_ALARM_EN,
    .counter_en = TIMER_PAUSE,
    .intr_type = TIMER_INTR_LEVEL,
    .counter_dir = TIMER_COUNT_UP,
    .auto_reload = TIMER_AUTORELOAD_EN,
    .divider = TIMER_DIVIDER,
};
// -------------------------------------

u8g2_t u8g2;
u8g2_esp32_hal_t u8g2_esp32_hal;

// -------------- Global tracking vars --------------
char *titleText = (char *)"";
char *artistText = (char *)"";
uint8_t titleWidth = 0;
uint8_t artistWidth = 0;

uint8_t displayStatus = 0;
uint8_t event = 0;
int16_t title_x = 0;
int16_t artist_x = 0;
// --------------------------------------------------

// -------------- Function definitions --------------
void update_display(al_event_cb_t, al_event_cb_param_t *);
void IRAM_ATTR draw();
bool IRAM_ATTR input_timer_cb(void *args);
bool IRAM_ATTR display_timer_cb(void *args);

void pot_volume_update_cb(float);
void pot_treb_update_cb(float);
void pot_mid_update_cb(float);
void pot_bass_update_cb(float);

void btn1_update_cb(void);
void btn2_update_cb(void);
void btn3_update_cb(void);
// --------------------------------------------------

// -------------- Object definitions --------------
Audiolib Audiosource = Audiolib("HESA PETTER", &update_display);

Potentiometer *pot_volume = new Potentiometer(ADC_UNIT_1, ADC_CHANNEL_0, 1, &pot_volume_update_cb);
Potentiometer *pot_treb = new Potentiometer(ADC_UNIT_1, ADC_CHANNEL_0, 1, &pot_treb_update_cb);
Potentiometer *pot_mid = new Potentiometer(ADC_UNIT_1, ADC_CHANNEL_3, 1, &pot_mid_update_cb);
Potentiometer *pot_bass = new Potentiometer(ADC_UNIT_1, ADC_CHANNEL_6, 1, &pot_bass_update_cb);

Button *btn1 = new Button(BTN1_PIN, &btn1_update_cb);
Button *btn2 = new Button(BTN2_PIN, &btn2_update_cb);
Button *btn3 = new Button(BTN3_PIN, &btn3_update_cb);

CombinedChannelFilter *highshelf_filter = new CombinedChannelFilter(new Filter(HIGHSHELF, 2000, 44100, 0.8, 0), new Filter(HIGHSHELF, 2000, 44100, 0.8, 0));
CombinedChannelFilter *lowshelf_filter = new CombinedChannelFilter(new Filter(LOWSHELF, 250, 44100, 0.8, 0), new Filter(LOWSHELF, 250, 44100, 0.8, 0));
CombinedChannelFilter* highpass_filter = new CombinedChannelFilter(new Filter(LOWPASS, 10000, 44100, 0.75, 0), new Filter(LOWPASS, 10000, 44100, 0.75, 0));
CombinedChannelFilter* lowpass_filter = new CombinedChannelFilter(new Filter(HIGHPASS, 50, 44100, 0.75, 0), new Filter(HIGHPASS, 50, 44100, 0.75, 0));
CombinedChannelFilter *peak_filter = new CombinedChannelFilter(new Filter(PEAK, 700, 44100, 0.8, 0), new Filter(PEAK, 700, 44100, 0.8, 0));
// ------------------------------------------------

extern "C"
{
    void app_main(void)
    {

        // -------------- U8G2 setup --------------
        u8g2_esp32_hal.sda = DISPLAY_SDA_PIN;
        u8g2_esp32_hal.scl = DISPLAY_SCL_PIN;
        u8g2_esp32_hal_init(u8g2_esp32_hal);
        u8g2_Setup_ssd1306_i2c_128x32_univision_f(
            &u8g2,
            U8G2_R0,
            u8g2_esp32_i2c_byte_cb,
            u8g2_esp32_gpio_and_delay_cb);
        u8x8_SetI2CAddress(&u8g2.u8x8, 0x3C);
        u8g2_InitDisplay(&u8g2);
        u8g2_SetPowerSave(&u8g2, 0);
        u8g2_SetFont(&u8g2, BT_FONT);
        // ----------------------------------------

        // -------------- Audiosource setup --------------
        Audiosource.set_I2S(EXT_DAC_BCLK_PIN, EXT_DAC_WSEL_PIN, EXT_DAC_DATA_PIN);
        Audiosource.add_combined_filter(highpass_filter);
        Audiosource.add_combined_filter(lowshelf_filter);
        Audiosource.add_combined_filter(peak_filter);
        Audiosource.add_combined_filter(highshelf_filter);
        Audiosource.add_combined_filter(lowpass_filter);
        Audiosource.start();
        // -----------------------------------------------

        // Main loop
        while (true)
        {
            draw();

            pot_volume->update();
            pot_treb->update();
            pot_mid->update();
            pot_bass->update();

            btn1->update();
            btn2->update();
            btn3->update();

            vTaskDelay(1);
        }
    }
}

void draw()
{
    if (displayStatus & BT)
    {
        u8g2_DrawGlyph(&u8g2, 0, SCREEN_HEIGHT / 2, 127);

        if (displayStatus & STOP)
        {
            u8g2_DrawGlyph(&u8g2, 12, SCREEN_HEIGHT / 2, 129);
        }
        else if (displayStatus & PAUSE)
        {
            u8g2_DrawGlyph(&u8g2, 12, SCREEN_HEIGHT / 2, 130);
        }
        else if (displayStatus & PLAY)
        {
            u8g2_DrawGlyph(&u8g2, 12, SCREEN_HEIGHT / 2, 128);
        }

        if (displayStatus & METADATA)
        {
            u8g2_DrawStr(&u8g2, 23, SCREEN_HEIGHT / 2, artistText);
            u8g2_DrawStr(&u8g2, title_x, SCREEN_HEIGHT, titleText);
            title_x -= 1;
            if (title_x + titleWidth <= SCREEN_WIDTH)
                title_x = 0;

            /*
            artist_x -= 1;
            if (artist_x + artistWidth + 23 <= SCREEN_WIDTH)
                artist_x = 0;
            */
        }
        else
        {
            u8g2_DrawStr(&u8g2, 23, SCREEN_HEIGHT / 2, "Connected!");
        }
    }
    else
    {
        u8g2_DrawStr(&u8g2, 0, SCREEN_HEIGHT / 2, "Disconnected!");
        u8g2_DrawStr(&u8g2, 0, SCREEN_HEIGHT, "Connect Device!");
    }
    u8g2_SendBuffer(&u8g2);
    u8g2_ClearBuffer(&u8g2);
}

void update_display(al_event_cb_t event, al_event_cb_param_t *param)
{
    switch (event)
    {
    case AL_CONNECTED:
        displayStatus = BT | STOP;
        printf("AL, Connected\n");
        break;

    case AL_DISCONNECTED:
        displayStatus = 0;
        printf("AL, Disconnected\n");
        break;

    case AL_PLAYING:
        displayStatus = BT | PLAY | METADATA;
        printf("AL, Playing\n");
        break;

    case AL_PAUSED:
        displayStatus = BT | PAUSE | METADATA;
        printf("AL, Paused\n");
        break;

    case AL_STOPPED:
        displayStatus = BT | STOP | METADATA;
        printf("AL, Stopped\n");
        break;

    case AL_META_UPDATE:
        titleText = param->metadata.title;
        artistText = param->metadata.artist;
        if ((*artistText == '\0') || (*titleText == '\0'))
        {
            displayStatus = BT | STOP;
        }
        else
        {
            displayStatus |= METADATA;
        }
        artistWidth = u8g2_GetStrWidth(&u8g2, artistText);
        titleWidth = u8g2_GetStrWidth(&u8g2, titleText);
        artist_x = 0;
        title_x = 0;
        printf("AL, Meta_Update\n");
        break;
    }
}

void pot_volume_update_cb(float val)
{
    printf("Volume changed %.2f\n", val);
    peak_filter->change_volume(val/100);
}
void pot_treb_update_cb(float val)
{
    printf("Treb changed %.2f\n", val);
    highshelf_filter->update((val - 50) / 4);
}
void pot_mid_update_cb(float val)
{
    printf("Mid changed %.2f\n", val);
    peak_filter->update((val-50) / 4);
}
void pot_bass_update_cb(float val)
{
    printf("Bass changed %.2f\n", val);
    lowshelf_filter->update((val-50) / 4);
}

void btn1_update_cb()
{
    Audiosource.next();
}
void btn2_update_cb()
{
    if (displayStatus & PLAY)
    {
        Audiosource.pause();
    }
    else if (displayStatus & (PAUSE | STOP))
    {
        Audiosource.play();
    }
}
void btn3_update_cb()
{
    Audiosource.previous();
}