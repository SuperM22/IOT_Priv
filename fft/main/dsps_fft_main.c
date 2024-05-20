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
#include "esp_partition.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "esp_ota_ops.h"
#include <sys/param.h>
#include "esp_dsp.h"

static const char *TAG = "main";

char buffer[10];
char string[50]= "The average of the signal is: ";

#if CONFIG_BROKER_CERTIFICATE_OVERRIDDEN == 1
static const uint8_t mqtt_eclipseprojects_io_pem_start[]  = "-----BEGIN CERTIFICATE-----\n" CONFIG_BROKER_CERTIFICATE_OVERRIDE "\n-----END CERTIFICATE-----";
#else
extern const uint8_t mqtt_eclipseprojects_io_pem_start[]   asm("_binary_mqtt_eclipseprojects_io_pem_start");
#endif
extern const uint8_t mqtt_eclipseprojects_io_pem_end[]   asm("_binary_mqtt_eclipseprojects_io_pem_end");



#define _USE_MATH_DEFINES
#define N_SAMPLES 1024


//SIGNAL DEFINES
#define WINDOW_TIME 5 //seconds

#define SAMPLING_RATE 4000 // Hz 5Khz is the max frequency at which we can sample signals of low frequency (< 4/5 hz) since the bins will be of around 5hz each (Fs/n)
#define NUM_SAMPLES 1024 //SAMPLE NUMBER HAVE TO BE EXPRESSIBLE AS A POWER OF 2 ELSE FFT WON'T WORK
#define TH 3 //threshold for magnitude, max frequency of the fft
int N = NUM_SAMPLES;
typedef struct {
    int num_sin_components;
    double sin_frequencies[5]; // Maximum number of sine components
    double sin_amplitudes[5]; // Maximum number of sine components
} SignalComponent;

__attribute__((aligned(16)))
float wave[NUM_SAMPLES];
// __attribute__((aligned(16)))
// float x1[N_SAMPLES];
// __attribute__((aligned(16)))
// float x2[N_SAMPLES];
// Window coefficients
__attribute__((aligned(16)))
float wind[NUM_SAMPLES];
// working complex array
__attribute__((aligned(16)))
float y_cf[NUM_SAMPLES * 2];
// Pointers to result arrays
float *y1_cf = &y_cf[0];
float *y2_cf = &y_cf[NUM_SAMPLES];


// Sum of y1 and y2
__attribute__((aligned(16)))
float sum_y[NUM_SAMPLES / 2];


__attribute__((aligned(16)))
float new_signal[NUM_SAMPLES]; //Allocating this array inside the main gives a Rom overflow


//MQTT FUNCTIONS
//
// Note: this function is for testing purposes only publishing part of the active partition
//       (to be checked against the original binary)
//

//SIGNAL AGGREGATION

