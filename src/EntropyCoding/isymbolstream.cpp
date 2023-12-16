#include "isymbolstream.h"

isymbolstream::isymbolstream(const content_t &content, size_t size) 
    : symbolstream_base(content, size), currentIndex(0) {};

size_t isymbolstream::get() {
    size_t ret_value =
        (unsigned char)(((content[this->currentIndex >> 1]) &
                        ((-(int)((~(this->currentIndex)) & 0x01)) ^ 0x0f)) >>
                    ((-(int)((~(this->currentIndex)) & 0x01)) & 4));
    ++(this->currentIndex);
    return ret_value;
};

isymbolstream::operator bool() { return currentIndex != size; }

bool isymbolstream::operator!() {
    // currentIndex == size;
    return !((bool)*this);
};