#include "ir_conv_fft.h"

#include "arm_const_structs.h"

static void InitRfftFast(arm_rfft_fast_instance_f32 *fft, size_t fft_size)
{
    switch(fft_size)
    {
        case 128:
            fft->Sint         = arm_cfft_sR_f32_len64;
            fft->fftLenRFFT   = 128;
            fft->pTwiddleRFFT = (float32_t *)twiddleCoef_rfft_128;
            break;
        case 256:
            fft->Sint         = arm_cfft_sR_f32_len128;
            fft->fftLenRFFT   = 256;
            fft->pTwiddleRFFT = (float32_t *)twiddleCoef_rfft_256;
            break;
        default:
        case 512:
            fft->Sint         = arm_cfft_sR_f32_len256;
            fft->fftLenRFFT   = 512;
            fft->pTwiddleRFFT = (float32_t *)twiddleCoef_rfft_512;
            break;
        case 1024:
            fft->Sint         = arm_cfft_sR_f32_len512;
            fft->fftLenRFFT   = 1024;
            fft->pTwiddleRFFT = (float32_t *)twiddleCoef_rfft_1024;
            break;
        case 2048:
            fft->Sint         = arm_cfft_sR_f32_len1024;
            fft->fftLenRFFT   = 2048;
            fft->pTwiddleRFFT = (float32_t *)twiddleCoef_rfft_2048;
            break;
        case 4096:
            fft->Sint         = arm_cfft_sR_f32_len2048;
            fft->fftLenRFFT   = 4096;
            fft->pTwiddleRFFT = (float32_t *)twiddleCoef_rfft_4096;
            break;
    }
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

void FftConvolver::Init()
{
    InitRfftFast(&fft_, kFftConvolverFftSize);

    for(size_t i = 0; i < kFftConvolverFftSize; i++)
        fft_input_[i] = 0.0f;
    for(size_t i = 0; i < kFftConvolverTailSize; i++)
        fft_input_[i] = ir[kFftConvolverHeadSize + i];

    arm_rfft_fast_f32(&fft_, fft_input_, ir_fft_, 0);
}

float FftConvolver::Process(float input)
{
    head_delay_[head_write_index_] = input;

    float  head_output = 0.0f;
    size_t read        = head_write_index_;
    for(size_t i = 0; i < kFftConvolverHeadSize; i++)
    {
        head_output += ir[i] * head_delay_[read];
        read = read == 0 ? kFftConvolverHeadSize - 1 : read - 1;
    }

    head_write_index_++;
    if(head_write_index_ >= kFftConvolverHeadSize)
        head_write_index_ = 0;

    const float tail_output = output_block_[output_index_++];

    input_block_[input_count_++] = input;
    if(input_count_ >= kFftConvolverBlockSize)
    {
        ProcessBlock();
        input_count_  = 0;
        output_index_ = 0;
    }

    return head_output + tail_output;
}

void FftConvolver::ProcessBlock()
{
    for(size_t i = 0; i < kFftConvolverFftSize - kFftConvolverBlockSize; i++)
        history_[i] = history_[i + kFftConvolverBlockSize];

    for(size_t i = 0; i < kFftConvolverBlockSize; i++)
        history_[kFftConvolverFftSize - kFftConvolverBlockSize + i] = input_block_[i];

    for(size_t i = 0; i < kFftConvolverFftSize; i++)
    {
        fft_input_[i] = history_[i];
        product_[i]   = 0.0f;
    }

    arm_rfft_fast_f32(&fft_, fft_input_, spectrum_, 0);
    PackedSpectrumMultiplyAccumulate(product_, spectrum_, ir_fft_, kFftConvolverFftSize);
    arm_rfft_fast_f32(&fft_, product_, ifft_output_, 1);

    for(size_t i = 0; i < kFftConvolverBlockSize; i++)
        output_block_[i] = ifft_output_[kFftConvolverFftSize - kFftConvolverBlockSize + i];
}
