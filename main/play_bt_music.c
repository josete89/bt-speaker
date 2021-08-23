/* Play music from Bluetooth device

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "i2s_stream.h"
#include "esp_peripherals.h"
#include "periph_touch.h"
#include "periph_adc_button.h"
#include "periph_button.h"
#include "periph_led.h"
#include "board.h"
#include "filter_resample.h"
#include "audio_mem.h"
#include "bluetooth_service.h"
#include "equalizer.h"

static const char *TAG = "Jose's car speaker";

esp_periph_handle_t led_handle = NULL;
void start_blink()
{
    if (led_handle) {
        periph_led_blink(led_handle, get_green_led_gpio(), 500, 500, true, -1, 0);
    }
    ESP_LOGW(TAG, "Start speaking now");
}

static audio_element_handle_t create_filter(int source_rate, int source_channel, int dest_rate, int dest_channel, int mode)
{
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    //rsp_cfg.src_rate = source_rate;
    //rsp_cfg.src_ch = source_channel;
    rsp_cfg.dest_rate = dest_rate;
    rsp_cfg.dest_ch = dest_channel;
    rsp_cfg.mode = mode;
    rsp_cfg.complexity = 0;
    return rsp_filter_init(&rsp_cfg);
}

void app_main(void)
{
    audio_pipeline_handle_t pipeline;
    audio_element_handle_t bt_stream_reader, i2s_stream_writer, equalizer;

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    ESP_LOGI(TAG, "[ 1 ] Create Bluetooth service");
    bluetooth_service_cfg_t bt_cfg = {
        .device_name = "Jose's car speaker",
        .mode = BLUETOOTH_A2DP_SINK,
    };
    bluetooth_service_start(&bt_cfg);

    ESP_LOGI(TAG, "[ 2 ] Start codec chip");
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[ 3 ] Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);

    ESP_LOGI(TAG, "[3.1] Create i2s stream to write data to codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg);

    ESP_LOGI(TAG, "[3.2] Get Bluetooth stream");
    bt_stream_reader = bluetooth_service_create_stream();
/*
    equalizer_cfg_t eq_cfg = DEFAULT_EQUALIZER_CONFIG();
    //int set_gain[] = { -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13};
    int set_gain[] = { 0,0,0,0,0 ,0,0,0, 0,0, 0,0,0,0,0 ,0,0,0,0,0};
    //                LEFT SPEAKER                     ,                   RIGHT SPEAKER
    eq_cfg.channel = 2;
    eq_cfg.set_gain =
        set_gain; // The size of gain array should be the multiplication of NUMBER_BAND and number channels of audio stream data. The minimum of gain is -13 dB.
    equalizer = equalizer_init(&eq_cfg);*/

    ESP_LOGI(TAG, "[3.2] Register all elements to audio pipeline");

    //audio_element_handle_t filter_upsample_el = create_filter(NULL, NULL, PLAYBACK_RATE, PLAYBACK_CHANNEL, RESAMPLE_DECODE_MODE);


    audio_pipeline_register(pipeline, bt_stream_reader, "bt");
    //audio_pipeline_register(pipeline, filter_upsample_el, "filter_upsample");
    //audio_pipeline_register(pipeline, equalizer, "equalizer");
    audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

    audio_hal_set_volume(board_handle->audio_hal, 90);

    ESP_LOGI(TAG, "[3.3] Link it together [Bluetooth]-->bt_stream_reader-->i2s_stream_writer-->[codec_chip]");

#if (CONFIG_ESP_LYRATD_MSC_V2_1_BOARD || CONFIG_ESP_LYRATD_MSC_V2_2_BOARD)
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = 44100;
    rsp_cfg.src_ch = 2;
    rsp_cfg.dest_rate = 48000;
    rsp_cfg.dest_ch = 2;
    audio_element_handle_t filter = rsp_filter_init(&rsp_cfg);
    audio_pipeline_register(pipeline, filter, "filter");
    i2s_stream_set_clk(i2s_stream_writer, 48000, 16, 2);
    const char *link_tag[3] = {"bt", "filter", "i2s"};
    audio_pipeline_link(pipeline, &link_tag[0], 3);
