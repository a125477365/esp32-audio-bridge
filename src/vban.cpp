#include "vban.h"

VBANParser::VBANParser() 
    : _pcmData(nullptr)
    , _pcmLength(0)
    , _valid(false) {
    memset(&_header, 0, sizeof(_header));
}

bool VBANParser::parse(const uint8_t* data, size_t len) {
    _valid = false;
    _pcmData = nullptr;
    _pcmLength = 0;
    
    // Minimum size: header + some PCM data
    if (data == nullptr || len < VBAN_PROTOCOL_HEADER_SIZE + 1) {
        return false;
    }
    
    // Check VBAN signature
    if (!isVBANPacket(data, len)) {
        return false;
    }
    
    // Copy header
    memcpy(&_header, data, VBAN_PROTOCOL_HEADER_SIZE);
    
    // Check protocol type (must be audio = 0)
    if (_header.protocol != VBAN_PROTOCOL_AUDIO) {
        DEBUG_SERIAL.printf("[VBAN] Unsupported protocol: %d\n", _header.protocol);
        return false;
    }
    
    // Check codec (must be PCM = 0)
    if (_header.codec != 0) {
        DEBUG_SERIAL.printf("[VBAN] Unsupported codec: %d\n", _header.codec);
        return false;
    }
    
    // Validate sample rate index
    if (_header.sampleRate > VBAN_SR_MAXIDX) {
        DEBUG_SERIAL.printf("[VBAN] Invalid sample rate index: %d\n", _header.sampleRate);
        return false;
    }
    
    // Extract PCM data (skip header)
    _pcmData = data + VBAN_PROTOCOL_HEADER_SIZE;
    _pcmLength = len - VBAN_PROTOCOL_HEADER_SIZE;
    
    _valid = true;
    return true;
}

const uint8_t* VBANParser::getPCMData() const {
    return _pcmData;
}

size_t VBANParser::getPCMLength() const {
    return _pcmLength;
}

uint32_t VBANParser::getSampleRate() const {
    return decodeSampleRate(_header.sampleRate);
}

uint8_t VBANParser::getBitsPerSample() const {
    return decodeBitDepth(_header.format);
}

uint8_t VBANParser::getChannels() const {
    return _header.channels + 1;  // Channels stored as N-1
}

bool VBANParser::isVBANPacket(const uint8_t* data, size_t len) {
    if (data == nullptr || len < 4) {
        return false;
    }
    
    // Check for 'VBAN' signature (little-endian: V=0x56, B=0x42, A=0x41, N=0x4E)
    return (data[0] == 'V' && data[1] == 'B' && data[2] == 'A' && data[3] == 'N');
}

uint32_t VBANParser::decodeSampleRate(uint8_t index) {
    if (index <= VBAN_SR_MAXIDX) {
        return VBAN_SAMPLE_RATES[index];
    }
    return 44100;  // Default fallback
}

uint8_t VBANParser::decodeBitDepth(uint8_t format) {
    // Format: lower 2 bits = bit depth index
    uint8_t bitIndex = format & 0x03;
    return VBAN_BIT_DEPTHS[bitIndex];
}
