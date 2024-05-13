// Copyright 2018-2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include <math.h>
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include <inttypes.h>
#include <assert.h>

#include "esp_dsp.h"

static const char *TAG = "main";
static esp_adc_cal_characteristics_t adc1_chars;
// This example shows how to use FFT from esp-dsp library

#define _USE_MATH_DEFINES
#define N_SAMPLES 1024
int N = N_SAMPLES;

//SIGNAL DEFINES
#define SAMPLE_RATE 2000 //samples per second (Hz)
#define FREQUENCY_SIN 15
#define FREQUENCY_COS 15
#define DURATION 5 //seconds


// Input test array
__attribute__((aligned(16)))
float x1[N_SAMPLES];
__attribute__((aligned(16)))
float x2[N_SAMPLES];
// Window coefficients
__attribute__((aligned(16)))
float wind[N_SAMPLES];
// working complex array
__attribute__((aligned(16)))
float y_cf[N_SAMPLES * 2];
// Pointers to result arrays
float *y1_cf = &y_cf[0];
//float *y2_cf = &y_cf[N_SAMPLES];

float sig[N_SAMPLES];

// Sum of y1 and y2
__attribute__((aligned(16)))
//float sum_y[N_SAMPLES / 2];



void app_main()
{


    // //code to take the voltage as the potentiometre
    // //uint32_t voltage;
    // esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, 0, &adc1_chars);
    // ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_DEFAULT));
    // ESP_ERROR_CHECK(adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11));
    // for(int j = 0 ;j < N ; j++){
    //     x1[j] = esp_adc_cal_raw_to_voltage(adc1_get_raw(ADC1_CHANNEL_0), &adc1_chars);
    //     //printf("%lf" PRIu32 " \n", x1[j]);
    //    vTaskDelay(pdMS_TO_TICKS(1000 / Fs));
    // }



    


    esp_err_t ret;
    ESP_LOGI(TAG, "Start Example.");
    ret = dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    if (ret  != ESP_OK) {
        ESP_LOGE(TAG, "Not possible to initialize FFT. Error = %i", ret);
        return;
    }

    int num_samples = SAMPLE_RATE * DURATION;
    double wave[num_samples];
    double increment_sin = 2.0 * M_PI * FREQUENCY_SIN / SAMPLE_RATE;
    double increment_cos = 2.0 * M_PI * FREQUENCY_COS / SAMPLE_RATE;
    double phase_sin = 0.0;
    double phase_cos = 0.0;

    for (int i = 0; i < num_samples; i++) {
        double sin_value = sin(phase_sin);
        double cos_value = cos(phase_cos);
        wave[i] = sin_value + cos_value;
        phase_sin += increment_sin;
        phase_cos += increment_cos;
        if (phase_sin >= 2.0 * M_PI) {
            phase_sin -= 2.0 * M_PI;
        }
        if (phase_cos >= 2.0 * M_PI) {
            phase_cos -= 2.0 * M_PI;
        }
    }


    // Generate hann window
    dsps_wind_hann_f32(wind, N);
    // Generate input signal for x1 A=1 , F=0.1
    dsps_tone_gen_f32(x1, N, 1.0, 0.16,  0);
    // Generate input signal for x2 A=0.1,F=0.2
    dsps_tone_gen_f32(x2, N, 0.1, 0.2, 0);

    // Convert two input vectors to one complex vector
    for (int i = 0 ; i < N ; i++) {
        y_cf[i * 2 + 0] = x1[i] * wind[i];
        y_cf[i * 2 + 1] = 0;
    }
    // FFT
    unsigned int start_b = dsp_get_cpu_cycle_count();
    dsps_fft2r_fc32(y_cf, N);
    unsigned int end_b = dsp_get_cpu_cycle_count();
    // Bit reverse
    dsps_bit_rev_fc32(y_cf, N);
    // Convert one complex vector to two complex vectors
    dsps_cplx2reC_fc32(y_cf, N);

    for (int i = 0 ; i < N / 2 ; i++) {
        y1_cf[i] = 10 * log10f((y1_cf[i * 2 + 0] * y1_cf[i * 2 + 0] + y1_cf[i * 2 + 1] * y1_cf[i * 2 + 1]) / N);
        //y2_cf[i] = 10 * log10f((y2_cf[i * 2 + 0] * y2_cf[i * 2 + 0] + y2_cf[i * 2 + 1] * y2_cf[i * 2 + 1]) / N);
        // Simple way to show two power spectrums as one plot
        //sum_y[i] = fmax(y1_cf[i], y2_cf[i]);
    }

    for()

    // Show power spectrum in 64x10 window from -100 to 0 dB from 0..N/4 samples
    ESP_LOGW(TAG, "Signal x1");
    dsps_view(y1_cf, N / 2, 64, 10,  -60, 40, '|');
    ESP_LOGW(TAG, "Signal x2");
    //dsps_view(y2_cf, N / 2, 64, 10,  -60, 40, '|');
    ESP_LOGW(TAG, "Signals x1 and x2 on one plot");
    //dsps_view(sum_y, N / 2, 64, 10,  -60, 40, '|');
    ESP_LOGI(TAG, "FFT for %i complex points take %i cycles", N, end_b - start_b);

    ESP_LOGI(TAG, "End Example.");
}

//fare for per : l'indice del result è la frequenza, il valore è l'ampiezza. Onde evitare rumori prendere max frequenza sopra una certa ampiezza di threshold.