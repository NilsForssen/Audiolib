#include "Audiolib.h"
#include "Peripherals.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "u8g2.h"
#include "u8g2_esp32_hal.h"
#include "BT_font.c"

enum StatusBM {
    BT = 1,
    PLAY = 2,
    STOP = 4,
    PAUSE = 8
};

u8g2_t u8g2;
u8g2_esp32_hal_t u8g2_esp32_hal;

char* titleText = (char*) "";
char* artistText = (char*) "";

int displayStatus = 0;
int SCREENHEIGHT;
int SCREENWIDTH;

void update_display(al_event_cb_t, al_event_cb_param_t*);
void draw();


Audiolib Audiosource = Audiolib("HESA FREDRIK", &update_display);

//void updateFilter(const uint8_t, float*, float*, CombinedChannelFilter*);
Potentiometer* pot1 = new Potentiometer(ADC_UNIT_2, ADC_CHANNEL_5, 70, 1010); //65/66 -> 572/573 -> 1019
//Potentiometer* pot2 = new Potentiometer(ADC1_CHANNEL_4, 41, 988);
//Potentiometer* pot3 = new Potentiometer(ADC1_CHANNEL_6, 38, 988);
//Potentiometer* pot4 = new Potentiometer(ADC1_CHANNEL_7, 29, 990);

CombinedChannelFilter* highshelf_filter = new CombinedChannelFilter(new Filter(HIGHSHELF, 2000, 44100, 0.8, 0), new Filter(HIGHSHELF, 2000, 44100, 0.8, 0));
CombinedChannelFilter* lowshelf_filter = new CombinedChannelFilter(new Filter(LOWSHELF, 250, 44100, 0.8, 0), new Filter(LOWSHELF, 250, 44100, 0.8, 0));
CombinedChannelFilter* highpass_filter = new CombinedChannelFilter(new Filter(LOWPASS, 8000, 44100, 0.75, 0), new Filter(LOWPASS, 8000, 44100, 0.75, 0));
CombinedChannelFilter* lowpass_filter = new CombinedChannelFilter(new Filter(HIGHPASS, 60, 44100, 0.75, 0), new Filter(HIGHPASS, 60, 44100, 0.75, 0));
CombinedChannelFilter* peak_filter = new CombinedChannelFilter(new Filter(PEAK, 700, 44100, 0.8, 0), new Filter(PEAK, 700, 44100, 0.8, 0));

extern "C" {
    void app_main(void){

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

        SCREENWIDTH = u8g2_GetDisplayWidth(&u8g2);
        SCREENHEIGHT = u8g2_GetDisplayHeight(&u8g2);

        Audiosource.set_I2S(26, 27, 25);

        Audiosource.add_combined_filter(highpass_filter);
        Audiosource.add_combined_filter(lowshelf_filter);  
        Audiosource.add_combined_filter(peak_filter); 
        Audiosource.add_combined_filter(highshelf_filter);
        Audiosource.add_combined_filter(lowpass_filter);
        Audiosource.start();

        while (true) {
            draw();
            //printf("%d\n", pot1->get_raw());
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void draw() {
    if (displayStatus & BT) {
        u8g2_DrawGlyph(&u8g2, 0, SCREENHEIGHT/2, 127);
    }
    if (displayStatus & STOP) {
        u8g2_DrawGlyph(&u8g2, 12, SCREENHEIGHT/2, 129);
    }
    else if (displayStatus & PAUSE) {
        u8g2_DrawGlyph(&u8g2, 12, SCREENHEIGHT/2, 130);
    }
    else if (displayStatus & PLAY) {
        u8g2_DrawGlyph(&u8g2, 12, SCREENHEIGHT/2, 128);
    }
    if ((displayStatus & PLAY) || (displayStatus & STOP) || (displayStatus & PAUSE)) {
        u8g2_DrawStr(&u8g2, 23, SCREENHEIGHT/2, artistText);
        u8g2_DrawStr(&u8g2, 0, SCREENHEIGHT, titleText);
    }
    else {
        u8g2_DrawStr(&u8g2, 0, SCREENHEIGHT/2, "Disconnected!");
        u8g2_DrawStr(&u8g2, 0, SCREENHEIGHT, "Connect Device!");  
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
        printf("AL, Meta_Update\n");
        break;
    }
}