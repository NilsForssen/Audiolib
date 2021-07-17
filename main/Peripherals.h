#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_adc_cal.h"

#ifndef Peripherals_H
#define Peripherals_H

#define SAMPLES 64
#define VREF 1100
#define AVERAGESUM 5

typedef void(*update_callback_t)(float);

class Potentiometer {
public:
    adc_unit_t adc_unit;
    adc_channel_t adc_channel;
    int min_raw;
    int max_raw;
    update_callback_t on_change;
    int prev_raw;
    Potentiometer(const int, const int, update_callback_t = NULL);
    Potentiometer(const adc_unit_t, const adc_channel_t, const int, const int, update_callback_t = NULL);
    int get_raw();
    int update();
    float get_percent();

private:

    static bool init_adc(const adc_unit_t, const adc_channel_t);
    static void check_efuse();
    static void print_char_val_type(esp_adc_cal_value_t);


    
};


class Button {};


#endif