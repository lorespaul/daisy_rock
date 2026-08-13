#pragma once

#include "arm_math.h"
#include <cstddef>

#if __has_include("generated_ir.h")
#include "generated_ir.h"
#else
static constexpr size_t kIrSize     = 2048;
static const float      ir[kIrSize] = {1.0f};
#endif

static constexpr size_t kPartitionedConvolverPartitionSize = 64;

static_assert(kIrSize >= kPartitionedConvolverPartitionSize, "IR length must be at least kPartitionedConvolverPartitionSize samples");

static constexpr size_t kPartitionedConvolverHeadSize      = kPartitionedConvolverPartitionSize;
static constexpr size_t kPartitionedConvolverTailSize      = kIrSize - kPartitionedConvolverHeadSize;
static constexpr size_t kPartitionedConvolverFftSize       = 2 * kPartitionedConvolverPartitionSize;
static constexpr size_t kPartitionedConvolverNumPartitions = kPartitionedConvolverTailSize / kPartitionedConvolverPartitionSize;

static_assert(kPartitionedConvolverTailSize % kPartitionedConvolverPartitionSize == 0, "IR tail length must be a multiple of kPartitionedConvolverPartitionSize samples");

class DirectHeadPartitionedFftConvolver
{
  public:
    static constexpr size_t kAudioBlockSize = kPartitionedConvolverPartitionSize;

    void  Init();
    float Process(float input);

  private:
    void ProcessBlock();

    float                      input_block_[kPartitionedConvolverPartitionSize]                             = {};
    float                      output_block_[kPartitionedConvolverPartitionSize]                            = {};
    float                      head_delay_[kPartitionedConvolverHeadSize]                                   = {};
    float                      overlap_[kPartitionedConvolverPartitionSize]                                 = {};
    float                      ir_fft_[kPartitionedConvolverNumPartitions][kPartitionedConvolverFftSize]    = {};
    float                      input_fft_[kPartitionedConvolverNumPartitions][kPartitionedConvolverFftSize] = {};
    float                      acc_fft_[kPartitionedConvolverFftSize]                                       = {};
    float                      fft_input_[kPartitionedConvolverFftSize]                                     = {};
    float                      ifft_output_[kPartitionedConvolverFftSize]                                   = {};
    arm_rfft_fast_instance_f32 fft_                                                                         = {};
    size_t                     input_count_                                                                 = 0;
    size_t                     output_index_                                                                = 0;
    size_t                     write_partition_                                                             = 0;
    size_t                     head_write_index_                                                            = 0;
};
