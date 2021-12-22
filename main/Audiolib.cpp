#include "Audiolib.h"

Audiolib *Audiolib_redirect;
void a2d_cb_redirect(esp_a2d_cb_event_t, esp_a2d_cb_param_t *);
void a2d_data_cb_redirect(const uint8_t *, uint32_t);
void avrc_cb_redirect(esp_avrc_ct_cb_event_t, esp_avrc_ct_cb_param_t *);

static al_event_cb_param_t sending_param;

static bool sent_title = false;
static bool sent_artist = false;
static bool sent_album = false;

Audiolib::Audiolib(const char *device_name, void (*on_change)(al_event_cb_t, al_event_cb_param_t*))
{
    _devname = device_name;
    Audiolib_redirect = this;
    this->on_change_cb = on_change;
}

void Audiolib::start()
{
    nvs_flash_init();
    
    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&cfg);
    esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);

    esp_bluedroid_init();
    esp_bluedroid_enable();

    esp_bt_dev_set_device_name(_devname);

    esp_avrc_ct_init();
    esp_avrc_ct_register_callback(avrc_cb_redirect);

    esp_a2d_sink_init();
    esp_a2d_register_callback(a2d_cb_redirect);
    esp_a2d_sink_register_data_callback(a2d_data_cb_redirect);

    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
}

void Audiolib::stop()
{
    esp_a2d_sink_disconnect(_adr);
    esp_a2d_sink_deinit();
    esp_avrc_ct_deinit();
    esp_bluedroid_deinit();
    esp_bluedroid_disable();

    i2s_stop(i2s_num);
    i2s_driver_uninstall(i2s_num);
}

void Audiolib::set_I2S(const int bit_clock_pin, const int word_select_pin, const int data_pin)
{
    static const i2s_config_t i2s_config = {
        .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = _sample_rate,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 3,
        .dma_buf_len = 600,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0};

    static const i2s_pin_config_t pin_config = {
        .bck_io_num = bit_clock_pin,
        .ws_io_num = word_select_pin,
        .data_out_num = data_pin,
        .data_in_num = I2S_PIN_NO_CHANGE};

    i2s_driver_install(i2s_num, &i2s_config, 0, NULL);
    i2s_set_pin(i2s_num, &pin_config);
}

void Audiolib::getMeta()
{
    esp_avrc_ct_send_metadata_cmd(0, ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST | ESP_AVRC_MD_ATTR_ALBUM);
}

void Audiolib::play() {
    esp_avrc_ct_send_passthrough_cmd(0, ESP_AVRC_PT_CMD_PLAY, ESP_AVRC_PT_CMD_STATE_PRESSED);
    esp_avrc_ct_send_passthrough_cmd(1, ESP_AVRC_PT_CMD_PLAY, ESP_AVRC_PT_CMD_STATE_RELEASED);
}

void Audiolib::pause() {
    esp_avrc_ct_send_passthrough_cmd(0, ESP_AVRC_PT_CMD_PAUSE, ESP_AVRC_PT_CMD_STATE_PRESSED);
    esp_avrc_ct_send_passthrough_cmd(1, ESP_AVRC_PT_CMD_PAUSE, ESP_AVRC_PT_CMD_STATE_RELEASED);
}

void Audiolib::next() {
    esp_avrc_ct_send_passthrough_cmd(0, ESP_AVRC_PT_CMD_FORWARD, ESP_AVRC_PT_CMD_STATE_PRESSED);
    esp_avrc_ct_send_passthrough_cmd(1, ESP_AVRC_PT_CMD_FORWARD, ESP_AVRC_PT_CMD_STATE_RELEASED);
}

void Audiolib::previous() {
    esp_avrc_ct_send_passthrough_cmd(0, ESP_AVRC_PT_CMD_BACKWARD, ESP_AVRC_PT_CMD_STATE_PRESSED);
    esp_avrc_ct_send_passthrough_cmd(1, ESP_AVRC_PT_CMD_BACKWARD, ESP_AVRC_PT_CMD_STATE_RELEASED);
}

