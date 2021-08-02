#include "Audiolib.h"
#include "Peripherals.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "u8g2.h"
#include "u8g2_esp32_hal.h"
#include "cstring"

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

u8g2_t u8g2;
u8g2_uint_t displayHeight = u8g2_GetDisplayHeight(&u8g2);
u8g2_uint_t displayWidth = u8g2_GetDisplayWidth(&u8g2);

u8g2_esp32_hal_t u8g2_esp32_hal;

struct timeval tv_now;

static int x;
static char* text;
static int textWidth;
static bool scrolling = false;
static bool connected = false;

void scrollText(char*);
void update_display(al_event_cb_t, al_event_cb_param_t*);
void draw();

Audiolib Audiosource = Audiolib("Titta vad jag heter", &update_display);

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
        u8g2_SetFont(&u8g2, u8g2_font_ncenR24_tf);


        Audiosource.set_I2S(26, 27, 25);

        Audiosource.add_combined_filter(highpass_filter);
        Audiosource.add_combined_filter(lowshelf_filter);  
        Audiosource.add_combined_filter(peak_filter); 
        Audiosource.add_combined_filter(highshelf_filter);
        Audiosource.add_combined_filter(lowpass_filter);
        Audiosource.start();
    
        while (true) {
            while (connected) {





            }
            draw();
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void draw() {
    u8g2_ClearBuffer(&u8g2);
    u8g2_DrawStr(&u8g2, x, 32, text);
    u8g2_SendBuffer(&u8g2);
}

void scrollText(char* text) {
    u8g2_ClearBuffer(&u8g2);
    u8g2_DrawStr(&u8g2, x, 32, text);
    u8g2_SendBuffer(&u8g2);
    if (x == 0) {
        vTaskDelay(pdMS_TO_TICKS(990));
    }
    
    if ((textWidth + x) > u8g2_GetDisplayWidth(&u8g2)) {
        x -= 1; 
    }
    
    else {
        x = 0;
        vTaskDelay(pdMS_TO_TICKS(990));
    }
    vTaskDelay(pdMS_TO_TICKS(10));
}

void update_display(al_event_cb_t event, al_event_cb_param_t* param) {
    x = 0;
    switch (event) {
    case AL_CONNECTED:
        scrolling = true;
        text = (char*) "Connected!";
        printf("AL, Connected\n");
        connected = true;
        break;
    
    case AL_DISCONNECTED:
        scrolling = true;
        text = (char*) "Disconnected!";
        printf("AL, Disconnected\n");
        break;
    
    case AL_PLAYING:
        scrolling = false;
        text = (char*) "PLAY";
        printf("AL, Playing\n");
        break;

    case AL_PAUSED:
        scrolling = false;
        text = (char*) "PAUSE";
        printf("AL, Paused\n");
        break;

    case AL_STOPPED:
        printf("AL, Stopped\n");
        break;

    case AL_META_UPDATE:
        scrolling = true;

        int titleLen = strlen(param->metadata.title);
        int artistLen = strlen(param->metadata.artist);

        text = (char*) malloc(titleLen + artistLen + 3 + 1);        //Maybe runaway pointer every time this is done?
        memcpy(text, param->metadata.title, titleLen);
        memcpy(text + titleLen, (char*)" - ", 3);
        memcpy(text + titleLen + 3, param->metadata.artist, artistLen);
        *(text + titleLen + 3 + artistLen) = *"\0";

        textWidth = u8g2_GetUTF8Width(&u8g2, text);
        printf("%d\n", textWidth);
        printf("%s\n", text);

        printf("AL, Meta_Update\n");
        break;
    }
}


/*
int substr(char* string, int startPixel, int totPixels) {

    int len = strlen(strin);
    int newlen = (int) len/2;

    char* newstr = (char*) malloc(newlen + 1);
    memcpy(newstr, &str[start], newlen);
    newstr[newlen] = *"\0";


    if (pixels > u8g2_GetUTF8Width((int) len/2)
    for (int i = 0; i < strlen(string))
    
}*/