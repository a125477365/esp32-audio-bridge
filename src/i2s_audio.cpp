#include "i2s_audio.h"

I2SAudio::I2SAudio()
 : _ready(false)
 , _sampleRate(DEFAULT_SAMPLE_RATE)
 , _bitsPerSample(DEFAULT_BITS_PER_SAMPLE)
 , _port(I2S_NUM_0) {
}

I2SAudio::~I2SAudio() {
 end();
}

bool I2SAudio::begin(uint32_t sampleRate, uint8_t bitsPerSample, size_t bufferSize) {
 if (_ready) {
 end();
 }
 
 _sampleRate = sampleRate;
 _bitsPerSample = bitsPerSample;
 
 // I2S configuration for Hi-Fi output
 i2s_config_t i2s_config = {
 .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
 .sample_rate = sampleRate,
 .bits_per_sample = (i2s_bits_per_sample_t)bitsPerSample,
 .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
 .communication_format = I2S_COMM_FORMAT_STAND_I2S,
 .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
 .dma_buf_count = 8,
 .dma_buf_len = 1024,
 .use_apll = true, // CRITICAL: Enable Audio PLL for low jitter
 .tx_desc_auto_clear = true,
 .fixed_mclk = 0
 };
 
 // Pin configuration - ESP32-S3 requires mck_io_num to be set
 i2s_pin_config_t pin_config = {
 .mck_io_num = -1, // MCLK not used, set to -1 (I2S_PIN_NO_CHANGE)
 .bck_io_num = I2S_BCLK_PIN,
 .ws_io_num = I2S_WS_PIN,
 .data_out_num = I2S_DATA_PIN,
 .data_in_num = -1
 };
 
 // Install I2S driver
 esp_err_t err = i2s_driver_install(_port, &i2s_config, 0, NULL);
 if (err != ESP_OK) {
 DEBUG_SERIAL.printf("[I2S] Driver install failed: %d\n", err);
 return false;
 }
 
 // Set pins
 err = i2s_set_pin(_port, &pin_config);
 if (err != ESP_OK) {
 DEBUG_SERIAL.printf("[I2S] Set pin failed: %d\n", err);
 i2s_driver_uninstall(_port);
 return false;
 }
 
 // Clear DMA buffers
 i2s_zero_dma_buffer(_port);
 
 // Verify APLL is being used
 if (!i2s_config.use_apll) {
 DEBUG_SERIAL.println("[I2S] WARNING: APLL not enabled!");
 } else {
 DEBUG_SERIAL.println("[I2S] APLL enabled - low jitter mode");
 }
 
 _ready = true;
 DEBUG_SERIAL.printf("[I2S] Started: %lu Hz, %d bit, APLL=%s\n", sampleRate, bitsPerSample, i2s_config.use_apll ? "ON" : "OFF");
 return true;
}

void I2SAudio::end() {
 if (!_ready) return;
 i2s_driver_uninstall(_port);
 _ready = false;
 DEBUG_SERIAL.println("[I2S] Stopped");
}

size_t I2SAudio::write(const uint8_t* data, size_t len) {
 if (!_ready || data == nullptr || len == 0) {
 return 0;
 }
 size_t bytes_written = 0;
 esp_err_t err = i2s_write(_port, data, len, &bytes_written, pdMS_TO_TICKS(100));
 if (err != ESP_OK) {
 DEBUG_SERIAL.printf("[I2S] Write error: %d\n", err);
 }
 return bytes_written;
}

void I2SAudio::writeSilence(size_t bytes) {
 if (!_ready || bytes == 0) return;
 
 // Allocate and fill with zeros
 uint8_t* silence = (uint8_t*)malloc(bytes);
 if (silence == nullptr) return;
 memset(silence, 0, bytes);
 
 size_t bytes_written = 0;
 i2s_write(_port, silence, bytes, &bytes_written, pdMS_TO_TICKS(100));
 free(silence);
}
