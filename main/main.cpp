#include "Audiolib.h"
#include "Peripherals.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <cmath>


//void updateFilter(const uint8_t, float*, float*, CombinedChannelFilter*);
Potentiometer* pot1 = new Potentiometer(ADC_UNIT_2, ADC_CHANNEL_5, 70, 1010); //65/66 -> 572/573 -> 1019
//Potentiometer* pot2 = new Potentiometer(ADC1_CHANNEL_4, 41, 988);
//Potentiometer* pot3 = new Potentiometer(ADC1_CHANNEL_6, 38, 988);
//Potentiometer* pot4 = new Potentiometer(ADC1_CHANNEL_7, 29, 990);

int sample_pot_percent(const adc1_channel_t*, float*);

Audiolib Audiosource = Audiolib("Bluetooth Speaker");

CombinedChannelFilter* highshelf_filter = new CombinedChannelFilter(new Filter(HIGHSHELF, 2000, 44100, 0.8, 0), new Filter(HIGHSHELF, 2000, 44100, 0.8, 0));
CombinedChannelFilter* lowshelf_filter = new CombinedChannelFilter(new Filter(LOWSHELF, 250, 44100, 0.8, 0), new Filter(LOWSHELF, 250, 44100, 0.8, 0));
CombinedChannelFilter* highpass_filter = new CombinedChannelFilter(new Filter(LOWPASS, 8000, 44100, 0.75, 0), new Filter(LOWPASS, 8000, 44100, 0.75, 0));
CombinedChannelFilter* lowpass_filter = new CombinedChannelFilter(new Filter(HIGHPASS, 60, 44100, 0.75, 0), new Filter(HIGHPASS, 60, 44100, 0.75, 0));
CombinedChannelFilter* peak_filter = new CombinedChannelFilter(new Filter(PEAK, 700, 44100, 0.8, 0), new Filter(PEAK, 700, 44100, 0.8, 0));

extern "C" {
    void app_main(void){
        Audiosource.set_I2S(26, 27, 25);

        Audiosource.add_combined_filter(highpass_filter);
        Audiosource.add_combined_filter(lowshelf_filter);  
        Audiosource.add_combined_filter(peak_filter); 
        Audiosource.add_combined_filter(highshelf_filter);
        Audiosource.add_combined_filter(lowpass_filter);
        Audiosource.start();
    
        while (true) {
            //printf("%d\n", pot1->update());
            printf("%d\n", pot1->update());
            printf("%f\n", pot1->get_percent());
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}


int sample_pot_percent(const adc1_channel_t* adc_channel, float* pot_val) {
    float adc_val = 0;
    for (int i = 0; i < 100; i++) {
        adc_val += adc1_get_raw(*adc_channel);
    }
    adc_val = adc_val/100;
    if (*pot_val == 0) {
        *pot_val = adc_val;
    }
    *pot_val = (adc_val + (9*(*pot_val)) ) / 10;

    printf("%f\n", round(*pot_val) );
    return 1;
}

// void setup()
// {
//     pinMode(BUTTON1, INPUT);
//     pinMode(BUTTON2, INPUT);
//     pinMode(BUTTON3, INPUT);

//     analogReadResolution(12);
//     Serial.begin(115200);
//     Audiosource.set_I2S(26, 27, 25);

//     Audiosource.add_combined_filter(highpass_filter);
//     Audiosource.add_combined_filter(lowshelf_filter);  
//     Audiosource.add_combined_filter(peak_filter); 
//     Audiosource.add_combined_filter(highshelf_filter);
//     Audiosource.add_combined_filter(lowpass_filter);

//     Audiosource.start();
// }

// void loop()
// {
//     currentMillis = millis();
//     if (currentMillis - previousPotMillis >= potPollInterval)
//     {
//         previousPotMillis = currentMillis;
//         updateFilter(POT1, &pot1val, &filter1gain, highshelf_filter);
//         updateFilter(POT2, &pot2val, &filter2gain, peak_filter);
//         updateFilter(POT3, &pot3val, &filter3gain, lowshelf_filter);
//     }
//     currentMillis = millis();
//     if (currentMillis - previousButtonMillis >= buttonPollInterval)
//     {
//         previousButtonMillis = currentMillis;
//         if (digitalRead(BUTTON1))
//         {
//             if (!prevButton1)
//             {
//                 printf("Start filter\n"); // Button 1
//                 prevButton1 = true;
//                 Audiosource.start_filter();
//             }
//         }
//         else
//         {
//             prevButton1 = false;
//         }
//         if (digitalRead(BUTTON2))
//         {
//             if (!prevButton2)
//             {
//                 printf("Stop filter\n"); // Button 2
//                 prevButton2 = true;
//                 Audiosource.stop_filter();
//             }
//         }
//         else
//         {
//             prevButton2 = false;
//         }
//         if (digitalRead(BUTTON3))
//         {
//             if (!prevButton3)
//             {
//                 printf("Button3\n"); // Button 3
//                 Audiosource.pause();
//                 printf("%s, %s, %s\n", Audiosource.title, Audiosource.artist, Audiosource.album);
//                 prevButton3 = true;
//             }
//         }
//         else
//         {
//             prevButton3 = false;
//         }
//     }
// }

// void updateFilter(const adc_channel_t adc_channel, float* potVal, float* prevGain, CombinedChannelFilter* filter)
// {   
//     *potVal = ((float(adc1_get_raw(adc_channel_1) adc_channel) * 20.00002 / 4095) - 10.00001 + (4*(*potVal))) / 5;

//     printf

//     if (!int(((fabs(*potVal)) * 2) + 0.75) * ((*potVal > 0) - (*potVal < 0))){
//         if (*prevGain != 0){
//             *prevGain = 0;
//             filter->update(*prevGain);
//         }
//         return;
//     }
//     float diff = fabs(*potVal) - fabs(*prevGain);
//     if (diff >= gainStep || diff <= -gainStep) {
//         *prevGain = float(int(((fabs(*potVal)) * 2) + 0.5)) / 2 * ((*potVal > 0) - (*potVal < 0));
//         filter->update(*prevGain);
//         return;
//     }
// }
