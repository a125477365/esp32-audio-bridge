#include "ring_buffer.h"
#include <stdlib.h>

RingBuffer::RingBuffer() 
    : _buffer(nullptr)
    , _size(0)
    , _head(0)
    , _tail(0)
    , _count(0)
    , _mutex(nullptr) {
}

RingBuffer::~RingBuffer() {
    deinit();
}

bool RingBuffer::init(size_t size) {
    if (_buffer != nullptr) {
        deinit();
    }
    
    // Allocate buffer
    _buffer = (uint8_t*)malloc(size);
    if (_buffer == nullptr) {
        return false;
    }
    
    _size = size;
    _head = 0;
    _tail = 0;
    _count = 0;
    
    // Create mutex for thread safety
    _mutex = xSemaphoreCreateMutex();
    if (_mutex == nullptr) {
        free(_buffer);
        _buffer = nullptr;
        return false;
    }
    
    memset(_buffer, 0, size);
    return true;
}

void RingBuffer::deinit() {
    if (_mutex != nullptr) {
        vSemaphoreDelete(_mutex);
        _mutex = nullptr;
    }
    
    if (_buffer != nullptr) {
        free(_buffer);
        _buffer = nullptr;
    }
    
    _size = 0;
    _head = 0;
    _tail = 0;
    _count = 0;
}

size_t RingBuffer::push(const uint8_t* data, size_t len) {
    if (_buffer == nullptr || data == nullptr || len == 0) {
        return 0;
    }
    
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return 0;
    }
    
    size_t free_space = _size - _count;
    size_t to_write = (len < free_space) ? len : free_space;
    
    // Write in two parts if wrap around
    size_t first_part = _size - _head;
    if (first_part >= to_write) {
        // No wrap
        memcpy(_buffer + _head, data, to_write);
        _head = (_head + to_write) % _size;
    } else {
        // Wrap around
        memcpy(_buffer + _head, data, first_part);
        memcpy(_buffer, data + first_part, to_write - first_part);
        _head = to_write - first_part;
    }
    
    _count += to_write;
    
    xSemaphoreGive(_mutex);
    return to_write;
}

size_t RingBuffer::pop(uint8_t* data, size_t len) {
    if (_buffer == nullptr || data == nullptr || len == 0) {
        return 0;
    }
    
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return 0;
    }
    
    size_t to_read = (len < _count) ? len : _count;
    
    if (to_read == 0) {
        xSemaphoreGive(_mutex);
        return 0;
    }
    
    // Read in two parts if wrap around
    size_t first_part = _size - _tail;
    if (first_part >= to_read) {
        // No wrap
        memcpy(data, _buffer + _tail, to_read);
        _tail = (_tail + to_read) % _size;
    } else {
        // Wrap around
        memcpy(data, _buffer + _tail, first_part);
        memcpy(data + first_part, _buffer, to_read - first_part);
        _tail = to_read - first_part;
    }
    
    _count -= to_read;
    
    xSemaphoreGive(_mutex);
    return to_read;
}

float RingBuffer::level() const {
    if (_size == 0) return 0.0f;
    return (float)_count / (float)_size;
}

size_t RingBuffer::available() const {
    return _count;
}

size_t RingBuffer::freeSpace() const {
    return _size - _count;
}

bool RingBuffer::isEmpty() const {
    return _count == 0;
}

bool RingBuffer::isFull() const {
    return _count == _size;
}

void RingBuffer::clear() {
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        _head = 0;
        _tail = 0;
        _count = 0;
        xSemaphoreGive(_mutex);
    }
}
