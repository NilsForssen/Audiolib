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

#define TIMER_DIVIDER 16
#define TIMER_SCALE (TIMER_BASE_CLK / TIMER_DIVIDER)
#define INPUT_TIMER TIMER_0
#define DISPLAY_TIMER TIMER_1

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define INPUT_INTERVAL_S 0.01
#define DISPLAY_INTERVAL_S 0.09

#define POT1_CHANNEL ADC_CHANNEL_5
#define POT2_CHANNEL ADC_CHANNEL_6
#define POT3_CHANNEL ADC_CHANNEL_7
#define POT4_CHANNEL ADC_CHANNEL_8

#define BTN1_PIN GPIO_NUM_4
#define BTN2_PIN GPIO_NUM_2
#define BTN3_PIN GPIO_NUM_15

// -------------- Flags --------------
enum StatusBM {
    BT = 1,
    PLAY = 2,
    STOP = 4,
    PAUSE = 8
};
enum eventBM {
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
xQueueHandle event_queue;

// -------------- Global tracking vars --------------
char* titleText = (char*) "";
char* artistText = (char*) "";

int8_t displayStatus = 0;
int8_t event = 0;
int x = 0;
// --------------------------------------------------

// -------------- Function definitions --------------
void update_display(al_event_cb_t, al_event_cb_param_t*);
void IRAM_ATTR draw();
bool IRAM_ATTR input_timer_cb(void* args);
bool IRAM_ATTR display_timer_cb(void* args);
// --------------------------------------------------

// -------------- Object definitions --------------
Audiolib Audiosource = Audiolib("HESA FREDRIK", &update_display);

Potentiometer* pot1 = new Potentiometer(ADC_UNIT_2, POT1_CHANNEL, 70, 1010); //65/66 -> 572/573 -> 1019
//Potentiometer* pot2 = new Potentiometer(ADC1_CHANNEL_4, 41, 988);
//Potentiometer* pot3 = new Potentiometer(ADC1_CHANNEL_6, 38, 988);
//Potentiometer* pot4 = new Potentiometer(ADC1_CHANNEL_7, 29, 990);

Button* btn1 = new Button(BTN1_PIN);
Button* btn2 = new Button(BTN2_PIN);
Button* btn3 = new Button(BTN3_PIN);

CombinedChannelFilter* highshelf_filter = new CombinedChannelFilter(new Filter(HIGHSHELF, 2000, 44100, 0.8, 0), new Filter(HIGHSHELF, 2000, 44100, 0.8, 0));
CombinedChannelFilter* lowshelf_filter = new CombinedChannelFilter(new Filter(LOWSHELF, 250, 44100, 0.8, 0), new Filter(LOWSHELF, 250, 44100, 0.8, 0));
CombinedChannelFilter* highpass_filter = new CombinedChannelFilter(new Filter(LOWPASS, 8000, 44100, 0.75, 0), new Filter(LOWPASS, 8000, 44100, 0.75, 0));
CombinedChannelFilter* lowpass_filter = new CombinedChannelFilter(new Filter(HIGHPASS, 60, 44100, 0.75, 0), new Filter(HIGHPASS, 60, 44100, 0.75, 0));
CombinedChannelFilter* peak_filter = new CombinedChannelFilter(new Filter(PEAK, 700, 44100, 0.8, 0), new Filter(PEAK, 700, 44100, 0.8, 0));
// ------------------------------------------------

extern "C" {
    void app_main(void){

        // -------------- U8G2 setup --------------
        u8g2_esp32_hal.sda = GPIO_NUM_32;
        u8g2_esp32_hal.scl = GPIO_NUM_33;
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

        event_queue = xQueueCreate(10, 1);

        // -------------- Audiosource setup --------------
        Audiosource.set_I2S(26, 27, 25);
        Audiosource.add_combined_filter(highpass_filter);
        Audiosource.add_combined_filter(lowshelf_filter);  
        Audiosource.add_combined_filter(peak_filter); 
        Audiosource.add_combined_filter(highshelf_filter);
        Audiosource.add_combined_filter(lowpass_filter);
        Audiosource.start();
        // -----------------------------------------------

        // -------------- Timer setup --------------
        timer_init(TIMER_GROUP_0, INPUT_TIMER, &timer_config);
        timer_set_counter_value(TIMER_GROUP_0, INPUT_TIMER, 0);
        timer_set_alarm_value(TIMER_GROUP_0, INPUT_TIMER, INPUT_INTERVAL_S * TIMER_SCALE);
        timer_enable_intr(TIMER_GROUP_0, INPUT_TIMER);
        timer_isr_callback_add(TIMER_GROUP_0, INPUT_TIMER, &input_timer_cb, nullptr, 0);
    
        timer_init(TIMER_GROUP_0, DISPLAY_TIMER, &timer_config);
        timer_set_counter_value(TIMER_GROUP_0, DISPLAY_TIMER, 0);
        timer_set_alarm_value(TIMER_GROUP_0, DISPLAY_TIMER, DISPLAY_INTERVAL_S * TIMER_SCALE);
        timer_enable_intr(TIMER_GROUP_0, DISPLAY_TIMER);
        timer_isr_callback_add(TIMER_GROUP_0, DISPLAY_TIMER, &display_timer_cb, nullptr, 0);

        timer_start(TIMER_GROUP_0, INPUT_TIMER);
        timer_start(TIMER_GROUP_0, DISPLAY_TIMER);
        // -----------------------------------------

        // Main loop
        while (true) {
            xQueueReceive(event_queue, &event, portMAX_DELAY);
            if (event & DISPLAY) {
                draw();
            }
            if (event & INPUT) {
                if (btn1->getPressedSingle()){
                    Audiosource.previous();
                }
                if (btn2->getPressedSingle()) {
                    if (displayStatus & PLAY) {
                        Audiosource.pause();
                    }
                    else if (displayStatus & (PAUSE | STOP)){
                        Audiosource.play();
                    }
                }
                if (btn3->getPressedSingle()) {
                    Audiosource.next();
                }
            }
        }
    }
}

void draw() {
    if (displayStatus & BT) {
        u8g2_DrawGlyph(&u8g2, 0, SCREEN_HEIGHT/2, 127);
    }
    if (displayStatus & STOP) {
        u8g2_DrawGlyph(&u8g2, 12, SCREEN_HEIGHT/2, 129);
    }
    else if (displayStatus & PAUSE) {
        u8g2_DrawGlyph(&u8g2, 12, SCREEN_HEIGHT/2, 130);
    }
    else if (displayStatus & PLAY) {
        u8g2_DrawGlyph(&u8g2, 12, SCREEN_HEIGHT/2, 128);
    }
    if ((displayStatus & PLAY) || (displayStatus & STOP) || (displayStatus & PAUSE)) {
        u8g2_DrawStr(&u8g2, 23, SCREEN_HEIGHT/2, artistText);
        u8g2_DrawStr(&u8g2, x, SCREEN_HEIGHT, titleText);
        x += 1;
        if (x == SCREEN_WIDTH) x = 0;
    }
    else {
        u8g2_DrawStr(&u8g2, 0, SCREEN_HEIGHT/2, "Disconnected!");
        u8g2_DrawStr(&u8g2, 0, SCREEN_HEIGHT, "Connect Device!");  
    }
    u8g2_SendBuffer(&u8g2);
    u8g2_ClearBuffer(&u8g2);
}

void update_display(al_event_cb_t event, al_event_cb_param_t* param) {
    switch (event) {
    case AL_CONNECTED:
        displayStatus = BT | STOP;
        printf("AL, Connected\n");
        break;
    
    case AL_DISCONNECTED:
        displayStatus = 0;
        printf("AL, Disconnected\n");
        break;
    
    case AL_PLAYING:
        displayStatus = BT | PLAY;
        printf("AL, Playing\n");
        break;

    case AL_PAUSED:
        displayStatus = BT | PAUSE;
        printf("AL, Paused\n");
        break;

    case AL_STOPPED:
        displayStatus = BT | STOP;
        printf("AL, Stopped\n");
        break;

    case AL_META_UPDATE:
        titleText = param->metadata.title;
        artistText = param->metadata.artist;
        if ((*artistText == '\0') || (*titleText == '\0')) {
            displayStatus = BT | STOP;
        }
        x = 0;
        printf("AL, Meta_Update\n");
        break;
    }
}

bool input_timer_cb(void* args) {
    int8_t msg = INPUT;
    BaseType_t high_task_awoken = pdFALSE;
    xQueueSendFromISR(event_queue, &msg, &high_task_awoken);
    return high_task_awoken == pdTRUE;
}

bool display_timer_cb(void* args) {
    int8_t msg = DISPLAY;
    BaseType_t high_task_awoken = pdFALSE;
    xQueueSendFromISR(event_queue, &msg, &high_task_awoken);
    return high_task_awoken == pdTRUE;
}