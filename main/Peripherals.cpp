#include "Peripherals.h"

Potentiometer::Potentiometer(pot_update_callback_t update_ntfy)
    : Potentiometer(ADC_UNIT_1, ADC_CHANNEL_1, 0, update_ntfy) {}

Potentiometer::Potentiometer(const adc_unit_t unit, const adc_channel_t channel, float offset_fact, pot_update_callback_t update_ntfy) 
    : adc_unit(unit), 
    adc_channel(channel), 
    on_change(update_ntfy),
    offset_fact(offset_fact),
    prev_raw(0) {

    init_adc(unit, channel);
}

int Potentiometer::get_raw() {
    int sum = 0;
    for (int i = 0; i < SAMPLES; i++) {
        if (adc_unit == ADC_UNIT_1) {
            sum += adc1_get_raw((adc1_channel_t)adc_channel);
        }
        else {
            int raw;
            adc2_get_raw((adc2_channel_t)adc_channel, WIDTH, &raw);
            sum += raw;
        }
    }
    return sum / SAMPLES;
}

float Potentiometer::get_percent() {
    int raw = get_raw();
    return ((float)raw / (4096)) * 100;
}

void Potentiometer::update() {
    int current = get_raw();
    if (on_change != NULL) {
        //Check threshold and maybe send update to function
        if (abs((int) current - prev_raw) > DIFF_THRESHOLD || ((current % 4095 == 0) && (prev_raw % 4095 != 0))) {
            float percent = (((float)current / (4095)) * 100) * offset_fact;
            if (percent > 100.0) {
                percent = 100.0;
            }
            else if (percent < 0) {
                percent = 0;
            }
            (*on_change)(percent);
            prev_raw = current;
        }
    }
}

Button::Button(gpio_num_t gpio_pin, btn_update_callback_t update_ntfy)
 : on_change(update_ntfy),
 gpio_pin(gpio_pin) {
    gpio_config_t config = {
        .pin_bit_mask = (uint64_t) 0,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    config.pin_bit_mask = (1 << gpio_pin);
    gpio_config(&config);
}

bool Button::getPressed() {
    return !(bool) gpio_get_level(gpio_pin);
}

bool Button::getPressedSingle() {
    bool state = !(bool) gpio_get_level(gpio_pin);
    bool ret = state && !lastState;
    lastState = state;
    return ret;
}

void Button::update() {
    if ((on_change != NULL) && getPressedSingle()) {
        (*on_change)();
    }
}

TogglePIN::TogglePIN(gpio_num_t gpio_pin) : gpio_pin(gpio_pin) {
    gpio_config_t config = {
        .pin_bit_mask = (uint64_t) 0,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    config.pin_bit_mask = (1 << gpio_pin);
    gpio_config(&config);
}

bool TogglePIN::get() {
    return gpio_get_level(gpio_pin);
}

void TogglePIN::set(bool level) {
    gpio_set_level(gpio_pin, level);
}


BatteryMonitor::BatteryMonitor(const adc_unit_t unit, const adc_channel_t channel, float zero)
    : adc_unit(unit),
    adc_channel(channel), 
    zero(zero) {

    init_adc(unit, channel);
}

int BatteryMonitor::get_raw() {
    int sum = 0;
    for (int i = 0; i < SAMPLES; i++) {
        if (adc_unit == ADC_UNIT_1) {
            sum += adc1_get_raw((adc1_channel_t)adc_channel);
        }
        else {
            int raw;
            adc2_get_raw((adc2_channel_t)adc_channel, WIDTH, &raw);
            sum += raw;
        }
    }
    return sum / SAMPLES;
}

float BatteryMonitor::get_percentage() {
    int current = get_raw();
    // Do some zeroing here
    return (((float)current / (4095)) * 100);
}


