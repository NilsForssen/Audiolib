#include "driver/adc.h"
#include "esp_adc_cal.h"

#ifndef Peripherals_H
#define Peripherals_H

#define SAMPLES 64
#define VREF 1100
#define DIFF_THRESHOLD 10

#define atten ADC_ATTEN_DB_11
#define width ADC_WIDTH_BIT_12

typedef void(*pot_update_callback_t)(float);
typedef void(*btn_update_callback_t)(void);



class Potentiometer {
public:
    Potentiometer(pot_update_callback_t = NULL);
    Potentiometer(const adc_unit_t, const adc_channel_t, float offset_fact, pot_update_callback_t = NULL);
    int get_raw();
    void update();
    float get_percent();

private:
    adc_unit_t adc_unit;
    adc_channel_t adc_channel;
    pot_update_callback_t on_change;
    float offset_fact; 
    int prev_raw;

    
    static bool init_adc(const adc_unit_t, const adc_channel_t);
    static void check_efuse();
    static void print_char_val_type(esp_adc_cal_value_t);
};

class Button {
public:
    Button(gpio_num_t, btn_update_callback_t = NULL);
    bool getPressed();
    bool getPressedSingle();
    void update();
    btn_update_callback_t on_change;
private:
    bool lastState = false;
    gpio_num_t gpio_pin;
};


#endif