#include "Peripherals.h"

gpio_config_t btn_config = {
    .pin_bit_mask = (uint64_t) 0,
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_ENABLE,
    .intr_type = GPIO_INTR_ANYEDGE
};

Potentiometer::Potentiometer(const int min, const int max, pot_update_callback_t update_ntfy)
    : Potentiometer(ADC_UNIT_1, ADC_CHANNEL_1, min, max, update_ntfy) {}

Potentiometer::Potentiometer(const adc_unit_t unit, const adc_channel_t channel, const int min, const int max, pot_update_callback_t update_ntfy) 
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

Button::Button(gpio_num_t gpio_pin, btn_update_callback_t update_ntfy)
 : on_change(update_ntfy),
 gpio_pin(gpio_pin) {
    gpio_config_t config = btn_config;
    config.pin_bit_mask = (1 << gpio_pin);
    gpio_config(&config);

    // static esp_err_t isr_service_init = gpio_install_isr_service(0);
    // gpio_isr_handler_add(gpio_pin, btn_isr_handler, this);
}

bool Button::getPressed() {
    return (bool) gpio_get_level(gpio_pin);
}

bool Button::getPressedSingle() {
    bool state = (bool) gpio_get_level(gpio_pin);
    bool ret = state && !lastState;
    lastState = state;
    return ret;
}
