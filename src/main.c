#include "driver/i2s.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <stdio.h>

#define SAMPLE_RATE (48000)
#define I2S_NUM (0)
#define WAVE_FREQ_HZ (184)
#define MAXVOL_INT (12000)
#define PI (3.14159265)
#define I2S_BCK_IO (GPIO_NUM_5) //(GPIO_NUM_26)
#define I2S_WS_IO (GPIO_NUM_25) //(GPIO_NUM_25)
#define I2S_DO_IO (GPIO_NUM_26) //(GPIO_NUM_22)
#define I2S_DI_IO (-1)

#define SAMPLE_PER_CYCLE (SAMPLE_RATE / WAVE_FREQ_HZ)
#define BUFFER_NUM (8)
#define DELAY_MS (5)
#define SEQ_LENGTH (1)

int midi_to_cycle(int m);

// uint16_t samples_data[(SAMPLE_PER_CYCLE)];
static uint16_t* samples_data = NULL;
static int wavesize = SAMPLE_PER_CYCLE;
static int temp_wavesize = 0;
static int sequence[] = { 50 };

size_t i2s_bytes_write = 0;
// int sequence[4] = { 48, 60, 72, 84 };

void setup_waves(int d)
{
    if (samples_data != NULL)
        free(samples_data);

    uint16_t* sine = (uint16_t*)malloc(d * sizeof(uint16_t*));

    double sin_float;
    int discriminant = d / 2;
    int counter = d - 1;

    for (int i = 0; i < d; i++) {
        sin_float = sin(2.0 * (double)i * PI / (double)d);
        // double x = (double)i / (double)d;

        sine[i] = (uint16_t)((sin_float + 1) * MAXVOL_INT);
        // sine[i] = (uint16_t)(x * MAXVOL_INT);
    }

    /*
     while (counter > -1) {
         float value = counter > discriminant ? 0.5 : -0.5;
         sine[counter] = (uint16_t)(value * MAXVOL_INT);
         counter--;
     }
     */

    wavesize = d;
    samples_data = sine;
}

/*
static void setup_triangle_sine_waves(int bits){}
*/

void i2s_setup()
{
    // for 36Khz sample rates, we create 100Hz sine wave, every cycle need 36000/100 = 360 samples (4-bytes or 8-bytes each sample)
    // depend on bits_per_sample
    // using 6 buffers, we need 60-samples per buffer
    // if 2-channels, 16-bit each channel, total buffer is 360*4 = 1440 bytes
    // if 2-channels, 24/32-bit each channel, total buffer is 360*8 = 2880 bytes
    // if 1-channels, 16-bit at one channel, total buffer is 360*2 = 720 bytes

    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX, // Only TX
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = 16,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // mono
        //        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,                           //2-channels
        .communication_format = I2S_COMM_FORMAT_STAND_I2S, // I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB,
        .dma_buf_count = BUFFER_NUM,
        //.dma_buf_len = SAMPLE_PER_CYCLE, // one cycle per one buffer
        .dma_buf_len = 1024,
        .use_apll = false,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // Interrupt level 1
        .tx_desc_auto_clear = true,
        .fixed_mclk = -1
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCK_IO,
        .ws_io_num = I2S_WS_IO,
        .data_out_num = I2S_DO_IO,
        .data_in_num = I2S_DI_IO // Not used
    };
    i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM, &pin_config);
    i2s_set_clk(I2S_NUM, SAMPLE_RATE, 16, 1); // 16bits, 1 channels

    // generate sine wave data (unsigned int 16bit)
    // setup_waves(midi_to_cycle(60));
}

void main_task()
{
    while (1) {
        if (temp_wavesize != wavesize)
            setup_waves(temp_wavesize);
        i2s_write(I2S_NUM, samples_data, wavesize * 2, &i2s_bytes_write, portMAX_DELAY);
    }
}

double mtof(int m)
{
    return pow(2, (m - 69) / 12.0) * 440.0;
}

int freq_to_cycle_length(double f)
{
    return (int)((double)SAMPLE_RATE / f);
}

int midi_to_cycle(int m)
{
    double f = mtof(m);
    int dim = freq_to_cycle_length(f);
    printf("dimensione: %d\n", dim);
    return dim;
}

void app_main()
{
    // i2s_setup();
    // setup_waves();
    // uint16_t c = 0;

    BaseType_t xReturned;
    TaskHandle_t xHandle = NULL;
    i2s_setup();
    setup_waves(SAMPLE_PER_CYCLE);
    xReturned = xTaskCreate(main_task, "main_task", 1024 * 2, NULL, 5, &xHandle);

    uint8_t counter = 0;

    while (1) {
        if (counter >= SEQ_LENGTH)
            counter = 0;

        int cycle = (int)((double)SAMPLE_RATE / (double)sequence[counter]);
        temp_wavesize = cycle;

        vTaskDelay(1000 / portTICK_RATE_MS);
        counter++;
    }

    /*
        if (xReturned == pdPASS)
            vTaskDelete(xHandle);
            */

    /*
        while (1) {
            printf("executed every 5 secs\n");
            vTaskDelay(5000/portTICK_RATE_MS);
        }
    */
}
