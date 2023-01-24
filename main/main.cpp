#include "Audiolib.h"
#include "Peripherals.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/timer.h"
#include "u8g2.h"
#include "u8g2_esp32_hal.h"

#define TIMER_DIVIDER 16
#define TIMER_SCALE (TIMER_BASE_CLK / TIMER_DIVIDER)
#define INPUT_TIMER TIMER_0
#define DISPLAY_TIMER TIMER_1

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define INPUT_INTERVAL_S 0.01
#define DISPLAY_INTERVAL_S 0.05

#define POT1_CHANNEL ADC_CHANNEL_0
#define POT2_CHANNEL ADC_CHANNEL_7
#define POT3_CHANNEL ADC_CHANNEL_6
#define POT4_CHANNEL ADC_CHANNEL_3

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
    METADATA = 16,
    POT_CHANGE = 32,
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
char *title_text = (char *)"";
char *artist_text = (char *)"";
char pot_str[50];

uint8_t counter = 0;
uint8_t title_width = 0;
uint8_t artist_width = 0;

uint8_t display_status = 0;
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

void prev_btn_cb(void);
void pause_btn_cb(void);
void next_btn_cb(void);
// --------------------------------------------------

// -------------- Object definitions --------------
Audiolib Audiosource = Audiolib("HESA PETTER", &update_display);

Potentiometer *pot_volume = new Potentiometer(ADC_UNIT_1, POT1_CHANNEL, 1, &pot_volume_update_cb);
Potentiometer *pot_treb = new Potentiometer(ADC_UNIT_1, POT2_CHANNEL, 1, &pot_treb_update_cb);
Potentiometer *pot_mid = new Potentiometer(ADC_UNIT_1, POT3_CHANNEL, 1, &pot_mid_update_cb);
Potentiometer *pot_bass = new Potentiometer(ADC_UNIT_1, POT4_CHANNEL, 1, &pot_bass_update_cb);

Button *prev_btn = new Button(BTN1_PIN, &prev_btn_cb);
Button *pause_btn = new Button(BTN2_PIN, &pause_btn_cb);
Button *next_btn = new Button(BTN3_PIN, &next_btn_cb);

CombinedChannelFilter *highshelf_filter = new CombinedChannelFilter(new Filter(HIGHSHELF, 2500, 44100, 0.8, 0), new Filter(HIGHSHELF, 2000, 44100, 0.8, 0));
CombinedChannelFilter *lowshelf_filter = new CombinedChannelFilter(new Filter(LOWSHELF, 250, 44100, 0.8, 0), new Filter(LOWSHELF, 250, 44100, 0.8, 0));
//CombinedChannelFilter* highpass_filter = new CombinedChannelFilter(new Filter(LOWPASS, 10000, 44100, 0.75, 0), new Filter(LOWPASS, 10000, 44100, 0.75, 0));
//CombinedChannelFilter* lowpass_filter = new CombinedChannelFilter(new Filter(HIGHPASS, 50, 44100, 0.75, 0), new Filter(HIGHPASS, 50, 44100, 0.75, 0));
CombinedChannelFilter *peak_filter = new CombinedChannelFilter(new Filter(PEAK, 800, 44100, 0.8, 0), new Filter(PEAK, 700, 44100, 0.8, 0));
// ------------------------------------------------

extern "C"
{
    void app_main(void)
    {

        // -------------- U8G2 setup --------------
        u8g2_esp32_hal.sda = DISPLAY_SDA_PIN;
        u8g2_esp32_hal.scl = DISPLAY_SCL_PIN;
        u8g2_esp32_hal_init(u8g2_esp32_hal);
        u8g2_Setup_ssd1306_i2c_128x64_noname_f(
            &u8g2,
            U8G2_R0,
            u8g2_esp32_i2c_byte_cb,
            u8g2_esp32_gpio_and_delay_cb);
        u8x8_SetI2CAddress(&u8g2.u8x8, 0x3C);
        u8g2_InitDisplay(&u8g2);
        u8g2_SetPowerSave(&u8g2, 0);
        u8g2_SetFont(&u8g2, u8g2_font_logisoso16_tr);
        // ----------------------------------------

        // -------------- Audiosource setup --------------
        Audiosource.set_I2S(EXT_DAC_BCLK_PIN, EXT_DAC_WSEL_PIN, EXT_DAC_DATA_PIN);
        //Audiosource.add_combined_filter(highpass_filter);
        Audiosource.add_combined_filter(lowshelf_filter);
        Audiosource.add_combined_filter(peak_filter);
        Audiosource.add_combined_filter(highshelf_filter);
        //Audiosource.add_combined_filter(lowpass_filter);
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

            prev_btn->update();
            pause_btn->update();
            next_btn->update();

            vTaskDelay(1);
        }
    }
}