void Audiolib::avrc_cb(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param)
{
    char *text;
    switch (event)
    {
    case ESP_AVRC_CT_CONNECTION_STATE_EVT:

        printf("AVRC, Connection state\n");
        esp_avrc_ct_send_get_rn_capabilities_cmd(0);
        getMeta();
        break;

    case ESP_AVRC_CT_PASSTHROUGH_RSP_EVT:

        printf("AVRC, Passthrough\n");
        break;

    case ESP_AVRC_CT_METADATA_RSP_EVT:

        printf("AVRC, Metadata response\n");
        text = (char *)calloc(param->meta_rsp.attr_length + 1, sizeof(char));
        memcpy(text, param->meta_rsp.attr_text, param->meta_rsp.attr_length);
        switch (param->meta_rsp.attr_id)
        {
        case ESP_AVRC_MD_ATTR_TITLE:
            title = text;
            sent_title = true;
            break;
        case ESP_AVRC_MD_ATTR_ARTIST:
            artist = text;
            sent_artist = true;
            break;
        case ESP_AVRC_MD_ATTR_ALBUM:
            album = text;
            sent_album = true;
            break;
        }
        if (sent_title && sent_artist && sent_album) {
            sending_param.metadata = {
            .title = this->title,
            .artist = this->artist,
            .album = this->album, 
            };
            on_change_cb(AL_META_UPDATE, &sending_param);
            sent_title = false;
            sent_artist = false;
            sent_album = false;
        }
        break;

    case ESP_AVRC_CT_PLAY_STATUS_RSP_EVT:

        printf("AVRC, Play status\n");
        break;

    case ESP_AVRC_CT_CHANGE_NOTIFY_EVT:

        switch(param->change_ntf.event_id)
        {
        case ESP_AVRC_RN_TRACK_CHANGE:

            printf("AVRC NTFY, track change\n");
            getMeta();
            esp_avrc_ct_send_register_notification_cmd(0, ESP_AVRC_RN_TRACK_CHANGE, 0);
            break;

        case ESP_AVRC_RN_PLAY_STATUS_CHANGE:

            printf("AVRC NTFY, play status change\n");
            esp_avrc_ct_send_register_notification_cmd(0, ESP_AVRC_RN_PLAY_STATUS_CHANGE, 0);

            switch(param->change_ntf.event_parameter.playback) {
            case ESP_AVRC_PLAYBACK_STOPPED:
                printf("Playback stopped\n");
                on_change_cb(AL_STOPPED, NULL);
                break;
            case ESP_AVRC_PLAYBACK_PLAYING:
                printf("Playback playing\n");
                on_change_cb(AL_PLAYING, NULL);
                break;
            case ESP_AVRC_PLAYBACK_PAUSED:
                printf("Playback paused\n");
                on_change_cb(AL_PAUSED, NULL);
                break;
            case ESP_AVRC_PLAYBACK_ERROR:
                printf("Playback erorr\n");
                break;
            default:
                printf("Default playback case\n");
                break;
            }
            break;

        case ESP_AVRC_RN_BATTERY_STATUS_CHANGE:

            printf("AVRC NTFY, Battery Change\n");
            esp_avrc_ct_send_register_notification_cmd(0, ESP_AVRC_RN_BATTERY_STATUS_CHANGE, 0);

            switch(param->change_ntf.event_parameter.batt) {
            case ESP_AVRC_BATT_NORMAL:
                printf("Battery Normal\n");
                break;
            case ESP_AVRC_BATT_WARNING:
                printf("Battery Warning\n");
                break;
            case ESP_AVRC_BATT_EXTERNAL:
                printf("Battery External\n");
                break;
            case ESP_AVRC_BATT_FULL_CHARGE:
                printf("Battery Full Charge\n");
                break;
            case ESP_AVRC_BATT_CRITICAL:
                printf("Battery Critical\n");
                break;
            }
            break;

        case ESP_AVRC_RN_VOLUME_CHANGE:

            printf("AVRC NTFY, Volume Change");
            esp_avrc_ct_send_register_notification_cmd(0, ESP_AVRC_RN_VOLUME_CHANGE, 0);
            break;

        default: 

            printf("AVRC NTFY, Default change notify");
            break;

        }
        break;

    case ESP_AVRC_CT_REMOTE_FEATURES_EVT:

        printf("AVRC, Remote features\n");
        break;

    case ESP_AVRC_CT_GET_RN_CAPABILITIES_RSP_EVT:

        printf("AVRC, RN capabilities\n");
        printf("Sent requests\n");
        esp_avrc_ct_send_register_notification_cmd(0, ESP_AVRC_RN_TRACK_CHANGE, 0);
        esp_avrc_ct_send_register_notification_cmd(1, ESP_AVRC_RN_PLAY_STATUS_CHANGE, 0);
        esp_avrc_ct_send_register_notification_cmd(2, ESP_AVRC_RN_VOLUME_CHANGE, 0);
        esp_avrc_ct_send_register_notification_cmd(3, ESP_AVRC_RN_BATTERY_STATUS_CHANGE, 0);
        break;

    default:

        printf("AVRC, Default case\n");
        break;

    }
}

