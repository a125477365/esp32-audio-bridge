#ifndef VBAN_H
#define VBAN_H

#include <Arduino.h>

// VBAN Protocol Constants
#define VBAN_PROTOCOL_HEADER_SIZE 28
#define VBAN_HEADER_BYTES_SIZE 4
#define VBAN_PROTOCOL_AUDIO 0x00

// VBAN Sample Rates (index -> Hz)
const uint32_t VBAN_SAMPLE_RATES[] = {
    6000, 12000, 24000, 48000, 96000, 192000, 384000,
    8000, 16000, 32000, 64000, 128000, 256000,
    44100, 88200, 176400, 352800
};
#define VBAN_SR_MAXIDX 16

// VBAN Bit Depths (index -> bits)
const uint8_t VBAN_BIT_DEPTHS[] = {8, 16, 24, 32};
#define VBAN_BIT_MAXIDX 3

// VBAN Header Structure
struct VBANHeader {
    char vban[4];           // 'VBAN'
    uint8_t protocol;       // Protocol type (0 = audio)
    uint8_t sampleRate;     // Sample rate index
    uint8_t samplesPerFrame; // Samples per frame - 1
    uint8_t channels;       // Channels - 1
    uint8_t format;         // Format (bit depth, etc)
    uint8_t codec;          // Codec (0 = PCM)
    uint8_t reserved[6];    // Reserved bytes
    uint32_t frameNumber;   // Frame counter
};

class VBANParser {
public:
    VBANParser();
    
    // Parse VBAN packet, returns true if valid
    bool parse(const uint8_t* data, size_t len);
    
    // Get extracted PCM data (skip header)
    const uint8_t* getPCMData() const;
    size_t getPCMLength() const;
    
    // Get audio parameters
    uint32_t getSampleRate() const;
    uint8_t getBitsPerSample() const;
    uint8_t getChannels() const;
    
    // Check if this is a VBAN packet
    static bool isVBANPacket(const uint8_t* data, size_t len);
    
private:
    VBANHeader _header;
    const uint8_t* _pcmData;
    size_t _pcmLength;
    bool _valid;
    
    uint32_t decodeSampleRate(uint8_t index);
    uint8_t decodeBitDepth(uint8_t format);
};

#endif // VBAN_H