#else
    //const char *link_tag[3] = {"bt", "equalizer", "i2s"};
    //audio_pipeline_link(pipeline, &link_tag[0], 3);
    
   
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = 44100;
    rsp_cfg.src_ch = 2;
    rsp_cfg.dest_rate = 96000;
    rsp_cfg.dest_ch = 2;
    audio_element_handle_t filter = rsp_filter_init(&rsp_cfg);
    audio_pipeline_register(pipeline, filter, "filter");
    i2s_stream_set_clk(i2s_stream_writer, 96000, 16, 2);

    //_cfg.samplerate = 96000;
    

    const char *link_tag[3] = {"bt", "filter", "i2s"};
    audio_pipeline_link(pipeline, &link_tag[0], 3);
#endif
    ESP_LOGI(TAG, "[ 4 ] Initialize peripherals");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    //ESP_LOGI(TAG, "[ 2 ] Initialize IS31fl3216 peripheral");
    //periph_is31fl3216_cfg_t is31fl3216_cfg = { 0 };
    //is31fl3216_cfg.state = IS31FL3216_STATE_ON;
    //esp_periph_handle_t is31fl3216_periph = periph_is31fl3216_init(&is31fl3216_cfg);

    ESP_LOGI(TAG, "[4.1] Initialize Touch peripheral");
    audio_board_key_init(set);

    ESP_LOGI(TAG, "[ 4.1.1 ] Start peripherals");

    periph_led_cfg_t led_cfg = {
        .led_speed_mode = LEDC_LOW_SPEED_MODE,
        .led_duty_resolution = LEDC_TIMER_10_BIT,
        .led_timer_num = LEDC_TIMER_0,
        .led_freq_hz = 5000,
    };
    led_handle = periph_led_init(&led_cfg);
    

    //esp_periph_start(set, is31fl3216_periph);

    //periph_is31fl3216_set_duty(is31fl3216_periph, 0, 255);
    //periph_is31fl3216_state_t led_state = IS31FL3216_STATE_ON;
    //periph_is31fl3216_set_state(is31fl3216_periph, led_state);

    ESP_LOGI(TAG, "[4.2] Create Bluetooth peripheral");
    esp_periph_handle_t bt_periph = bluetooth_service_create_periph();

    ESP_LOGI(TAG, "[4.2] Start all peripherals");
    esp_periph_start(set, bt_periph);
    esp_periph_start(set, led_handle);

    ESP_LOGI(TAG, "[ 5 ] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[5.1] Listening event from all elements of pipeline");
    audio_pipeline_set_listener(pipeline, evt);

    ESP_LOGI(TAG, "[5.2] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    ESP_LOGI(TAG, "[ 6 ] Start audio_pipeline");
    start_blink();
    audio_pipeline_run(pipeline);

    //audio_hal_set_volume(board_handle->audio_hal, 90);

    int mode = 0;

    int player_volume;
    audio_hal_get_volume(board_handle->audio_hal, &player_volume);
    ESP_LOGI(TAG, "[ * ] Volume = %d", player_volume);

    ESP_LOGI(TAG, "[ 7 ] Listen for all pipeline events");
    while (1) {
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) bt_stream_reader
            && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
            audio_element_info_t music_info = {0};
            audio_element_getinfo(bt_stream_reader, &music_info);

            ESP_LOGI(TAG, "[ * ] Receive music info from Bluetooth, sample_rates=%d, bits=%d, ch=%d",
                     music_info.sample_rates, music_info.bits, music_info.channels);

            audio_element_setinfo(i2s_stream_writer, &music_info);
#if (CONFIG_ESP_LYRATD_MSC_V2_1_BOARD || CONFIG_ESP_LYRATD_MSC_V2_2_BOARD)
#else
            /*if (equalizer_set_info(equalizer, music_info.sample_rates, music_info.channels) != ESP_OK) {
               ESP_LOGI(TAG, "equalizer_set_info error"); 
            }*/
            //i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
#endif
            continue;
        }

        if ((msg.source_type == PERIPH_ID_TOUCH || msg.source_type == PERIPH_ID_BUTTON || msg.source_type == PERIPH_ID_ADC_BTN)
            && (msg.cmd == PERIPH_TOUCH_TAP || msg.cmd == PERIPH_BUTTON_PRESSED || msg.cmd == PERIPH_ADC_BUTTON_PRESSED)) {

            if ((int) msg.data == get_input_play_id()) {
                ESP_LOGI(TAG, "[ * ] [Play] touch tap event");
                periph_bluetooth_play(bt_periph);
            } else if ((int) msg.data == get_input_set_id()) {
                ESP_LOGI(TAG, "[ * ] [Set] touch tap event");
                periph_bluetooth_pause(bt_periph);
            } else if ((int) msg.data == get_input_volup_id()) {
                ESP_LOGI(TAG, "[ * ] [Vol+] touch tap event");
                periph_bluetooth_next(bt_periph);
                player_volume += 10;
                if (player_volume > 100) {
                    player_volume = 100;
                }
                // print volume
                ESP_LOGI(TAG, "[ * ] Volume = %d", player_volume);
                //audio_hal_set_volume(board_handle->audio_hal, player_volume);
            } else if ((int) msg.data == get_input_voldown_id()) {
                ESP_LOGI(TAG, "[ * ] [Vol-] touch tap event");
                player_volume -= 10;
                if (player_volume < 0) {
                    player_volume = 0;
                }
                //audio_hal_set_volume(board_handle->audio_hal, player_volume);
                periph_bluetooth_prev(bt_periph);
            } else if ((int) msg.data == get_input_mode_id()){
                ESP_LOGI(TAG, "[ * ] [Mode] tap event");
                /*mode = mode + 1;
                if (mode > 1) {
                    mode = 0;
                }

                if (mode == 0) {
                    eq_cfg.set_gain = set_gain;
                    ESP_LOGI(TAG, "[ * ] [Mode] Normal Eq");
                } else {
                    ESP_LOGI(TAG, "[ * ] [Mode] Pop Eq");
                    int set_gain_pop[] = { -1.6, 4.5, 7, 8, 5.6, 0, -2.5, -2, -1.6, -1.5, -1.6, 4.5, 7, 8, 5.6, 0, -2.5, -2, -1.6, -1.5 };
                    eq_cfg.set_gain = set_gain_pop;
                }
                */

            }
        }

        /* Stop when the Bluetooth is disconnected or suspended */
        if (msg.source_type == PERIPH_ID_BLUETOOTH
            && msg.source == (void *)bt_periph) {
            if (msg.cmd == PERIPH_BLUETOOTH_DISCONNECTED) {
                ESP_LOGW(TAG, "[ * ] Bluetooth disconnected");
                break;
            }
        }
        /* Stop when the last pipeline element (i2s_stream_writer in this case) receives stop event */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) i2s_stream_writer
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS
            && (((int)msg.data == AEL_STATUS_STATE_STOPPED) || ((int)msg.data == AEL_STATUS_STATE_FINISHED))) {
            ESP_LOGW(TAG, "[ * ] Stop event received");
            break;
        }
    }

    ESP_LOGI(TAG, "[ 8 ] Stop audio_pipeline");
    periph_led_stop(led_handle, get_green_led_gpio());
    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);

    audio_pipeline_unregister(pipeline, bt_stream_reader);
    //audio_pipeline_unregister(pipeline, equalizer);
    audio_pipeline_unregister(pipeline, i2s_stream_writer);

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline);

    /* Stop all peripherals before removing the listener */
    esp_periph_set_stop_all(set);
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    /* Release all resources */

#if (CONFIG_ESP_LYRATD_MSC_V2_1_BOARD || CONFIG_ESP_LYRATD_MSC_V2_2_BOARD)
    audio_pipeline_unregister(pipeline, filter);
    audio_element_deinit(filter);
#endif
    audio_pipeline_unregister(pipeline, filter);
    audio_element_deinit(filter);
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(bt_stream_reader);
    //audio_element_deinit(equalizer);
    audio_element_deinit(i2s_stream_writer);
    esp_periph_set_destroy(set);
    bluetooth_service_destroy();
}
