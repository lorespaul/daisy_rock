#pragma once

#include "arm_math.h"
#include <cstddef>

#if __has_include("generated_ir.h")
#include "generated_ir.h"
#else
static constexpr size_t kIrSize     = 512;
static const float      ir[kIrSize] = {1.0f};
#endif

static constexpr size_t kFftConvolverBlockSize = 64;

static_assert(kIrSize >= kFftConvolverBlockSize, "IR length must be at least kFftConvolverBlockSize samples");

static constexpr size_t kFftConvolverHeadSize = kFftConvolverBlockSize;
static constexpr size_t kFftConvolverTailSize = kIrSize - kFftConvolverHeadSize;
static constexpr size_t kFftConvolverFftSize  = kIrSize;

class FftConvolver
{
  public:
    static constexpr size_t kAudioBlockSize = kFftConvolverBlockSize;

    void  Init();
    float Process(float input);

  private:
    void ProcessBlock();

    arm_rfft_fast_instance_f32 fft_                                  = {};
    float                      history_[kFftConvolverFftSize]        = {};
    float                      input_block_[kFftConvolverBlockSize]  = {};
    float                      output_block_[kFftConvolverBlockSize] = {};
    float                      head_delay_[kFftConvolverHeadSize]    = {};
    float                      fft_input_[kFftConvolverFftSize]      = {};
    float                      ifft_output_[kFftConvolverFftSize]    = {};
    float                      ir_fft_[kFftConvolverFftSize]         = {};
    float                      spectrum_[kFftConvolverFftSize]       = {};
    float                      product_[kFftConvolverFftSize]        = {};
    size_t                     input_count_                          = 0;
    size_t                     output_index_                         = 0;
    size_t                     head_write_index_                     = 0;
};
