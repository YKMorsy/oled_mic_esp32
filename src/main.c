// stdlib
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <complex.h>
// error logging
#include "esp_log.h"
// rtos
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// adc
#include "driver/adc.h"
#include "esp_adc_cal.h"
// display
#include "ssd1306.h"

#define TAG "SSD1306"
#define ADC_CHANNEL ADC1_CHANNEL_6
#define DEFAULT_VREF 1100
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define NO_OF_SAMPLES 128
#define NUM_BINS 16

SSD1306_t dev;
static SemaphoreHandle_t bin_sem; 
static double complex fft_out[NO_OF_SAMPLES];

void fft(float *x, int N, int s, double complex *y)
{
    if (N == 1)
    {
        y[0] = x[0];
    }
    else
    {
        fft(x, N / 2, 2 * s, y);
        fft(x + s, N / 2, 2 * s, y + N / 2);

        for (int i = 0; i < N / 2; i++)
        {
            double complex t = y[i];
            double complex u = y[i + N / 2];
            double complex w = cexp(-2 * M_PI * I * i / N); // Using complex exponentiation
            y[i] = t + w * u;
            y[i + N / 2] = t - w * u;
        }
    }
}

void mic_in(void *pvParameters)
{
    esp_adc_cal_characteristics_t adc_chars;
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_0, ADC_WIDTH_BIT_12, DEFAULT_VREF, &adc_chars);
    adc_set_clk_div(1);

    float samples[NO_OF_SAMPLES]; // 4 * 64 = 512 bytes

    while (1)
    {
        xSemaphoreTake(bin_sem, portMAX_DELAY);

        float sum = 0;
        for (int i = 0; i < NO_OF_SAMPLES; i++)
        {
            samples[i] = adc1_get_raw(ADC_CHANNEL);
            sum += samples[i];
        }

        float mean = sum / NO_OF_SAMPLES;

        // subtract mean from each sample
        for (int i = 0; i < NO_OF_SAMPLES; i++)
        {
            samples[i] -= mean;  // Center around 0
        }

        fft(samples, NO_OF_SAMPLES, 1, fft_out);

        xSemaphoreGive(bin_sem);
        vTaskDelay(pdMS_TO_TICKS(10));  // Prevent WDT resets

    }
}

void display()
{
    while(1)
    {

        // UBaseType_t high_water_mark = uxTaskGetStackHighWaterMark(NULL);
        // printf("High water mark: %u bytes\n", high_water_mark);
        
        xSemaphoreTake(bin_sem, portMAX_DELAY);

        ssd1306_clear_screen(&dev, false);

        // Normalize values into bins
        double bins[NUM_BINS] = {0};
        int bin_size = (NO_OF_SAMPLES / 2) / NUM_BINS;  // Number of samples per bin

        for (int i = 0; i < NO_OF_SAMPLES / 2; i++)
        {
            int bin_index = i / bin_size;
            if (bin_index < NUM_BINS)
            {
                bins[bin_index] += cabs(fft_out[i]);
                // printf("bin: %d, val: %f\n", bin_index, cabs(fft_out[i]));
            }
        }

        double max_bin_value = 1;
        for (int i = 0; i < NUM_BINS; i++) {
            if (bins[i] > max_bin_value) {
                max_bin_value = bins[i];
            }
        }

        // Scale and draw bins
        int x_pos = 2;
        for (int i = 0; i < NUM_BINS; i++)
        {
            int height = (int)((bins[i] / max_bin_value) * (SCREEN_HEIGHT - 37)); // Scale height
            if (height < 1) height = 1;
            _ssd1306_line(&dev, x_pos, 32, x_pos, 32+height, false);  // Draw bar
            x_pos += (SCREEN_WIDTH/NUM_BINS);
        }

        ssd1306_show_buffer(&dev);

        xSemaphoreGive(bin_sem);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


void app_main(void)
{
    // initialize display
    i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
    ssd1306_init(&dev, SCREEN_WIDTH, SCREEN_HEIGHT);

    vSemaphoreCreateBinary(bin_sem);

    ssd1306_clear_screen(&dev, false);

    // tasks
    if (xTaskCreate(&mic_in, "mic_in", 16384, NULL, 2, NULL) != pdPASS)
    {
        ESP_LOGE("TASK", "Failed to create mic_in task! Not enough heap memory.");
    }
    if (xTaskCreate(&display, "display", 8192, NULL, 2, NULL) != pdPASS)
    {
        ESP_LOGE("TASK", "Failed to create display task! Not enough heap memory.");
    }

    // vTaskStartScheduler();
}