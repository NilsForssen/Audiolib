#ifndef Audiolib_H
#define Audiolib_H

#include "esp_log.h"
#include "esp_gap_bt_api.h"
#include "esp_bt.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "driver/i2s.h"
#include "math.h"
#include "vector"
#include "cstring"
#include "algorithm"
#include "nvs_flash.h"

#define ROOT2 1.4142135623730950488016887242097
#define PI 3.1415926535897932384626433832796

typedef enum {
    NOTHING,
    HIGHPASS,
    LOWPASS,
    HIGHSHELF,
    LOWSHELF,
    PEAK
} filter_t;

typedef enum {
    AL_CONNECTED,
    AL_DISCONNECTED,
    AL_PLAYING,
    AL_PAUSED,
    AL_STOPPED,
    AL_META_UPDATE
} al_event_cb_t;

typedef union {

    struct al_meta_update_cb_param_t {
        char* title;
        char* artist;
        char* album;
    } metadata;

} al_event_cb_param_t;

class Audiolib;
class Filter;
struct CombinedChannelFilter;



class Audiolib {
public:
    Audiolib(const char* device_name, void (*)(al_event_cb_t, al_event_cb_param_t*));

    void start();
    void stop();

    void set_I2S(const int bit_clock_pin, const int word_select_pin, const int data_pin);

    void add_left_filter(Filter* filter);
    void add_right_filter(Filter* filter);
    void remove_left_filter(Filter* filter);
    void remove_right_filter(Filter* filter);
    void add_combined_filter(CombinedChannelFilter* filter);
    void remove_combined_filter(CombinedChannelFilter* filter);

    void start_filter();
    void stop_filter();
    
    void avrc_cb(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param);
    void a2d_data_cb(const uint8_t *data, uint32_t len);
    void a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t* param);

    void pause();
    void play();
    void next();
    void previous();
    

private:

    char* title = (char*)"";
    char* artist = (char*)"";
    char* album = (char*)"";

    void (*on_change_cb)(al_event_cb_t, al_event_cb_param_t*);

    void getMeta();
    uint16_t supportedNotifications = 0x00;
    bool _filtering = true;
    const char* _devname;
    const i2s_port_t i2s_num = I2S_NUM_0;
    const int _sample_rate = 44100;
    uint8_t _adr[ESP_BD_ADDR_LEN];
    std::vector<Filter*> _left;
    std::vector<Filter*> _right;
};


class Filter
{
public:
    Filter(filter_t type, uint16_t frequency, uint16_t sample_rate, float Q, float peakgain);

    template <typename in_out_type>
    in_out_type process_signal(in_out_type in) {
        float fout;
        float fin = (float) in;
        fout = fin * _b0 + _z1;
        _z1 = fin * _b1 + _z2 - _a1 * fout;
        _z2 = fin * _b2 - _a2 * fout;
        return (in_out_type) fout * volume;
    }
    void update(float peakgain);

private:
    uint16_t _sample_rate;
    filter_t _type;
    float _b0, _b1, _b2, _a1, _a2;
    float _z1 = 0, _z2 = 0;
    float _k, _q;
    float volume = 1.0;
    
};

struct CombinedChannelFilter {
    CombinedChannelFilter(Filter* left_filter, Filter* right_filter);
    void update(float gain);
    Filter* left;
    Filter* right;
};


#endif