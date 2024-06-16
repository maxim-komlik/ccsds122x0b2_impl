#pragma once

#include "symbolstream_base.h"

// TODO: fill interface
class osymbolstream : virtual public symbolstream_base {
public:
  void put(size_t item);
};