void Audiolib::a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    switch (event)
    {
    case ESP_A2D_CONNECTION_STATE_EVT:
        switch (param->conn_stat.state)
        {
        case ESP_A2D_AUDIO_STATE_STARTED:
            for (int i = 0; i < ESP_BD_ADDR_LEN; i++)
            {
                _adr[i] = *(param->conn_stat.remote_bda + i);
            }
            printf("A2D, Connected with adress set\n");
            break;
        case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
            printf("A2D, Disconnected\n");
            break;
        case ESP_A2D_CONNECTION_STATE_DISCONNECTING:
            printf("A2D, Disconnecting\n");
            break;
        case ESP_A2D_CONNECTION_STATE_CONNECTING:
            printf("A2D, Connecting\n");
            break;
        }
        break;
    case ESP_A2D_AUDIO_STATE_EVT:
        switch(param->audio_stat.state){
        case ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND:
            printf("A2D, Audio state suspended\n");
            break;
        case ESP_A2D_AUDIO_STATE_STOPPED:
            printf("A2D, Audio state stopped\n");
            break;
        case ESP_A2D_AUDIO_STATE_STARTED:
            printf("A2D, Audio state started\n");
            break;
        }
        break;
    case ESP_A2D_AUDIO_CFG_EVT:
        printf("A2D, Audio config\n");
        break;
    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
        printf("A2D, Media control\n");
        break;
    default:
        printf("A2D, Default catch\n");
        break;
    }
}

void Audiolib::a2d_data_cb(const uint8_t *data, uint32_t len)
{
    int16_t *data16 = (int16_t *)data;
    int temp;

    size_t bytes_written = 0;
    int16_t out[2];
    for (int i = 0; i < len / 4; i++)
    {
        temp = *data16;
        if (_filtering)
        {
            for (Filter *filter : _left)
            {
                temp = filter->process_signal(temp);
                if (temp > 32767)
                {
                    temp = 32767;
                }
                if (temp < -32767)
                {
                    temp = -32767;
                }
            }
        }
        out[0] = (int16_t)(temp);
        data16++;

        temp = *data16;

        if (_filtering)
        {
            for (Filter *filter : _right)
            {
                temp = filter->process_signal(temp);
                if (temp > 32767)
                {
                    temp = 32767;
                }
                if (temp < -32767)
                {
                    temp = -32767;
                }
            }
        }
        out[1] = (int16_t)(temp);
        data16++;
        i2s_write(i2s_num, out, 4, &bytes_written, 100);
    }
}

void Audiolib::add_left_filter(Filter *filter)
{
    _left.push_back(filter);
}

void Audiolib::add_right_filter(Filter *filter)
{
    _right.push_back(filter);
}

void Audiolib::remove_left_filter(Filter *filter)
{
    _left.erase(std::remove(_left.begin(), _left.end(), filter), _left.end());
}

void Audiolib::remove_right_filter(Filter *filter)
{
    // Might have to include Algorithm
    _right.erase(std::remove(_right.begin(), _right.end(), filter), _right.end());
}

void Audiolib::add_combined_filter(CombinedChannelFilter *filter)
{
    _right.push_back(filter->right);
    _left.push_back(filter->left);
}

void Audiolib::remove_combined_filter(CombinedChannelFilter *filter)
{
    _right.erase(std::remove(_right.begin(), _right.end(), filter->right), _right.end());
    _left.erase(std::remove(_left.begin(), _left.end(), filter->left), _left.end());
}

void Audiolib::start_filter()
{
    _filtering = true;
}

void Audiolib::stop_filter()
{
    _filtering = false;
}

void Audiolib::setAbsVolume(uint8_t vol) {
    esp_avrc_ct_send_set_absolute_volume_cmd(0, vol);
}

Filter::Filter(filter_t type, uint16_t freq, uint16_t sample_rate, float Q, float peakgain)
{
    _sample_rate = sample_rate;
    _q = Q;
    _k = tan(PI * freq / _sample_rate);
    _type = type;
    update(peakgain);
}

