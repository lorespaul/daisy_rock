#pragma once

#include "ir.h"

class DirectConvolver
{
  public:
    static constexpr size_t kAudioBlockSize = 4;

    void  Init();
    void  SetIr(size_t index);
    size_t NextIr() { SetIr(ir_index_ + 1); return ir_index_; }
    float Process(float input);

  private:
    float  delay_[kIrSize] = {};
    size_t write_index_    = 0;
    size_t ir_index_       = 0;
};
