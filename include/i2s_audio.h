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
	
	// Reconfigure I2S with new sample rate (for bit-perfect playback)
	bool reconfigure(uint32_t sampleRate, uint8_t bitsPerSample, uint8_t channels);
	
	// Set volume (0-100)
	void setVolume(uint8_t volume);
	
	// Get current volume
	uint8_t getVolume() const { return _volume; }
	
	// Write data to I2S (returns bytes written)
	size_t write(const uint8_t* data, size_t len);
	
	// Write silence (for underrun)
	void writeSilence(size_t bytes);
	
	// Check if I2S is ready
	bool isReady() const { return _ready; }
	
	// Get configuration
	uint32_t getSampleRate() const { return _sampleRate; }
	uint8_t getBitsPerSample() const { return _bitsPerSample; }
	uint8_t getChannels() const { return _channels; }
	
private:
	bool _ready;
	uint32_t _sampleRate;
	uint8_t _bitsPerSample;
	uint8_t _channels;
	uint8_t _volume;  // 0-100
	i2s_port_t _port;
};

#endif // I2S_AUDIO_H
