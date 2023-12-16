#pragma once

#include "symbolstream_base.h"

// TODO: fill interface
class isymbolstream : virtual public symbolstream_base {
  size_t currentIndex = 0;

protected:
  isymbolstream() = default;

public:
  isymbolstream(const content_t &content, size_t size);

  size_t get();

  operator bool();
  bool operator!();
};
