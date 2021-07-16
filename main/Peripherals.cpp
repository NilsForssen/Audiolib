#include "Peripherals.h"

static const adc_atten_t atten = ADC_ATTEN_DB_0;
static const adc_bits_width_t width = ADC_WIDTH_BIT_12;

Potentiometer::Potentiometer(const int min, const int max, update_callback_t update_ntfy)
    : Potentiometer(ADC_UNIT_1, ADC_CHANNEL_1, min, max, update_ntfy) {}

Potentiometer::Potentiometer(const adc_unit_t unit, const adc_channel_t channel, const int min, const int max, udpate_callback_t update_ntfy) 
    : adc_unit(unit), 
    adc_channel(channel), 
    min_raw(min), 
    max_raw(max),
    on_change(update_ntfy),
    prev_raw(0) {

    static bool adc_first_init = init_adc(unit, channel);
}

bool Potentiometer::init_adc(const adc_unit_t unit, const adc_channel_t channel) {
    check_efuse();
    //Configure ADC
    if (unit == ADC_UNIT_1) {
        adc1_config_width(width);
        adc1_config_channel_atten((adc1_channel_t)channel, atten);
    } else {
        adc2_config_channel_atten((adc2_channel_t)channel, atten);
    }
    
    //Characterize ADC
    esp_adc_cal_characteristics_t* adc_chars = (esp_adc_cal_characteristics_t*) calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, width, VREF, adc_chars);
    print_char_val_type(val_type);
    return true;
}

void Potentiometer::check_efuse() {
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
}

void Potentiometer::print_char_val_type(esp_adc_cal_value_t val_type) {
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("Characterized using eFuse Vref\n");
    } else {
        printf("Characterized using Default Vref\n");
    }
}

int Potentiometer::get_raw() {
    uint32_t sum = 0;
    for (int i = 0; i < SAMPLES; i++) {
        if (adc_unit == ADC_UNIT_1) {
            sum += adc1_get_raw((adc1_channel_t)adc_channel);
        }else {
            int raw;
            adc2_get_raw((adc2_channel_t)adc_channel, width, &raw);
            sum += raw;
        }
    }
    return sum / SAMPLES;
}

int Potentiometer::update() {
    prev_raw = (get_raw() + (prev_raw*(AVERAGESUM - 1))) / AVERAGESUM;

    if (on_change != NULL) {
        //Check threshold and maybe send update to function
    }
    return prev_raw;
}

float Potentiometer::get_percent() {
    int raw = get_raw();
    return ((float)raw / (4096)) * 100;
}