static void send_binary(esp_mqtt_client_handle_t client)
{
    esp_partition_mmap_handle_t out_handle;
    const void *binary_address;
    const esp_partition_t *partition = esp_ota_get_running_partition();
    esp_partition_mmap(partition, 0, partition->size, ESP_PARTITION_MMAP_DATA, &binary_address, &out_handle);
    // sending only the configured portion of the partition (if it's less than the partition size)
    int binary_size = MIN(CONFIG_BROKER_BIN_SIZE_TO_SEND, partition->size);
    int msg_id = esp_mqtt_client_publish(client, "/topic/binary", binary_address, binary_size, 0, 0);
    ESP_LOGI("MQTT", "binary sent with msg_id=%d", msg_id);
}
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD("MQTT", "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI("MQTT", "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        ESP_LOGI("MQTT", "sent subscribe successful, msg_id=%d", msg_id);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI("MQTT", "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI("MQTT", "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", string, 0, 0, 0);
        ESP_LOGI("MQTT", "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI("MQTT", "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI("MQTT", "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI("MQTT", "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        if (strncmp(event->data, "send binary please", event->data_len) == 0) {
            ESP_LOGI("MQTT", "Sending the binary");
            send_binary(client);
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI("MQTT", "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGI("MQTT", "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGI("MQTT", "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            ESP_LOGI("MQTT", "Last captured errno : %d (%s)",  event->error_handle->esp_transport_sock_errno,
                     strerror(event->error_handle->esp_transport_sock_errno));
        } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGI("MQTT", "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        } else {
            ESP_LOGW("MQTT", "Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;
    default:
        ESP_LOGI("MQTT", "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = CONFIG_BROKER_URI,
            .verification.certificate = (const char *)mqtt_eclipseprojects_io_pem_start
        },
    };

    ESP_LOGI("MQTT", "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

//SIGNAL GENERATOR FUNCTION

float composite_signal(float t, SignalComponent *component) {
    float signal = 0;
    
    // Summing sine components
    for (int i = 0; i < component->num_sin_components; i++) {
        signal += component->sin_amplitudes[i] * sin(2 * M_PI * component->sin_frequencies[i] * t);
    }
    
    return signal;
}

SignalComponent component = {
    2, // Number of sine components
    {3,5}, // Sine frequencies in HZ
    {2,4}, // Sine amplitudes
};

void signal_sampling(){
        for (int i = 0; i < N; i++) {
        float t = (float)i / SAMPLING_RATE;
        wave[i] = composite_signal(t, &component);
        
    }
    ESP_LOGI("signal_sampling", "Signal sampling over ");
}

void signal_resampling(float newF,float array[],int dimension){
        for (int i = 0; i < dimension; i++) {
        float t = (float)i / newF;
        array[i] = composite_signal(t, &component);
    }
    ESP_LOGI("signal_resampling", "Signal resampling over ");
}

float signal_mean(float signal[],int dimension){
    float sum = 0;
    for (int i = 0;i < dimension;i++){
        sum+=signal[i];
    }
    ESP_LOGI("signal_mean", "Signal mean over ");
    return sum/(float) dimension;
}

float perform_FFT(){
    esp_err_t ret;
    ESP_LOGI("FFT", "Start FFT.");
    ret = dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    if (ret  != ESP_OK) {
        ESP_LOGE("FFT", "Not possible to initialize FFT. Error = %i", ret);
        return 0.0;
    }

    // Generate hann window
    dsps_wind_hann_f32(wind, N);
    

    // Convert two input vectors to one complex vector
    for (int i = 0 ; i < N ; i++) {
        y_cf[i * 2] = wave[i] ;//* wind[i];
        y_cf[i * 2 + 1] = 0; // I don't have a second vector as in the example
    }
    // FFT
    unsigned int start_b = dsp_get_cpu_cycle_count();
    dsps_fft2r_fc32(y_cf, N);
    unsigned int end_b = dsp_get_cpu_cycle_count();
    // Bit reverse
    dsps_bit_rev_fc32(y_cf, N);
    // Convert one complex vector to two complex vectors
    dsps_cplx2reC_fc32(y_cf, N);

    //RETRIEVE THE MAX FREQ OF THE SIGNAL
    int maxI = 0;
    float maxM = 0;
    for (int i = 0; i < N; i++) 
    {
        float mag = sqrt( y_cf[i]*y_cf[i] + y_cf[i+1] * y_cf[i+1]);
        if(mag>TH && mag>maxM){
            maxI = i;
            maxM = mag;
        }
    }
    ESP_LOGI("FFT", "THE MAX Index is %i", maxI);
    float maxF = maxI;
    maxF = maxF*(SAMPLING_RATE) / (float) (N*2); //max frequency retrival
    


    for (int i = 0 ; i < N / 2 ; i++) {
        y1_cf[i] = 10 * log10f((y1_cf[i * 2 + 0] * y1_cf[i * 2 + 0] + y1_cf[i * 2 + 1] * y1_cf[i * 2 + 1]) / N);
        y2_cf[i] = 10 * log10f((y2_cf[i * 2 + 0] * y2_cf[i * 2 + 0] + y2_cf[i * 2 + 1] * y2_cf[i * 2 + 1]) / N);
        // Simple way to show two power spectrums as one plot
        sum_y[i] = fmax(y1_cf[i], y2_cf[i]);
    }

    // Show power spectrum in 64x10 window from -100 to 0 dB from 0..N/4 samples
    ESP_LOGW("FFT", "Signal x1");
    dsps_view(wave, N / 2, 64, 10,  -60, 40, '|');
    ESP_LOGW("FFT", "FFT");
    dsps_view(y1_cf, N / 2, 64, 10,  -60, 40, '|');
    //dsps_view(y2_cf, N / 2, 64, 10,  -60, 40, '|');
    
    //ESP_LOGW(TAG, "Signal x2");
    //dsps_view(y2_cf, N / 2, 64, 10,  -60, 40, '|');
    //ESP_LOGW(TAG, "Signals x1 and x2 on one plot");
    //dsps_view(sum_y, N / 2, 64, 10,  -60, 40, '|');
    
    ESP_LOGI("FFT", "FFT for %i complex points take %i cycles", N, end_b - start_b);
    ESP_LOGI("FFT", "The maximum frequency of the signal is %f, and the relative magnitude value is %f", maxF, maxM);
    ESP_LOGI("FFT","FFT calculation and max frequency retrieval is over");
    return maxF;
    
}



void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    //sample the signal and store it in wave[];
    signal_sampling();

    //the FFT is performed on wave[], stored in y_cf[] and the max frequency of the signal is returned as a float;
    float maxF = perform_FFT();


    //correctly evaluate how many samples of the signal are needed
    int newF= 2 * (int) ceil(maxF);
    ESP_LOGI(TAG, "Resampled at freq %i", newF);
    int dimension = newF* WINDOW_TIME ;
    
    //resample the signal and store the samples in new_signal[]
    signal_resampling(maxF,new_signal,dimension);


    float mean = signal_mean(new_signal,dimension);

    sprintf(buffer, "%.2f", mean);
    strcat(string,buffer);

    mqtt_app_start();
}

//l aggregate sarà  una semplice media
