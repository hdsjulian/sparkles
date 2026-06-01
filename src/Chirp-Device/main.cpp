#include <Arduino.h>
#include "driver/i2s_std.h"
#include <math.h>

#define BCLK_PIN    GPIO_NUM_4
#define WS_PIN      GPIO_NUM_5
#define DOUT_PIN    GPIO_NUM_17

#define SAMPLE_RATE 44100
#define AMPLITUDE   28000
#define FREQ_HZ     1000
#define BUF_FRAMES  256

static i2s_chan_handle_t s_tx;
static int16_t s_buf[BUF_FRAMES * 2]; // stereo

void setup() {
    Serial.begin(115200);

    i2s_chan_config_t ch = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ch.auto_clear = true;
    ESP_ERROR_CHECK(i2s_new_channel(&ch, &s_tx, nullptr));

    i2s_std_config_t cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = BCLK_PIN,
            .ws   = WS_PIN,
            .dout = DOUT_PIN,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { false, false, false },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_tx, &cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_tx));

    for (int i = 0; i < BUF_FRAMES; i++) {
        int16_t v = (int16_t)(AMPLITUDE * sinf(2.0f * M_PI * FREQ_HZ * i / SAMPLE_RATE));
        s_buf[i * 2]     = v;
        s_buf[i * 2 + 1] = v;
    }

    Serial.println("I2S ready — playing 1kHz sine");
}

void loop() {
    size_t written = 0;
    i2s_channel_write(s_tx, s_buf, sizeof(s_buf), &written, portMAX_DELAY);
}
