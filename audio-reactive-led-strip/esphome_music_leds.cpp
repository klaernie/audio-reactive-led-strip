#include "esphome_music_leds.h"
using namespace esphome;

MusicLeds::MusicLeds(
        uint16_t n_pixels,
        float min_volume_threshold,
        int pdm_ws_io_pin,
        int pdm_data_in_pin
    ){
    pin_config.ws_io_num = pdm_ws_io_pin;
    pin_config.data_in_num = pdm_data_in_pin;

    physic_leds = new CRGB[n_pixels];

    fft = new FFT(
            BUFFER_SIZE*N_ROLLING_HISTORY,
            N_MEL_BIN,
            MIN_FREQUENCY,
            MAX_FREQUENCY,
            SAMPLE_RATE,
            min_volume_threshold
          );
    effect = new VisualEffect(N_MEL_BIN,n_pixels);
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

}


void MusicLeds::ShowFrame( PLAYMODE CurrentMode, light::AddressableLight *p_it){
    static float mel_data[N_MEL_BIN];

    for (int i = 0; i < N_ROLLING_HISTORY - 1; i++)
        memcpy(y_data + i * BUFFER_SIZE, y_data + (i + 1)*BUFFER_SIZE, sizeof(float)*BUFFER_SIZE);

    int16_t l[BUFFER_SIZE];

    unsigned int read_num;
        i2s_read(MLED_I2S_NUM, l, BUFFER_SIZE * 2, &read_num, portMAX_DELAY);

    for(int i = 0; i < BUFFER_SIZE; i++) {
        y_data[BUFFER_SIZE * (N_ROLLING_HISTORY - 1) + i] = l[i] / 32768.0;
        static int ii=0;
        ii++;
        if(ii%SAMPLE_RATE==0)
          ESP_LOGD("custom","%lu\t%d\n",millis(),ii);
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
