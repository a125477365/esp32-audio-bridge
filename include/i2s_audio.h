#ifndef I2S_AUDIO_H
#define I2S_AUDIO_H

#include <Arduino.h>
#include <driver/i2s.h>
#include "config.h"
#include "ring_buffer.h"

class I2SAudio {
public:
    I2SAudio();
    ~I2SAudio();
    
    bool begin(uint32_t sampleRate, uint8_t bitsPerSample, size_t bufferSize);
    void end();
    
    // Write data to I2S (returns bytes written)
    size_t write(const uint8_t* data, size_t len);
    
    // Write silence (for underrun)
    void writeSilence(size_t bytes);
    
    // Check if I2S is ready
    bool isReady() const { return _ready; }
    
    // Get configuration
    uint32_t getSampleRate() const { return _sampleRate; }
    uint8_t getBitsPerSample() const { return _bitsPerSample; }

private:
    bool _ready;
    uint32_t _sampleRate;
    uint8_t _bitsPerSample;
    i2s_port_t _port;
};

#endif // I2S_AUDIO_H