void draw()
{
    if (display_status & BT)
    {   
        if (display_status & POT_CHANGE && counter != 10) {
            counter += 1;
            u8g2_DrawStr(&u8g2, 2, 62, pot_str);
        }
        else {
            counter = 0;
            display_status &= ~POT_CHANGE;
            if (display_status & STOP)
            {
                u8g2_DrawStr(&u8g2, 2, 62, "STOPPED");
            }
            else if (display_status & PAUSE)
            {
                u8g2_DrawStr(&u8g2, 2, 62, "PAUSED");
            }
            else if (display_status & PLAY)
            {
                u8g2_DrawStr(&u8g2, 2, 62, "PLAYING");
            }
        }
        if (display_status & METADATA)
        {
            u8g2_DrawStr(&u8g2, 2 + artist_x, 18, artist_text);
            u8g2_DrawStr(&u8g2, 2 + title_x, 38, title_text);

            if (title_width > 123) {
                title_x -= 1;
                if (title_x + title_width  <= 123)
                    title_x = 0;
            }

            if (artist_width > 123) {
                artist_x -= 1;
                if (artist_x + artist_width <= 123)
                    artist_x = 0;
            }
            
        }
        else
        {
            u8g2_DrawStr(&u8g2, 2, 18, "Connected!");
            u8g2_DrawStr(&u8g2, 2, 38, "Waiting ...");
        }
    }
    else
    {
        u8g2_DrawStr(&u8g2, 2, 18, "Disconnected!");
        u8g2_DrawStr(&u8g2, 2, 38, "Waiting ...");
    }
    u8g2_DrawLine(&u8g2, 0, 0, 127, 0);
    u8g2_DrawLine(&u8g2, 0, 0, 0, 63);
    u8g2_DrawLine(&u8g2, 0, 63, 127, 63);
    u8g2_DrawLine(&u8g2, 127, 0, 127, 63);
    u8g2_DrawLine(&u8g2, 0, 44, 127, 44);
    u8g2_SendBuffer(&u8g2);
    u8g2_ClearBuffer(&u8g2);
}

void update_display(al_event_cb_t event, al_event_cb_param_t *param)
{
    switch (event)
    {
    case AL_CONNECTED:
        display_status = BT | STOP;
        printf("AL, Connected\n");
        break;

    case AL_DISCONNECTED:
        display_status = 0;
        printf("AL, Disconnected\n");
        break;

    case AL_PLAYING:
        display_status = BT | PLAY | METADATA;
        printf("AL, Playing\n");
        break;

    case AL_PAUSED:
        display_status = BT | PAUSE | METADATA;
        printf("AL, Paused\n");
        break;

    case AL_STOPPED:
        display_status = BT | STOP | METADATA;
        printf("AL, Stopped\n");
        
        break;

    case AL_META_UPDATE:
        title_text = param->metadata.title;
        artist_text = param->metadata.artist;
        if ((*artist_text == '\0') || (*title_text == '\0'))
        {
            display_status = BT | STOP;
        }
        else
        {
            display_status |= METADATA;
        }
        artist_width = u8g2_GetStrWidth(&u8g2, artist_text);
        title_width = u8g2_GetStrWidth(&u8g2, title_text);
        artist_x = 0;
        title_x = 0;
        printf("AL, Meta_Update\n");
        break;
    }
}

void pot_volume_update_cb(float val)
{
    sprintf(pot_str, "Volume: %.2f", val);
    display_status |= POT_CHANGE;
    peak_filter->change_volume(val/100);
    counter = 0;
}
void pot_treb_update_cb(float val)
{
    sprintf(pot_str, "Treb: %.2f", val);
    display_status |= POT_CHANGE;
    highshelf_filter->update((val - 50) / 4);
    counter = 0;
}
void pot_mid_update_cb(float val)
{
    sprintf(pot_str, "Mid: %.2f", val);
    display_status |= POT_CHANGE;
    peak_filter->update((val-50) / 4);
    counter = 0;
}
void pot_bass_update_cb(float val)
{
    sprintf(pot_str, "Bass: %.2f", val);
    display_status |= POT_CHANGE;
    lowshelf_filter->update((val-50) / 4);
    counter = 0;
}

void prev_btn_cb()
{
    Audiosource.previous();
}

void pause_btn_cb()
{
    if (display_status & PLAY)
    {
        Audiosource.pause();
    }
    else if (display_status & (PAUSE | STOP))
    {
        Audiosource.play();
    }
}

void next_btn_cb()
{
    Audiosource.next();
}
