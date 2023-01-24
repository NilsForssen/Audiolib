#ifndef Peripherals_H
#define Peripherals_H

#include "driver/adc.h"
#include "esp_adc_cal.h"



#define SAMPLES 64
#define VREF 1100
#define DIFF_THRESHOLD 50

#define ATTEN ADC_ATTEN_DB_11
#define WIDTH ADC_WIDTH_BIT_12

typedef void(*pot_update_callback_t)(float);
typedef void(*btn_update_callback_t)(void);

static bool check_efuse() {
    //Check if TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("eFuse Two Point: NOT supported\n");
    }
    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        printf("eFuse Vref: Supported\n");
    } else {
        printf("eFuse Vref: NOT supported\n");
    }
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("Cannot retrieve eFuse Two Point calibration values. Default calibration values will be used.\n");
    }
    printf("REMOVETHISFLAG");
    return true;
}
static bool print_char_val_type(esp_adc_cal_value_t val_type) {
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("Characterized using eFuse Vref\n");
    } else {
        printf("Characterized using Default Vref\n");
    }
    printf("REMOVETHISFLAG");
    return true;
}
static bool init_adc(const adc_unit_t unit, const adc_channel_t channel) {
    static bool some_check = check_efuse();

    //Configure ADC
    if (unit == ADC_UNIT_1) {
        adc1_config_width(WIDTH);
        adc1_config_channel_atten((adc1_channel_t)channel, ATTEN);
    } else {
        adc2_config_channel_atten((adc2_channel_t)channel, ATTEN);
    }
    
    //Characterize ADC
    static esp_adc_cal_characteristics_t* adc_chars = (esp_adc_cal_characteristics_t*) calloc(1, sizeof(esp_adc_cal_characteristics_t));
    static esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, ATTEN, WIDTH, VREF, adc_chars);
    static bool some_print = print_char_val_type(val_type);
    return true;
}

class Potentiometer {
public:
    Potentiometer(pot_update_callback_t = NULL);
    Potentiometer(const adc_unit_t, const adc_channel_t, float, pot_update_callback_t = NULL);
    int get_raw();
    void update();
    float get_percent();

private:
    adc_unit_t adc_unit;
    adc_channel_t adc_channel;
    pot_update_callback_t on_change;
    float offset_fact; 
    int prev_raw;
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

class TogglePIN {
public:
    TogglePIN(gpio_num_t);
    bool get();
    void set(bool);
private:
    gpio_num_t gpio_pin;
};

class BatteryMonitor {
public:
    BatteryMonitor(const adc_unit_t, const adc_channel_t, float);
    int get_raw();
    float get_percentage();
private:
    adc_unit_t adc_unit;
    adc_channel_t adc_channel;
    float zero;
};
#endif