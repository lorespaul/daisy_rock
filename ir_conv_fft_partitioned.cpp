#include "ir_conv_fft_partitioned.h"

#include "arm_const_structs.h"

static void InitRfftFast(arm_rfft_fast_instance_f32 *fft)
{
    fft->Sint         = arm_cfft_sR_f32_len64;
    fft->fftLenRFFT   = 128;
    fft->pTwiddleRFFT = (float32_t *)twiddleCoef_rfft_128;
}

static void PackedSpectrumMultiplyAccumulate(float *dst, const float *a, const float *b, size_t fft_size)
{
    dst[0] += a[0] * b[0];
    dst[1] += a[1] * b[1];

    for(size_t bin = 1; bin < fft_size / 2; bin++)
    {
        const size_t re = 2 * bin;
        const size_t im = re + 1;
        const float  ar = a[re];
        const float  ai = a[im];
        const float  br = b[re];
        const float  bi = b[im];

        dst[re] += ar * br - ai * bi;
        dst[im] += ar * bi + ai * br;
    }
}

void DirectHeadPartitionedFftConvolver::Init()
{
    InitRfftFast(&fft_);

    for(size_t partition = 0; partition < kPartitionedConvolverNumPartitions; partition++)
    {
        float *dst = &ir_fft_[partition][0];

        for(size_t i = 0; i < kPartitionedConvolverFftSize; i++)
            fft_input_[i] = 0.0f;
        for(size_t i = 0; i < kPartitionedConvolverPartitionSize; i++)
            fft_input_[i] = ir[kPartitionedConvolverHeadSize + partition * kPartitionedConvolverPartitionSize + i];

        arm_rfft_fast_f32(&fft_, fft_input_, dst, 0);
    }
}

float DirectHeadPartitionedFftConvolver::Process(float input)
{
    head_delay_[head_write_index_] = input;

    float  head_output = 0.0f;
    size_t read        = head_write_index_;
    for(size_t i = 0; i < kPartitionedConvolverHeadSize; i++)
    {
        head_output += ir[i] * head_delay_[read];
        read = read == 0 ? kPartitionedConvolverHeadSize - 1 : read - 1;
    }

    head_write_index_++;
    if(head_write_index_ >= kPartitionedConvolverHeadSize)
        head_write_index_ = 0;

    const float tail_output = output_block_[output_index_++];

    input_block_[input_count_++] = input;
    if(input_count_ >= kPartitionedConvolverPartitionSize)
    {
        ProcessBlock();
        input_count_  = 0;
        output_index_ = 0;
    }

    return head_output + tail_output;
}

void DirectHeadPartitionedFftConvolver::ProcessBlock()
{
    float *x = &input_fft_[write_partition_][0];

    for(size_t i = 0; i < kPartitionedConvolverFftSize; i++)
    {
        fft_input_[i] = i < kPartitionedConvolverPartitionSize ? input_block_[i] : 0.0f;
        acc_fft_[i]   = 0.0f;
    }

    arm_rfft_fast_f32(&fft_, fft_input_, x, 0);

    for(size_t partition = 0; partition < kPartitionedConvolverNumPartitions; partition++)
    {
        const size_t x_index = (write_partition_ + kPartitionedConvolverNumPartitions - partition) % kPartitionedConvolverNumPartitions;
        PackedSpectrumMultiplyAccumulate(acc_fft_, input_fft_[x_index], ir_fft_[partition], kPartitionedConvolverFftSize);
    }

    arm_rfft_fast_f32(&fft_, acc_fft_, ifft_output_, 1);

    for(size_t i = 0; i < kPartitionedConvolverPartitionSize; i++)
    {
        output_block_[i] = ifft_output_[i] + overlap_[i];
        overlap_[i]      = ifft_output_[i + kPartitionedConvolverPartitionSize];
    }

    write_partition_++;
    if(write_partition_ >= kPartitionedConvolverNumPartitions)
        write_partition_ = 0;
}
