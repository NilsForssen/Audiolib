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

/*
  Fontname: Untitledthisfon
  Copyright: Created with Fony 1.4.0.2
  Glyphs: 224/256
  BBX Build Mode: 0
*/
const uint8_t BT_font[939] U8G2_FONT_SECTION("BT_font") = 
  "\340\0\3\3\4\4\1\1\5\10\17\0\0\17\0\0\0\0\204\1\20\3\216 \4\0c!\4\0c\42"
  "\4\0c#\4\0c$\4\0c%\4\0c&\4\0c'\4\0c(\4\0c)\4\0c*"
  "\4\0c+\4\0c,\4\0c-\4\0c.\4\0c/\4\0c\60\4\0c\61\4\0c\62"
  "\4\0c\63\4\0c\64\4\0c\65\4\0c\66\4\0c\67\4\0c\70\4\0c\71\4\0c:"
  "\4\0c;\4\0c<\4\0c=\4\0c>\4\0c\77\4\0c@\4\0cA\20\370\343\211"
  "\245D\66\221\236,\207!\343X\34B\4\0cC\4\0cD\4\0cE\4\0cF\4\0cG"
  "\4\0cH\4\0cI\4\0cJ\4\0cK\4\0cL\4\0cM\4\0cN\4\0cO"
  "\4\0cP\4\0cQ\4\0cR\4\0cS\4\0cT\4\0cU\4\0cV\4\0cW"
  "\4\0cX\4\0cY\4\0cZ\4\0c[\4\0c\134\4\0c]\4\0c^\4\0c_"
  "\4\0c`\4\0ca\4\0cb\4\0cc\4\0cd\4\0ce\4\0cf\4\0cg"
  "\4\0ch\4\0ci\4\0cj\4\0ck\4\0cl\4\0cm\4\0cn\4\0co"
  "\4\0cp\4\0cq\4\0cr\4\0cs\4\0ct\4\0cu\4\0cv\4\0cw"
  "\4\0cx\4\0cy\4\0cz\4\0c{\4\0c|\4\0c}\4\0c~\4\0c\177"
  "\4\0c\200\4\0c\201\4\0c\202\4\0c\203\4\0c\204\4\0c\205\4\0c\206\4\0c\207"
  "\4\0c\210\4\0c\211\4\0c\212\4\0c\213\4\0c\214\4\0c\215\4\0c\216\4\0c\217"
  "\4\0c\220\4\0c\221\4\0c\222\4\0c\223\4\0c\224\4\0c\225\4\0c\226\4\0c\227"
  "\4\0c\230\4\0c\231\4\0c\232\4\0c\233\4\0c\234\4\0c\235\4\0c\236\4\0c\237"
  "\4\0c\240\4\0c\241\4\0c\242\4\0c\243\4\0c\244\4\0c\245\4\0c\246\4\0c\247"
  "\4\0c\250\4\0c\251\4\0c\252\4\0c\253\4\0c\254\4\0c\255\4\0c\256\4\0c\257"
  "\4\0c\260\4\0c\261\4\0c\262\4\0c\263\4\0c\264\4\0c\265\4\0c\266\4\0c\267"
  "\4\0c\270\4\0c\271\4\0c\272\4\0c\273\4\0c\274\4\0c\275\4\0c\276\4\0c\277"
  "\4\0c\300\4\0c\301\4\0c\302\4\0c\303\4\0c\304\4\0c\305\4\0c\306\4\0c\307"
  "\4\0c\310\4\0c\311\4\0c\312\4\0c\313\4\0c\314\4\0c\315\4\0c\316\4\0c\317"
  "\4\0c\320\4\0c\321\4\0c\322\4\0c\323\4\0c\324\4\0c\325\4\0c\326\4\0c\327"
  "\4\0c\330\4\0c\331\4\0c\332\4\0c\333\4\0c\334\4\0c\335\4\0c\336\4\0c\337"
  "\4\0c\340\4\0c\341\4\0c\342\4\0c\343\4\0c\344\4\0c\345\4\0c\346\4\0c\347"
  "\4\0c\350\4\0c\351\4\0c\352\4\0c\353\4\0c\354\4\0c\355\4\0c\356\4\0c\357"
  "\4\0c\360\4\0c\361\4\0c\362\4\0c\363\4\0c\364\4\0c\365\4\0c\366\4\0c\367"
  "\4\0c\370\4\0c\371\4\0c\372\4\0c\373\4\0c\374\4\0c\375\4\0c\376\4\0c\377"
  "\4\0c\0\0\0\4\377\377\0";

static char* text = (char*) "Disconnected!";
static int textWidth;
static bool connected = false;
static int SCREENHEIGHT;
static int SCREENWIDTH;
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
        u8g2_SetFont(&u8g2, BT_font);

        SCREENWIDTH = u8g2_GetDisplayWidth(&u8g2);
        SCREENHEIGHT = u8g2_GetDisplayHeight(&u8g2);
        textWidth = u8g2_GetStrWidth(&u8g2, text);

        Audiosource.set_I2S(26, 27, 25);

        Audiosource.add_combined_filter(highpass_filter);
        Audiosource.add_combined_filter(lowshelf_filter);  
        Audiosource.add_combined_filter(peak_filter); 
        Audiosource.add_combined_filter(highshelf_filter);
        Audiosource.add_combined_filter(lowpass_filter);
        Audiosource.start();

        while (true) {
            draw();
        }
    }
}


void draw() {
    u8g2_DrawStr(&u8g2, 0, SCREENHEIGHT, "AAAAAAAAAA");
    u8g2_SendBuffer(&u8g2);
    vTaskDelay(pdMS_TO_TICKS(10));
}

void update_display(al_event_cb_t event, al_event_cb_param_t* param) {
    switch (event) {
    case AL_CONNECTED:
        text = (char*) "Connected!";
        printf("AL, Connected\n");
        connected = true;
        break;
    
    case AL_DISCONNECTED:
        text = (char*) "Disconnected!";
        printf("AL, Disconnected\n");
        break;
    
    case AL_PLAYING:
        text = (char*) "PLAY";
        printf("AL, Playing\n");
        break;

    case AL_PAUSED:
        text = (char*) "PAUSE";
        printf("AL, Paused\n");
        break;

    case AL_STOPPED:
        printf("AL, Stopped\n");
        break;

    case AL_META_UPDATE:
        int titleLen = strlen(param->metadata.title);
        int artistLen = strlen(param->metadata.artist);

        text = (char*) malloc(titleLen + artistLen + 3 + 1);        //Maybe runaway pointer every time this is done?
        memcpy(text, param->metadata.title, titleLen);
        memcpy(text + titleLen, (char*)" - ", 3);
        memcpy(text + titleLen + 3, param->metadata.artist, artistLen);
        *(text + titleLen + 3 + artistLen) = *"\0";

        printf("%s\n", text);

        printf("AL, Meta_Update\n");
        break;
        
    }
    textWidth = u8g2_GetStrWidth(&u8g2, text);
    draw();
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