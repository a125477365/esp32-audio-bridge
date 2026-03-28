#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class RingBuffer {
public:
    RingBuffer();
    ~RingBuffer();
    
    bool init(size_t size);
    void deinit();
    
    // Push data into buffer (from UDP)
    size_t push(const uint8_t* data, size_t len);
    
    // Pop data from buffer (to I2S)
    size_t pop(uint8_t* data, size_t len);
    
    // Get buffer status
    float level() const;        // 0.0 ~ 1.0
    size_t available() const;   // bytes available to read
    size_t freeSpace() const;   // bytes can be written
    bool isEmpty() const;
    bool isFull() const;
    
    // Clear buffer
    void clear();
    
    // Get total size
    size_t size() const { return _size; }

private:
    uint8_t* _buffer;
    size_t _size;
    volatile size_t _head;  // write position
    volatile size_t _tail;  // read position
    volatile size_t _count; // current data count
    
    SemaphoreHandle_t _mutex;
};

#endif // RING_BUFFER_H
