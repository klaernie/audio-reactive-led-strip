/**
 *
 * esphome_music_leds.cpp
 *
 * Implementation of ESPHome Music LEDs
 *
 * ESPHome Music LEDs is a custom component for ESPHome that visualizes sound
 * with an addressible LED strip supported by the FastLED library.
 *
 * Original concept by: https://github.com/scottlawsonbc
 * ESPHome Port by: https://github.com/zhujisheng
 * ESPHome Component support by: https://github.com/geoffdavis
 *
 * License: GPL v3.0 - See LICENSE file in top of repository.
 */
#include "esphome_music_leds.h"
using namespace esphome;

MusicLeds::MusicLeds(
        uint16_t n_pixels,
        float min_volume_threshold,
        int pdm_ws_io_pin,
        int pdm_data_in_pin
    ): n_pixels_{n_pixels} {
    this->pin_config.ws_io_num = pdm_ws_io_pin;
    this->pin_config.data_in_num = pdm_data_in_pin;

    this->physic_leds = new CRGB[n_pixels_];

    this->fft = new FFT(
            BUFFER_SIZE*N_ROLLING_HISTORY,
            N_MEL_BIN,
            MIN_FREQUENCY,
            MAX_FREQUENCY,
            SAMPLE_RATE,
            min_volume_threshold
          );
    this->effect = new VisualEffect(N_MEL_BIN,n_pixels_);
}

MusicLeds::~MusicLeds(){
    i2s_stop(MLED_I2S_NUM);
    delete fft;
    delete effect;
    delete physic_leds;
}

void MusicLeds::setup() {
    i2s_driver_install(MLED_I2S_NUM, &i2s_config, 0, NULL);
    i2s_set_pin(MLED_I2S_NUM, &pin_config);
    i2s_stop(MLED_I2S_NUM);
    i2s_start(MLED_I2S_NUM);
    ESP_LOGCONFIG(TAG, "setup: using %u pixels, pins: %d ws, %d data for mic.",
            this->n_pixels_, pin_config.ws_io_num, pin_config.data_in_num);
}

void MusicLeds::ShowFrame( PLAYMODE CurrentMode, light::AddressableLight *p_it){
    static float mel_data[N_MEL_BIN] {};
    int16_t i2s_buf[BUFFER_SIZE] {};
    static unsigned int bytes_read {0};

    for (int i = 0; i < N_ROLLING_HISTORY - 1; i++)
        memcpy(
                y_data + i * BUFFER_SIZE, y_data + (i + 1)*BUFFER_SIZE,
                sizeof(float)*BUFFER_SIZE
              );


    i2s_read(MLED_I2S_NUM, i2s_buf, BUFFER_SIZE * 2, &bytes_read, portMAX_DELAY);

    for(int i = 0; i < BUFFER_SIZE; i++) {
        y_data[BUFFER_SIZE * (N_ROLLING_HISTORY - 1) + i] = i2s_buf[i] / 32768.0;
        static int ii=0;
        ii++;
        if(ii%SAMPLE_RATE==0)
          ESP_LOGD(TAG, "mode: %d - %lu milliseconds\t%d samples\n",
                  CurrentMode, millis(), ii);
    }
    fft->t2mel( y_data, mel_data );

    switch(CurrentMode){
      case MODE_SCROLL:
        effect->visualize_scroll(mel_data, physic_leds);
        break;
      case MODE_ENERGY:
        effect->visualize_energy(mel_data, physic_leds);
        break;
      case MODE_SPECTRUM:
        effect->visualize_spectrum(mel_data, physic_leds);
        break;
    }

    for (int i = 0; i < p_it->size(); i++) {
        light::ESPColor c;
        c.r = physic_leds[i].r;
        c.g = physic_leds[i].g;
        c.b = physic_leds[i].b;
        c.w = 0;
        (*p_it)[i] = c;
    }
}
