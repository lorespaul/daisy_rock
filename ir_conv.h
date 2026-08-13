#pragma once

#include <cstddef>

#if __has_include("generated_ir.h")
#include "generated_ir.h"
#else
static constexpr size_t kIrSize     = 128;
static const float      ir[kIrSize] = {1.0f};
#endif

class DirectConvolver
{
  public:
    static constexpr size_t kAudioBlockSize = 4;

    void  Init() {}
    float Process(float input);

  private:
    float  delay_[kIrSize] = {};
    size_t write_index_    = 0;
};