void Filter::update(float peakgain)
{
    float v0 = pow(10, fabs(peakgain) / 20);
    float denom;
    float sqrt2v0;

    switch (_type)
    {
    case NOTHING:
        _b0 = 1;
        _b1 = 0;
        _b2 = 0;
        _a1 = 0;
        _a2 = 0;
        break;
    case HIGHPASS:
        denom = 1 / (1 + _k / _q + _k * _k);
        _b0 = 1 * denom;
        _b1 = -2 * _b0;
        _b2 = _b0;
        _a1 = 2 * (_k * _k - 1) * denom;
        _a2 = (1 - _k / _q + _k * _k) * denom;
        break;
    case LOWPASS:
        denom = 1 / (1 + _k / _q + _k * _k);
        _b0 = _k * _k * denom;
        _b1 = 2 * _b0;
        _b2 = _b0;
        _a1 = 2 * (_k * _k - 1) * denom;
        _a2 = (1 - _k / _q + _k * _k) * denom;
        break;
    case HIGHSHELF:
        sqrt2v0 = sqrt(2 * v0);
        if (peakgain >= 0)
        {
            denom = 1 / (1 + ROOT2 * _k + _k * _k);
            _b0 = (v0 + sqrt2v0 * _k + _k * _k) * denom;
            _b1 = 2 * (_k * _k - v0) * denom;
            _b2 = (v0 - sqrt2v0 * _k + _k * _k) * denom;
            _a1 = 2 * (_k * _k - 1) * denom;
            _a2 = (1 - ROOT2 * _k + _k * _k) * denom;
        }
        else
        {
            denom = 1 / (v0 + sqrt2v0 * _k + _k * _k);
            _b0 = (1 + ROOT2 * _k + _k * _k) * denom;
            _b1 = 2 * (_k * _k - 1) * denom;
            _b2 = (1 - ROOT2 * _k + _k * _k) * denom;
            _a1 = 2 * (_k * _k - v0) * denom;
            _a2 = (v0 - sqrt2v0 * _k + _k * _k) * denom;
        }
        break;
    case LOWSHELF:
        sqrt2v0 = sqrt(2 * v0);
        if (peakgain >= 0)
        {
            denom = 1 / (1 + ROOT2 * _k + _k * _k);
            _b0 = (1 + sqrt2v0 * _k + v0 * _k * _k) * denom;
            _b1 = 2 * (v0 * _k * _k - 1) * denom;
            _b2 = (1 - sqrt2v0 * _k + v0 * _k * _k) * denom;
            _a1 = 2 * (_k * _k - 1) * denom;
            _a2 = (1 - ROOT2 * _k + _k * _k) * denom;
        }
        else
        {
            denom = 1 / (1 + sqrt2v0 * _k + v0 * _k * _k);
            _b0 = (1 + ROOT2 * _k + _k * _k) * denom;
            _b1 = 2 * (_k * _k - 1) * denom;
            _b2 = (1 - ROOT2 * _k + _k * _k) * denom;
            _a1 = 2 * (v0 * _k * _k - 1) * denom;
            _a2 = (1 - sqrt2v0 * _k + v0 * _k * _k) * denom;
        }
        break;
    case PEAK:
        if (peakgain >= 0)
        {
            denom = 1 / (1 + 1 / _q * _k + _k * _k);
            _b0 = (1 + v0 / _q * _k + _k * _k) * denom;
            _b1 = 2 * (_k * _k - 1) * denom;
            _b2 = (1 - v0 / _q * _k + _k * _k) * denom;
            _a1 = _b1;
            _a2 = (1 - 1 / _q * _k + _k * _k) * denom;
        }
        else
        {
            denom = 1 / (1 + v0 / _q * _k + _k * _k);
            _b0 = (1 + 1 / _q * _k + _k * _k) * denom;
            _b1 = 2 * (_k * _k - 1) * denom;
            _b2 = (1 - 1 / _q * _k + _k * _k) * denom;
            _a1 = _b1;
            _a2 = (1 - v0 / _q * _k + _k * _k) * denom;
        }
        break;
    }
}

void a2d_cb_redirect(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    Audiolib_redirect->a2d_cb(event, param);
}
void a2d_data_cb_redirect(const uint8_t *data, uint32_t len)
{
    Audiolib_redirect->a2d_data_cb(data, len);
}

void avrc_cb_redirect(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param)
{
    Audiolib_redirect->avrc_cb(event, param);
}

CombinedChannelFilter::CombinedChannelFilter(Filter *left_filter, Filter *right_filter)
{
    left = left_filter;
    right = right_filter;
}

void CombinedChannelFilter::update(float gain)
{
    left->update(gain);
    right->update(gain);
}
